// ============================================================
//  serial_sim.cpp  —  Serial Fault Simulation
//
//  For each test vector, for each fault:
//    1. Good-circuit simulate (get golden values)
//    2. Copy golden values → faulty snapshot
//    3. Inject fault (force stuck-at value)
//    4. Event-driven re-simulation of affected cone
//    5. Compare outputs → detected if any output differs
// ============================================================

#include "serial_sim.h"

#include <chrono>
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <unordered_set>

// ============================================================
//  CONSTRUCTOR — build fault list
// ============================================================

SerialFaultSim::SerialFaultSim(const Circuit& ckt,
                               const std::vector<int>& topoOrder)
    : ckt_(ckt), topoOrder_(topoOrder)
{
    buildFaultList();
}

SerialFaultSim::SerialFaultSim(const Circuit& ckt,
                               const std::vector<int>& topoOrder,
                               const std::vector<Fault>& collapsedFaultList)
    : ckt_(ckt), topoOrder_(topoOrder), faults_(collapsedFaultList)
{
}

// ============================================================
//  BUILD FAULT LIST
// ============================================================

void SerialFaultSim::buildFaultList() {
    faults_.clear();

    auto addFaults = [&](int id) {
        Node* n = ckt_.getNode(id);
        if (!n) return;
        faults_.push_back({ id, n->name, 0 });
        faults_.push_back({ id, n->name, 1 });
    };

    for (int id : ckt_.inputIds)  addFaults(id);
    for (int id : ckt_.gateIds)   addFaults(id);
    for (int id : ckt_.branchIds) addFaults(id);
}

// ============================================================
//  GATE EVALUATION (on faulty value array)
// ============================================================

int SerialFaultSim::evaluateGate(const Node* n,
                                  const std::vector<int>& nodeVals) const {
    std::vector<int> inVals;
    inVals.reserve(n->inputs.size());

    for (int inId : n->inputs) {
        if (inId == -1) { inVals.push_back(0); continue; }
        if (inId == -2) { inVals.push_back(1); continue; }
        int v = (inId < (int)nodeVals.size()) ? nodeVals[inId] : -1;
        inVals.push_back(v);
    }

    switch (n->gateType) {
        case GateType::AND:
            for (int v : inVals) if (v == 0) return 0;
            for (int v : inVals) if (v < 0) return -1;
            return 1;
        case GateType::NAND:
            for (int v : inVals) if (v == 0) return 1;
            for (int v : inVals) if (v < 0) return -1;
            return 0;
        case GateType::OR:
            for (int v : inVals) if (v == 1) return 1;
            for (int v : inVals) if (v < 0) return -1;
            return 0;
        case GateType::NOR:
            for (int v : inVals) if (v == 1) return 0;
            for (int v : inVals) if (v < 0) return -1;
            return 1;
        case GateType::XOR: {
            int r = 0;
            for (int v : inVals) { if (v < 0) return -1; r ^= v; }
            return r;
        }
        case GateType::XNOR: {
            int r = 0;
            for (int v : inVals) { if (v < 0) return -1; r ^= v; }
            return r ^ 1;
        }
        case GateType::NOT:
            return (inVals.empty() || inVals[0] < 0) ? -1 : inVals[0] ^ 1;
        case GateType::BUFF:
            return (inVals.empty() || inVals[0] < 0) ? -1 : inVals[0];
        default:
            return -1;
    }
}

// ============================================================
//  SIMULATE ONE FAULT AGAINST ONE TEST VECTOR
// ============================================================

