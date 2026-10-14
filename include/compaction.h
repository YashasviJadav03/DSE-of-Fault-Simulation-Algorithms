#pragma once

// ============================================================
//  compaction.h  —  Reverse Compaction (Backward Elimination)
//
//  Algorithm: Start from the full candidate test vector set.
//  Walk backward from the last vector to the first.
//  Try removing each vector; if the remaining set still
//  covers every detectable fault, permanently drop it.
//  Result: minimal set that preserves full fault coverage.
// ============================================================

#include "circuit.h"
#include "simulator.h"

#include <vector>
#include <string>
#include <unordered_set>

// ---- Fault Simulation Result (shared by all algorithms) ----

struct FaultSimResult {
    int    totalFaults;
    int    detectedFaults;
    int    undetectedFaults;
    double faultCoverage;       // percentage
    double computationTimeMs;

    // Fault collapsing stats (if applicable)
    int    collapsedFrom;       // fault count before collapsing
    int    collapsedTo;         // fault count after collapsing (= totalFaults)

    // faultDetectedBy[fi] = vector indices that detect fault fi
    std::vector<std::vector<int>> faultDetectedBy;

    // vectorDetects[vi] = fault indices detected by vector vi
    std::vector<std::unordered_set<int>> vectorDetects;
};

// ---- Compaction Result ----

struct CompactionResult {
    std::vector<int> keptIndices;           // indices into original vector list
    std::vector<TestVector> compactedVectors;

    int originalCount;
    int compactedCount;
    int faultsCovered;
    int totalFaults;

    double reductionPct;
    double coveragePct;
};

// ---- Reverse Compaction Function ----
// vectors    : the original full test vector set
// fsResult   : result from fault simulation (must have trackVectorMapping=true)
CompactionResult reverseCompact(const std::vector<TestVector>& vectors,
                                const FaultSimResult& fsResult);

// Print a detailed compaction report including the kept test vectors.
void printCompactionReport(const CompactionResult& cr, const Circuit& ckt);
