// ============================================================
//  dse_engine.cpp  —  Design Space Exploration Engine
// ============================================================

#include "dse_engine.h"
#include "fault_collapsing.h"

#include <iostream>
#include <iomanip>
#include <fstream>
#include <algorithm>

DSEEngine::DSEEngine(const Circuit& ckt, const std::vector<int>& topoOrder)
    : ckt_(ckt), topoOrder_(topoOrder)
{
}

DSEReport DSEEngine::runComparison(const std::vector<TestVector>& vectors, bool runSweep) {
    DSEReport rep;
    rep.circuitName       = ckt_.name;
    rep.numPIs           = (int)ckt_.inputIds.size();
    rep.numPOs           = (int)ckt_.outputIds.size();
    rep.numGates         = (int)ckt_.gateIds.size();
    rep.totalNodes       = ckt_.totalNodes();
    rep.totalFaultsBefore = ckt_.totalFaults();
    rep.testVectorCount  = (int)vectors.size();

    // 1. Unified Fault Collapsing so all algorithms simulate the exact same fault set
    FaultCollapser fc(ckt_);
    fc.collapse();
    const auto& collapsedFaults = fc.collapsedFaults();
    rep.totalFaultsAfter = (int)collapsedFaults.size();

    // 2. Run Serial Fault Simulation
    {
        AlgoDSEResult aRes;
        aRes.name = "Serial Fault Simulation (SFS)";
        SerialFaultSim sfs(ckt_, topoOrder_, collapsedFaults);
        aRes.simResult = sfs.run(vectors, true);
        aRes.compaction = reverseCompact(vectors, aRes.simResult);

        for (int fi = 0; fi < (int)collapsedFaults.size(); fi++) {
            if (aRes.simResult.faultDetectedBy[fi].empty()) {
                aRes.undetectedFaultNames.push_back(
                    collapsedFaults[fi].nodeName + " SA" + std::to_string(collapsedFaults[fi].stuckAt));
            }
        }
        rep.algoResults.push_back(aRes);
    }

    // 3. Run Parallel Pattern Fault Simulation
    {
        AlgoDSEResult aRes;
        aRes.name = "Parallel Pattern Fault Simulation (PPFS)";
        ParallelPatternFaultSim ppfs(ckt_, topoOrder_);
        aRes.simResult = ppfs.run(vectors, true);
        aRes.compaction = reverseCompact(vectors, aRes.simResult);

        for (int fi = 0; fi < (int)collapsedFaults.size(); fi++) {
            if (aRes.simResult.faultDetectedBy[fi].empty()) {
                aRes.undetectedFaultNames.push_back(
                    collapsedFaults[fi].nodeName + " SA" + std::to_string(collapsedFaults[fi].stuckAt));
            }
        }
        rep.algoResults.push_back(aRes);
    }

    // 4. Run Concurrent Fault Simulation
    {
        AlgoDSEResult aRes;
        aRes.name = "Concurrent Fault Simulation (CFS)";
        ConcurrentFaultSim cfs(ckt_, topoOrder_, collapsedFaults);
        aRes.simResult = cfs.run(vectors, true);
        aRes.compaction = reverseCompact(vectors, aRes.simResult);

        for (int fi = 0; fi < (int)collapsedFaults.size(); fi++) {
            if (aRes.simResult.faultDetectedBy[fi].empty()) {
                aRes.undetectedFaultNames.push_back(
                    collapsedFaults[fi].nodeName + " SA" + std::to_string(collapsedFaults[fi].stuckAt));
            }
        }
        rep.algoResults.push_back(aRes);
    }

    // 5. Sweep analysis (if requested and we have sufficient vectors)
    if (runSweep && vectors.size() > 2) {
        std::vector<int> sweepSizes;
        for (int sz = 2; sz < (int)vectors.size(); sz *= 2) {
            sweepSizes.push_back(sz);
        }
        sweepSizes.push_back((int)vectors.size());

        for (int sz : sweepSizes) {
            std::vector<TestVector> subVecs(vectors.begin(), vectors.begin() + sz);

            SerialFaultSim sfs(ckt_, topoOrder_, collapsedFaults);
            auto sRes = sfs.run(subVecs, false);

            ParallelPatternFaultSim ppfs(ckt_, topoOrder_);
            auto pRes = ppfs.run(subVecs, false);

            ConcurrentFaultSim cfs(ckt_, topoOrder_, collapsedFaults);
            auto cRes = cfs.run(subVecs, false);

            DSEReport::SweepPoint sp;
            sp.vectorCount  = sz;
            sp.sfsCoverage  = sRes.faultCoverage;
            sp.sfsTimeMs    = sRes.computationTimeMs;
            sp.ppfsCoverage = pRes.faultCoverage;
            sp.ppfsTimeMs   = pRes.computationTimeMs;
            sp.cfsCoverage  = cRes.faultCoverage;
            sp.cfsTimeMs    = cRes.computationTimeMs;
            rep.sweepPoints.push_back(sp);
        }
    }

    return rep;
}

