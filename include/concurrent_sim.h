#pragma once

// ============================================================
//  concurrent_sim.h  —  Concurrent Fault Simulation
//
//  Each gate maintains a fault list: a set of fault indices
//  that would cause a different value at this gate compared
//  to the good-circuit value.
//
//  For each test vector:
//    1. Good-circuit simulate
//    2. Seed faults at PIs
//    3. Propagate level by level using gate-specific rules
//    4. At POs: union all fault IDs → detected faults
// ============================================================

#include "circuit.h"
#include "simulator.h"
#include "compaction.h"  // FaultSimResult

#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <cstdint>

class ConcurrentFaultSim {
public:
    ConcurrentFaultSim(const Circuit& ckt, const std::vector<int>& topoOrder);
    ConcurrentFaultSim(const Circuit& ckt, const std::vector<int>& topoOrder,
                       const std::vector<Fault>& customFaultList);

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

    // Map: (nodeId, stuckAt) -> fault index in faults_
    std::unordered_map<int64_t, int> faultMap_;
    static int64_t faultKey(int nodeId, int sa) {
        return ((int64_t)nodeId << 32) | (int64_t)sa;
    }

    // Per-node fault lists (indexed by nodeId)
    std::vector<std::unordered_set<int>> nodeFaultLists_;

    void seedFaults(const std::vector<int>& goodVals, const std::vector<bool>& faultDetected);
    void propagateFaults(const std::vector<int>& goodVals, const std::vector<bool>& faultDetected);
    std::unordered_set<int> collectDetected();
};
