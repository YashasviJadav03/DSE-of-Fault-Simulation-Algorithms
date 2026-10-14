// ============================================================
//  main.cpp  —  Unified Fault Simulation & DSE CLI Driver
// ============================================================

#include "circuit.h"
#include "parser.h"
#include "simulator.h"
#include "testvector.h"
#include "fault_collapsing.h"
#include "compaction.h"
#include "serial_sim.h"
#include "parallel_pattern_sim.h"
#include "concurrent_sim.h"
#include "dse_engine.h"

#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <algorithm>
#include <filesystem>

namespace fs = std::filesystem;

void printUsage(const char* prog) {
    std::cout << "\n============================================================\n";
    std::cout << "  FAULT SIMULATION & DESIGN SPACE EXPLORATION (DSE) TOOL\n";
    std::cout << "============================================================\n";
    std::cout << "Usage:\n";
    std::cout << "  " << prog << " --circuit <path> [options]\n";
    std::cout << "  " << prog << " --all-benchmarks [options]\n\n";
    std::cout << "Options:\n";
    std::cout << "  -c, --circuit <path>    Circuit netlist file (e.g. benchmarks/c17.txt)\n";
    std::cout << "  -a, --algo <name>       Algorithm: 'all' (default), 'sfs', 'ppfs', 'cfs'\n";
    std::cout << "  -n, --vectors <N>       Number of test vectors (default: smart/exhaustive)\n";
    std::cout << "      --exhaustive        Force exhaustive test vector generation (N <= 20)\n";
    std::cout << "      --seed <val>        RNG seed for random vector generation (default: 42)\n";
    std::cout << "      --compact           Run and print detailed Reverse Compaction report\n";
    std::cout << "      --dse               Run vector budget sweep and scaling analysis\n";
    std::cout << "      --export            Export Markdown and CSV reports into reports/\n";
    std::cout << "      --all-benchmarks    Run DSE across all 5 benchmark circuits automatically\n";
    std::cout << "  -h, --help              Show this help message\n\n";
    std::cout << "Examples:\n";
    std::cout << "  " << prog << " --circuit benchmarks/c17.txt --algo all --compact --dse\n";
    std::cout << "  " << prog << " --circuit benchmarks/6_bit_ALU.txt --vectors 500 --compact\n";
    std::cout << "  " << prog << " --all-benchmarks --export\n\n";
}

