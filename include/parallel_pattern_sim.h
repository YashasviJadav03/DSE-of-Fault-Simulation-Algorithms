#pragma once

// ============================================================
//  parallel_pattern_sim.h  —  Parallel Pattern Fault Simulation
//
//  Packs 64 test vectors into uint64_t words and evaluates
//  all 64 vectors simultaneously using bitwise operations.
//  For each fault: inject, re-propagate, compare outputs.
// ============================================================

#include "circuit.h"
#include "simulator.h"
#include "compaction.h"  // FaultSimResult

#include <vector>
#include <cstdint>

class ParallelPatternFaultSim {
public:
    static constexpr int WORD_SIZE = 64;

    ParallelPatternFaultSim(const Circuit& ckt, const std::vector<int>& topoOrder);

    FaultSimResult run(const std::vector<TestVector>& vectors,
                       bool trackVectorMapping = true);

    const std::vector<Fault>& faults() const { return faults_; }

private:
    const Circuit&          ckt_;
    const std::vector<int>& topoOrder_;
    Simulator               sim_;
    std::vector<Fault>      faults_;

    int collapsedFrom_;
    int collapsedTo_;

    void buildFaultList();

    // Simulate one fault against goodWords
    // Returns 64-bit mask: bit k set → fault detected by vector k
    uint64_t simulateFault(const std::vector<uint64_t>& goodWords,
                           const Fault& fault, int packedCount) const;
};