// ============================================================
//  CONSOLE REPORT
// ============================================================

void DSEEngine::printConsoleReport(const DSEReport& rep) {
    std::cout << "\n============================================================\n";
    std::cout << "  DESIGN SPACE EXPLORATION (DSE) REPORT\n";
    std::cout << "  Circuit: " << rep.circuitName << "\n";
    std::cout << "============================================================\n";
    std::cout << "  Primary Inputs (PIs) : " << rep.numPIs << "\n";
    std::cout << "  Primary Outputs (POs): " << rep.numPOs << "\n";
    std::cout << "  Logic Gates          : " << rep.numGates << "\n";
    std::cout << "  Total Circuit Nodes  : " << rep.totalNodes << "\n";
    std::cout << "  Original Faults      : " << rep.totalFaultsBefore << "\n";
    std::cout << "  Collapsed Faults     : " << rep.totalFaultsAfter
              << " (" << std::fixed << std::setprecision(1)
              << (100.0 * (1.0 - (double)rep.totalFaultsAfter / rep.totalFaultsBefore))
              << "% reduction)\n";
    std::cout << "  Test Vectors Applied : " << rep.testVectorCount << "\n";
    std::cout << "------------------------------------------------------------\n";

    // Table 1: Cross-Algorithm Performance & Coverage
    std::cout << "\n[1] ALGORITHM COMPARISON (All 4 Professor-Mandated Metrics)\n";
    std::cout << "+------------------------------+------------+------------+------------+---------------+----------------+\n";
    std::cout << "| Algorithm                    | Time (ms)  | Speedup*   | Coverage % | Undetected #  | Compacted Vecs |\n";
    std::cout << "+------------------------------+------------+------------+------------+---------------+----------------+\n";

    double baseTime = rep.algoResults.empty() ? 1.0 : rep.algoResults[0].simResult.computationTimeMs;
    if (baseTime <= 0.0) baseTime = 0.001;

    for (const auto& a : rep.algoResults) {
        double sp = baseTime / (a.simResult.computationTimeMs > 0 ? a.simResult.computationTimeMs : 0.001);
        std::cout << "| " << std::left << std::setw(28) << a.name << " | "
                  << std::right << std::setw(10) << std::fixed << std::setprecision(3) << a.simResult.computationTimeMs << " | "
                  << std::right << std::setw(9) << std::fixed << std::setprecision(2) << sp << "x | "
                  << std::right << std::setw(9) << std::fixed << std::setprecision(2) << a.simResult.faultCoverage << "% | "
                  << std::right << std::setw(13) << a.simResult.undetectedFaults << " | "
                  << std::right << std::setw(14) << a.compaction.compactedCount << " |\n";
    }
    std::cout << "+------------------------------+------------+------------+------------+---------------+----------------+\n";
    std::cout << " *Speedup normalized relative to Serial Fault Simulation (SFS).\n";

    // Table 2: Reverse Compaction Details
    std::cout << "\n[2] REVERSE COMPACTION SUMMARY\n";
    for (const auto& a : rep.algoResults) {
        std::cout << "  * " << a.name << ":\n";
        std::cout << "      Original Vectors  : " << a.compaction.originalCount << "\n";
        std::cout << "      Compacted Vectors : " << a.compaction.compactedCount
                  << " (" << std::fixed << std::setprecision(1) << a.compaction.reductionPct << "% reduction)\n";
        std::cout << "      Faults Preserved  : " << a.compaction.faultsCovered << " / " << a.compaction.totalFaults
                  << " (" << std::fixed << std::setprecision(2) << a.compaction.coveragePct << "%)\n";
    }

    // List of undetected faults
    std::cout << "\n[3] UNDETECTED FAULTS\n";
    if (rep.algoResults.empty() || rep.algoResults[0].undetectedFaultNames.empty()) {
        std::cout << "  None! All simulated faults were successfully detected (100% coverage).\n";
    } else {
        std::cout << "  " << rep.algoResults[0].undetectedFaultNames.size() << " undetected fault(s):\n";
        for (const auto& fname : rep.algoResults[0].undetectedFaultNames) {
            std::cout << "    - " << fname << "\n";
        }
    }

    // Sweep table (if available)
    if (!rep.sweepPoints.empty()) {
        std::cout << "\n[4] DSE SCALING ANALYSIS (Vector Budget Sweep)\n";
        std::cout << "+---------+--------------------+--------------------+--------------------+\n";
        std::cout << "| Vectors | SFS Cov% / Time(ms)| PPFS Cov% /Time(ms)| CFS Cov% / Time(ms)|\n";
        std::cout << "+---------+--------------------+--------------------+--------------------+\n";
        for (const auto& sp : rep.sweepPoints) {
            std::cout << "| " << std::setw(7) << sp.vectorCount << " | "
                      << std::setw(6) << std::fixed << std::setprecision(1) << sp.sfsCoverage << "% / "
                      << std::setw(8) << std::fixed << std::setprecision(3) << sp.sfsTimeMs << " | "
                      << std::setw(6) << std::fixed << std::setprecision(1) << sp.ppfsCoverage << "% / "
                      << std::setw(8) << std::fixed << std::setprecision(3) << sp.ppfsTimeMs << " | "
                      << std::setw(6) << std::fixed << std::setprecision(1) << sp.cfsCoverage << "% / "
                      << std::setw(8) << std::fixed << std::setprecision(3) << sp.cfsTimeMs << " |\n";
        }
        std::cout << "+---------+--------------------+--------------------+--------------------+\n";
    }
    std::cout << "============================================================\n\n";
}