bool processCircuit(const std::string& circuitPath,
                    const std::string& algoChoice,
                    int requestedVecCount,
                    bool forceExhaustive,
                    uint32_t seed,
                    bool showCompaction,
                    bool runSweep,
                    bool exportReports)
{
    Circuit ckt;
    std::string actualPath = findFile(circuitPath);
    if (!parseNetlist(actualPath, ckt)) {
        std::cerr << "[-] Error: Failed to parse circuit netlist: " << actualPath << "\n";
        return false;
    }

    std::vector<int> topoOrder = topologicalSort(ckt);
    if (topoOrder.empty() && ckt.totalNodes() > 0) {
        std::cerr << "[-] Error: Topological sort failed or circuit contains cycles.\n";
        return false;
    }

    std::cout << "\n============================================================\n";
    std::cout << "  Loaded Circuit: " << ckt.name << "  (" << actualPath << ")\n";
    std::cout << "  Inputs: " << ckt.inputIds.size()
              << " | Outputs: " << ckt.outputIds.size()
              << " | Gates: " << ckt.gateIds.size()
              << " | Branches: " << ckt.branchIds.size()
              << " | Total Nodes: " << ckt.totalNodes() << "\n";
    std::cout << "============================================================\n";

    // Test vector generation
    std::vector<TestVector> vectors;
    int numInputs = (int)ckt.inputIds.size();

    if (forceExhaustive) {
        if (numInputs > 20) {
            std::cerr << "[-] Error: Too many inputs (" << numInputs << ") for exhaustive generation.\n";
            return false;
        }
        vectors = genExhaustive(ckt);
    } else if (requestedVecCount > 0) {
        if (requestedVecCount <= (1 << numInputs) && numInputs <= 12) {
            auto full = genExhaustive(ckt);
            vectors.assign(full.begin(), full.begin() + std::min((size_t)requestedVecCount, full.size()));
        } else {
            vectors = genRandom(ckt, requestedVecCount, seed);
        }
    } else {
        // Smart default
        vectors = genForFaultSim(ckt);
    }

    std::cout << "[*] Test vectors generated: " << vectors.size() << "\n";

    // 1. Fault Collapsing Report
    FaultCollapser fc(ckt);
    fc.collapse();
    fc.printReport();
    const auto& collapsedList = fc.collapsedFaults();

    // 2. Algorithm Execution
    if (algoChoice == "all") {
        DSEEngine dse(ckt, topoOrder);
        DSEReport rep = dse.runComparison(vectors, runSweep);
        DSEEngine::printConsoleReport(rep);

        if (exportReports) {
            fs::create_directories("reports");
            std::string mdPath = "reports/" + ckt.name + "_dse_report.md";
            std::string csvPath = "reports/" + ckt.name + "_dse_metrics.csv";
            DSEEngine::exportMarkdown(rep, mdPath);
            DSEEngine::exportCSV(rep, csvPath);
            std::cout << "[+] Exported Markdown report to: " << mdPath << "\n";
            std::cout << "[+] Exported CSV metrics to    : " << csvPath << "\n";
        }
    } else if (algoChoice == "sfs") {
        std::cout << "\n[*] Running Serial Fault Simulation (SFS)...\n";
        SerialFaultSim sfs(ckt, topoOrder, collapsedList);
        auto res = sfs.run(vectors, true);
        std::cout << "  - Computational Time: " << res.computationTimeMs << " ms\n";
        std::cout << "  - Fault Coverage    : " << res.faultCoverage << "% ("
                  << res.detectedFaults << " / " << res.totalFaults << ")\n";
        std::cout << "  - Undetected Faults : " << res.undetectedFaults << "\n";

        if (showCompaction) {
            auto cr = reverseCompact(vectors, res);
            printCompactionReport(cr, ckt);
        }
    } else if (algoChoice == "ppfs") {
        std::cout << "\n[*] Running Parallel Pattern Fault Simulation (PPFS)...\n";
        ParallelPatternFaultSim ppfs(ckt, topoOrder);
        auto res = ppfs.run(vectors, true);
        std::cout << "  - Computational Time: " << res.computationTimeMs << " ms\n";
        std::cout << "  - Fault Coverage    : " << res.faultCoverage << "% ("
                  << res.detectedFaults << " / " << res.totalFaults << ")\n";
        std::cout << "  - Undetected Faults : " << res.undetectedFaults << "\n";

        if (showCompaction) {
            auto cr = reverseCompact(vectors, res);
            printCompactionReport(cr, ckt);
        }
    } else if (algoChoice == "cfs") {
        std::cout << "\n[*] Running Concurrent Fault Simulation (CFS)...\n";
        ConcurrentFaultSim cfs(ckt, topoOrder, collapsedList);
        auto res = cfs.run(vectors, true);
        std::cout << "  - Computational Time: " << res.computationTimeMs << " ms\n";
        std::cout << "  - Fault Coverage    : " << res.faultCoverage << "% ("
                  << res.detectedFaults << " / " << res.totalFaults << ")\n";
        std::cout << "  - Undetected Faults : " << res.undetectedFaults << "\n";

        if (showCompaction) {
            auto cr = reverseCompact(vectors, res);
            printCompactionReport(cr, ckt);
        }
    } else {
        std::cerr << "[-] Error: Unknown algorithm '" << algoChoice << "'. Use 'sfs', 'ppfs', 'cfs', or 'all'.\n";
        return false;
    }

    return true;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printUsage(argv[0]);
        return 0;
    }

    std::string circuitPath;
    std::string algoChoice = "all";
    int requestedVecCount = 0;
    bool forceExhaustive = false;
    uint32_t seed = 42;
    bool showCompaction = false;
    bool runSweep = false;
    bool exportReports = false;
    bool allBenchmarks = false;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            printUsage(argv[0]);
            return 0;
        } else if (arg == "-c" || arg == "--circuit") {
            if (i + 1 < argc) circuitPath = argv[++i];
        } else if (arg == "-a" || arg == "--algo") {
            if (i + 1 < argc) algoChoice = argv[++i];
        } else if (arg == "-n" || arg == "--vectors") {
            if (i + 1 < argc) requestedVecCount = std::stoi(argv[++i]);
        } else if (arg == "--exhaustive") {
            forceExhaustive = true;
        } else if (arg == "--seed") {
            if (i + 1 < argc) seed = (uint32_t)std::stoul(argv[++i]);
        } else if (arg == "--compact") {
            showCompaction = true;
        } else if (arg == "--dse") {
            runSweep = true;
        } else if (arg == "--export") {
            exportReports = true;
        } else if (arg == "--all-benchmarks") {
            allBenchmarks = true;
        } else if (circuitPath.empty() && arg[0] != '-') {
            circuitPath = arg;
        }
    }

    std::transform(algoChoice.begin(), algoChoice.end(), algoChoice.begin(), ::tolower);

    if (allBenchmarks) {
        std::vector<std::string> benchmarks = {
            "c17.txt",
            "4-bit_magnitude_comparator.txt",
            "6_bit_ALU.txt",
            "8_bit_barrel_shifter.txt",
            "8_bit_carry_select_adder.txt"
        };

        std::cout << "\n============================================================\n";
        std::cout << "  RUNNING DSE BENCHMARK SUITE (" << benchmarks.size() << " CIRCUITS)\n";
        std::cout << "============================================================\n";

        for (const auto& bm : benchmarks) {
            std::cout << "\n>>> Starting Benchmark: " << bm << " <<<\n";
            processCircuit("benchmarks/" + bm, "all", requestedVecCount, forceExhaustive,
                           seed, showCompaction, true, true);
        }
        std::cout << "\n[+] Benchmark suite completed! Reports generated in reports/\n";
        return 0;
    }

    if (circuitPath.empty()) {
        std::cerr << "[-] Error: No circuit netlist specified. Use --circuit <path> or --all-benchmarks.\n";
        printUsage(argv[0]);
        return 1;
    }

    bool success = processCircuit(circuitPath, algoChoice, requestedVecCount, forceExhaustive,
                                  seed, showCompaction, runSweep, exportReports);
    return success ? 0 : 1;
}
