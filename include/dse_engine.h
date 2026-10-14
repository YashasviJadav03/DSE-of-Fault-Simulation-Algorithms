#pragma once

// ============================================================
//  dse_engine.h  —  Design Space Exploration (DSE) Engine
//
//  Coordinates fair comparative analysis across:
//    1. Serial Fault Simulation (SFS)
//    2. Parallel Pattern Fault Simulation (PPFS)
//    3. Concurrent Fault Simulation (CFS)
//
//  Computes and reports:
//    - Computation time & relative speedups
//    - Fault coverage (%)
//    - Undetected fault list
//    - Test vectors before & after Reverse Compaction
//    - Vector budget sweep curves (DSE scaling analysis)
// ============================================================

#include "circuit.h"
#include "simulator.h"
#include "compaction.h"
#include "serial_sim.h"
#include "parallel_pattern_sim.h"
#include "concurrent_sim.h"

#include <string>
#include <vector>

struct AlgoDSEResult {
    std::string name;              // "Serial", "Parallel Pattern", "Concurrent"
    FaultSimResult simResult;
    CompactionResult compaction;
    std::vector<std::string> undetectedFaultNames;
};

struct DSEReport {
    std::string circuitName;
    int numPIs;
    int numPOs;
    int numGates;
    int totalNodes;
    int totalFaultsBefore;
    int totalFaultsAfter;
    int testVectorCount;

    std::vector<AlgoDSEResult> algoResults;

    struct SweepPoint {
        int vectorCount;
        double sfsCoverage;
        double sfsTimeMs;
        double ppfsCoverage;
        double ppfsTimeMs;
        double cfsCoverage;
        double cfsTimeMs;
    };
    std::vector<SweepPoint> sweepPoints;
};

class DSEEngine {
public:
    DSEEngine(const Circuit& ckt, const std::vector<int>& topoOrder);

    // Run full comparative analysis on a set of test vectors
    DSEReport runComparison(const std::vector<TestVector>& vectors, bool runSweep = true);

    // Print formatted console report
    static void printConsoleReport(const DSEReport& rep);

    // Export markdown report
    static void exportMarkdown(const DSEReport& rep, const std::string& filepath);

    // Export CSV report
    static void exportCSV(const DSEReport& rep, const std::string& filepath);

private:
    const Circuit& ckt_;
    const std::vector<int>& topoOrder_;
};