// ============================================================
//  EXPORT MARKDOWN REPORT
// ============================================================

void DSEEngine::exportMarkdown(const DSEReport& rep, const std::string& filepath) {
    std::ofstream out(filepath);
    if (!out) return;

    out << "# Design Space Exploration Report: " << rep.circuitName << "\n\n";
    out << "## Circuit Information\n\n";
    out << "| Parameter | Value |\n";
    out << "| :--- | :--- |\n";
    out << "| Circuit Name | `" << rep.circuitName << "` |\n";
    out << "| Primary Inputs (PIs) | " << rep.numPIs << " |\n";
    out << "| Primary Outputs (POs) | " << rep.numPOs << " |\n";
    out << "| Logic Gates | " << rep.numGates << " |\n";
    out << "| Total Circuit Nodes | " << rep.totalNodes << " |\n";
    out << "| Original Total Faults | " << rep.totalFaultsBefore << " |\n";
    out << "| Collapsed Faults | " << rep.totalFaultsAfter << " ("
        << std::fixed << std::setprecision(1)
        << (100.0 * (1.0 - (double)rep.totalFaultsAfter / rep.totalFaultsBefore)) << "% reduction) |\n";
    out << "| Test Vectors Evaluated | " << rep.testVectorCount << " |\n\n";

    out << "## Algorithm Performance Comparison\n\n";
    out << "| Algorithm | Time (ms) | Speedup vs SFS | Fault Coverage (%) | Undetected Faults | Compacted Test Vectors |\n";
    out << "| :--- | :---: | :---: | :---: | :---: | :---: |\n";

    double baseTime = rep.algoResults.empty() ? 1.0 : rep.algoResults[0].simResult.computationTimeMs;
    if (baseTime <= 0.0) baseTime = 0.001;

    for (const auto& a : rep.algoResults) {
        double sp = baseTime / (a.simResult.computationTimeMs > 0 ? a.simResult.computationTimeMs : 0.001);
        out << "| **" << a.name << "** | "
            << std::fixed << std::setprecision(3) << a.simResult.computationTimeMs << " | "
            << std::fixed << std::setprecision(2) << sp << "x | "
            << std::fixed << std::setprecision(2) << a.simResult.faultCoverage << "% | "
            << a.simResult.undetectedFaults << " | "
            << a.compaction.compactedCount << " (" << std::fixed << std::setprecision(1) << a.compaction.reductionPct << "% red.) |\n";
    }

    out << "\n## Undetected Faults\n\n";
    if (rep.algoResults.empty() || rep.algoResults[0].undetectedFaultNames.empty()) {
        out << "*All simulated faults were detected (100% coverage).*\n\n";
    } else {
        out << "The following " << rep.algoResults[0].undetectedFaultNames.size() << " faults remained undetected:\n\n";
        for (const auto& fn : rep.algoResults[0].undetectedFaultNames) {
            out << "- `" << fn << "`\n";
        }
        out << "\n";
    }

    if (!rep.sweepPoints.empty()) {
        out << "## Vector Budget Scaling (Design Space Exploration)\n\n";
        out << "| Vectors | SFS Cov (%) | SFS Time (ms) | PPFS Cov (%) | PPFS Time (ms) | CFS Cov (%) | CFS Time (ms) |\n";
        out << "| :---: | :---: | :---: | :---: | :---: | :---: | :---: |\n";
        for (const auto& sp : rep.sweepPoints) {
            out << "| " << sp.vectorCount << " | "
                << std::fixed << std::setprecision(1) << sp.sfsCoverage << "% | "
                << std::fixed << std::setprecision(3) << sp.sfsTimeMs << " | "
                << std::fixed << std::setprecision(1) << sp.ppfsCoverage << "% | "
                << std::fixed << std::setprecision(3) << sp.ppfsTimeMs << " | "
                << std::fixed << std::setprecision(1) << sp.cfsCoverage << "% | "
                << std::fixed << std::setprecision(3) << sp.cfsTimeMs << " |\n";
        }
    }
}

