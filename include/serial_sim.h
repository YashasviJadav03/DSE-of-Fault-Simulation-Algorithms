#pragma once

// ============================================================
//  serial_sim.h  —  Serial Fault Simulation
//
//  For each test vector, for each fault:
//    1. Inject the fault (force wire to stuck-at value)
//    2. Re-simulate affected gates (event-driven)
//    3. Compare outputs → detected if any output differs
// ============================================================

#include "circuit.h"
#include "simulator.h"
#include "compaction.h"  // FaultSimResult

#include <vector>
#include <string>

class SerialFaultSim {
public:
    SerialFaultSim(const Circuit& ckt, const std::vector<int>& topoOrder);

    // Overload: use a pre-collapsed fault list
    SerialFaultSim(const Circuit& ckt, const std::vector<int>& topoOrder,
                   const std::vector<Fault>& collapsedFaultList);

    FaultSimResult run(const std::vector<TestVector>& vectors,
                       bool trackVectorMapping = true);

    const std::vector<Fault>& faults() const { return faults_; }

private:
    const Circuit&          ckt_;
    const std::vector<int>& topoOrder_;
    std::vector<Fault>      faults_;

    void buildFaultList();
    int evaluateGate(const Node* n, const std::vector<int>& nodeVals) const;
    bool simulateFault(const std::vector<int>& goodVals, const Fault& fault) const;
};