bool SerialFaultSim::simulateFault(const std::vector<int>& goodVals,
                                    const Fault& fault) const {
    int maxId = (int)goodVals.size();

    // Step 1: copy fault-free snapshot
    std::vector<int> faultyVals(goodVals);

    // Step 2: inject fault
    if (fault.nodeId >= maxId) return false;
    faultyVals[fault.nodeId] = fault.stuckAt;

    // If stuck value equals fault-free value → not activated
    if (goodVals[fault.nodeId] == fault.stuckAt) return false;

    // Step 3: event-driven re-simulation
    std::unordered_set<int> changedSet;
    changedSet.insert(fault.nodeId);

    for (int nodeId : topoOrder_) {
        if (nodeId == fault.nodeId) continue; // Never overwrite injected fault
        Node* n = ckt_.getNode(nodeId);
        if (!n || n->nodeType == NodeType::INPUT) continue;

        bool inputChanged = false;
        for (int inId : n->inputs) {
            if (inId < 0) continue;
            if (changedSet.count(inId)) { inputChanged = true; break; }
        }
        if (!inputChanged) continue;

        int oldVal = (nodeId < maxId) ? faultyVals[nodeId] : -1;
        int newVal;
        if (n->nodeType == NodeType::BRNH) {
            newVal = (!n->inputs.empty() && n->inputs[0] > 0 && n->inputs[0] < maxId)
                     ? faultyVals[n->inputs[0]] : -1;
        } else {
            newVal = evaluateGate(n, faultyVals);
        }

        if (nodeId < maxId) faultyVals[nodeId] = newVal;

        if (newVal != oldVal) changedSet.insert(nodeId);
    }

    // Step 4: compare outputs
    for (int oid : ckt_.outputIds) {
        if (oid <= 0 || oid >= maxId) continue;
        if (faultyVals[oid] != goodVals[oid] &&
            goodVals[oid] != -1 && faultyVals[oid] != -1)
            return true;

        // Also check if any branch of this output wire changed
        Node* outNode = ckt_.getNode(oid);
        if (outNode) {
            for (int bid : ckt_.branchIds) {
                Node* br = ckt_.getNode(bid);
                if (br && !br->inputs.empty() && br->inputs[0] == oid && bid < maxId) {
                    if (faultyVals[bid] != goodVals[bid] &&
                        goodVals[bid] != -1 && faultyVals[bid] != -1)
                        return true;
                }
            }
        }
    }

    return false;
}

// ============================================================
//  RUN — main simulation loop
// ============================================================

FaultSimResult SerialFaultSim::run(const std::vector<TestVector>& vectors,
                                    bool trackVectorMapping) {
    auto tStart = std::chrono::high_resolution_clock::now();

    int numFaults  = (int)faults_.size();
    int numVectors = (int)vectors.size();

    FaultSimResult res;
    res.totalFaults = numFaults;
    res.collapsedFrom = ckt_.totalFaults();
    res.collapsedTo   = numFaults;

    res.faultDetectedBy.resize(numFaults);
    res.vectorDetects.resize(numVectors);

    Simulator sim(ckt_, topoOrder_);

    // Track which faults have been detected (for fault dropping)
    std::vector<bool> faultDetected(numFaults, false);

    for (int vi = 0; vi < numVectors; vi++) {
        sim.applyVector(vectors[vi]);
        sim.simulate();
        auto goodVals = sim.snapshot();

        for (int fi = 0; fi < numFaults; fi++) {
            if (faultDetected[fi]) continue;  // fault dropping

            if (simulateFault(goodVals, faults_[fi])) {
                faultDetected[fi] = true;
                if (trackVectorMapping) {
                    res.faultDetectedBy[fi].push_back(vi);
                    res.vectorDetects[vi].insert(fi);
                }
            }
        }
    }

    auto tEnd = std::chrono::high_resolution_clock::now();
    res.computationTimeMs = std::chrono::duration_cast<std::chrono::microseconds>(
        tEnd - tStart).count() / 1000.0;

    res.detectedFaults = 0;
    for (int fi = 0; fi < numFaults; fi++)
        if (faultDetected[fi]) res.detectedFaults++;
    res.undetectedFaults = numFaults - res.detectedFaults;
    res.faultCoverage = numFaults > 0
        ? 100.0 * res.detectedFaults / numFaults : 0.0;

    return res;
}