// ============================================================
//  EXPORT CSV REPORT
// ============================================================

void DSEEngine::exportCSV(const DSEReport& rep, const std::string& filepath) {
    std::ofstream out(filepath);
    if (!out) return;

    out << "Metric,Serial,ParallelPattern,Concurrent\n";
    if (rep.algoResults.size() >= 3) {
        out << "ComputationTimeMs,"
            << rep.algoResults[0].simResult.computationTimeMs << ","
            << rep.algoResults[1].simResult.computationTimeMs << ","
            << rep.algoResults[2].simResult.computationTimeMs << "\n";
        out << "FaultCoveragePct,"
            << rep.algoResults[0].simResult.faultCoverage << ","
            << rep.algoResults[1].simResult.faultCoverage << ","
            << rep.algoResults[2].simResult.faultCoverage << "\n";
        out << "UndetectedFaults,"
            << rep.algoResults[0].simResult.undetectedFaults << ","
            << rep.algoResults[1].simResult.undetectedFaults << ","
            << rep.algoResults[2].simResult.undetectedFaults << "\n";
        out << "OriginalVectors,"
            << rep.algoResults[0].compaction.originalCount << ","
            << rep.algoResults[1].compaction.originalCount << ","
            << rep.algoResults[2].compaction.originalCount << "\n";
        out << "CompactedVectors,"
            << rep.algoResults[0].compaction.compactedCount << ","
            << rep.algoResults[1].compaction.compactedCount << ","
            << rep.algoResults[2].compaction.compactedCount << "\n";
    }

    if (!rep.sweepPoints.empty()) {
        out << "\nSweep_VectorCount,SFS_Cov,SFS_TimeMs,PPFS_Cov,PPFS_TimeMs,CFS_Cov,CFS_TimeMs\n";
        for (const auto& sp : rep.sweepPoints) {
            out << sp.vectorCount << ","
                << sp.sfsCoverage << "," << sp.sfsTimeMs << ","
                << sp.ppfsCoverage << "," << sp.ppfsTimeMs << ","
                << sp.cfsCoverage << "," << sp.cfsTimeMs << "\n";
        }
    }
}
