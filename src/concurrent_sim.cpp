// ============================================================
//  concurrent_sim.cpp  —  Concurrent Fault Simulation
//
//  Classical list-based concurrent fault simulation engine.
//  Each node maintains a set of fault indices that cause its
//  value to deviate from the good-circuit value.
// ============================================================

#include "concurrent_sim.h"
#include "fault_collapsing.h"

#include <chrono>
#include <iostream>
#include <algorithm>

// ============================================================
//  CONSTRUCTORS
// ============================================================

ConcurrentFaultSim::ConcurrentFaultSim(const Circuit& ckt,
                                       const std::vector<int>& topoOrder)
    : ckt_(ckt), topoOrder_(topoOrder),
      sim_(ckt, topoOrder),
      collapsedFrom_(0), collapsedTo_(0)
{
    buildFaultList();
}

ConcurrentFaultSim::ConcurrentFaultSim(const Circuit& ckt,
                                       const std::vector<int>& topoOrder,
                                       const std::vector<Fault>& customFaultList)
    : ckt_(ckt), topoOrder_(topoOrder),
      sim_(ckt, topoOrder),
      faults_(customFaultList),
      collapsedFrom_(ckt.totalFaults()),
      collapsedTo_((int)customFaultList.size())
{
    faultMap_.clear();
    for (int fi = 0; fi < (int)faults_.size(); fi++) {
        faultMap_[faultKey(faults_[fi].nodeId, faults_[fi].stuckAt)] = fi;
    }
}

// ============================================================
//  BUILD FAULT LIST (with structural fault collapsing)
// ============================================================

void ConcurrentFaultSim::buildFaultList() {
    collapsedFrom_ = ckt_.totalFaults();

    FaultCollapser fc(ckt_);
    fc.collapse();

    faults_ = fc.collapsedFaults();
    collapsedTo_ = (int)faults_.size();

    faultMap_.clear();
    for (int fi = 0; fi < (int)faults_.size(); fi++) {
        faultMap_[faultKey(faults_[fi].nodeId, faults_[fi].stuckAt)] = fi;
    }
}

// ============================================================
//  SEED FAULTS AT PRIMARY INPUTS
// ============================================================

void ConcurrentFaultSim::seedFaults(const std::vector<int>& goodVals,
                                     const std::vector<bool>& faultDetected)
{
    for (int inId : ckt_.inputIds) {
        if (inId <= 0 || inId >= (int)goodVals.size()) continue;
        int gv = goodVals[inId];
        if (gv != 0 && gv != 1) continue;

        // Stuck-at fault that differs from good value:
        // If gv == 0 -> SA1 differs; if gv == 1 -> SA0 differs
        int targetSa = (gv == 0) ? 1 : 0;
        auto it = faultMap_.find(faultKey(inId, targetSa));
        if (it != faultMap_.end()) {
            int fi = it->second;
            if (!faultDetected[fi]) {
                nodeFaultLists_[inId].insert(fi);
            }
        }
    }
}

// ============================================================
//  PROPAGATE FAULTS
// ============================================================

void ConcurrentFaultSim::propagateFaults(const std::vector<int>& goodVals,
                                         const std::vector<bool>& faultDetected)
{
    int maxId = (int)nodeFaultLists_.size();

    for (int nodeId : topoOrder_) {
        Node* n = ckt_.getNode(nodeId);
        if (!n || n->nodeType == NodeType::INPUT) continue;
        if (nodeId <= 0 || nodeId >= maxId) continue;

        // ---- BRANCH NODES ----
        if (n->nodeType == NodeType::BRNH) {
            if (!n->inputs.empty()) {
                int stemId = n->inputs[0];
                if (stemId > 0 && stemId < maxId) {
                    nodeFaultLists_[nodeId] = nodeFaultLists_[stemId];
                }
            }

            // Local fault on this branch
            int gv = (nodeId < (int)goodVals.size()) ? goodVals[nodeId] : -1;
            if (gv == 0 || gv == 1) {
                int targetSa = (gv == 0) ? 1 : 0;
                auto it = faultMap_.find(faultKey(nodeId, targetSa));
                if (it != faultMap_.end()) {
                    int fi = it->second;
                    if (!faultDetected[fi]) {
                        nodeFaultLists_[nodeId].insert(fi);
                    }
                }
            }
            continue;
        }

        // ---- GATE NODES ----
        std::unordered_set<int>& outList = nodeFaultLists_[nodeId];
        outList.clear();

        GateType gt = n->gateType;
        const auto& inIds = n->inputs;

        if (gt == GateType::NOT || gt == GateType::BUFF) {
            if (!inIds.empty() && inIds[0] > 0 && inIds[0] < maxId) {
                outList = nodeFaultLists_[inIds[0]];
            }
        } else if (gt == GateType::XOR || gt == GateType::XNOR) {
            // Count occurrences: an odd count flips XOR/XNOR output
            std::unordered_map<int, int> faultCounts;
            for (int inId : inIds) {
                if (inId <= 0 || inId >= maxId) continue;
                for (int fi : nodeFaultLists_[inId]) {
                    faultCounts[fi]++;
                }
            }
            for (const auto& kv : faultCounts) {
                if ((kv.second % 2) != 0) {
                    outList.insert(kv.first);
                }
            }
        } else if (gt == GateType::AND || gt == GateType::NAND ||
                   gt == GateType::OR  || gt == GateType::NOR) {
            int ctrlVal = (gt == GateType::AND || gt == GateType::NAND) ? 0 : 1;

            // If any constant input is controlling (e.g. 1'b0 on AND),
            // no fault can flip it, so no input fault can ever propagate through.
            bool permanentlyBlocked = false;
            for (int inId : inIds) {
                if ((inId == -1 && ctrlVal == 0) || (inId == -2 && ctrlVal == 1)) {
                    permanentlyBlocked = true;
                    break;
                }
            }

            // Check if any input has controlling good value
            bool hasCtrlInput = false;
            for (int inId : inIds) {
                if (inId <= 0 || inId >= (int)goodVals.size()) continue;
                if (goodVals[inId] == ctrlVal) {
                    hasCtrlInput = true;
                    break;
                }
            }

            if (permanentlyBlocked) {
                // Gate output is permanently blocked by constant controlling input
            } else if (!hasCtrlInput) {
                // Pass-through state: ALL inputs are non-controlling.
                // Any input flipping to controlling will flip the output.
                for (int inId : inIds) {
                    if (inId <= 0 || inId >= maxId) continue;
                    for (int fi : nodeFaultLists_[inId]) {
                        outList.insert(fi);
                    }
                }
            } else {
                // Blocked state: one or more inputs have controlling value.
                // Fault must invert ALL controlling inputs simultaneously,
                // and MUST NOT invert any non-controlling input.
                bool firstCtrl = true;
                std::unordered_set<int> ctrlIntersection;

                for (int inId : inIds) {
                    if (inId <= 0 || inId >= (int)goodVals.size()) continue;
                    if (goodVals[inId] == ctrlVal) {
                        if (inId >= maxId) continue;
                        if (firstCtrl) {
                            ctrlIntersection = nodeFaultLists_[inId];
                            firstCtrl = false;
                        } else {
                            std::unordered_set<int> tmp;
                            for (int fi : nodeFaultLists_[inId]) {
                                if (ctrlIntersection.count(fi)) tmp.insert(fi);
                            }
                            ctrlIntersection = std::move(tmp);
                        }
                    }
                }

                if (!firstCtrl && !ctrlIntersection.empty()) {
                    // Subtract faults appearing on non-controlling inputs
                    std::unordered_set<int> nonCtrlFaults;
                    for (int inId : inIds) {
                        if (inId <= 0 || inId >= (int)goodVals.size()) continue;
                        if (goodVals[inId] != ctrlVal && inId < maxId) {
                            for (int fi : nodeFaultLists_[inId]) {
                                nonCtrlFaults.insert(fi);
                            }
                        }
                    }

                    for (int fi : ctrlIntersection) {
                        if (!nonCtrlFaults.count(fi)) {
                            outList.insert(fi);
                        }
                    }
                }
            }
        }

        // Local stuck-at fault on this gate's output wire
        int gv = (nodeId < (int)goodVals.size()) ? goodVals[nodeId] : -1;
        if (gv == 0 || gv == 1) {
            int targetSa = (gv == 0) ? 1 : 0;
            auto it = faultMap_.find(faultKey(nodeId, targetSa));
            if (it != faultMap_.end()) {
                int fi = it->second;
                if (!faultDetected[fi]) {
                    outList.insert(fi);
                }
            }
        }
    }
}

// ============================================================
//  COLLECT DETECTED FAULTS AT PRIMARY OUTPUTS
// ============================================================

std::unordered_set<int> ConcurrentFaultSim::collectDetected() {
    std::unordered_set<int> detected;
    int maxId = (int)nodeFaultLists_.size();

    for (int oid : ckt_.outputIds) {
        if (oid <= 0 || oid >= maxId) continue;
        for (int fi : nodeFaultLists_[oid]) {
            detected.insert(fi);
        }

        // Also check any branch node associated with this output
        Node* outNode = ckt_.getNode(oid);
        if (outNode) {
            for (int bid : ckt_.branchIds) {
                Node* br = ckt_.getNode(bid);
                if (br && !br->inputs.empty() && br->inputs[0] == oid && bid < maxId) {
                    for (int fi : nodeFaultLists_[bid]) {
                        detected.insert(fi);
                    }
                }
            }
        }
    }

    return detected;
}

// ============================================================
//  RUN — main simulation loop
// ============================================================

FaultSimResult ConcurrentFaultSim::run(const std::vector<TestVector>& vectors,
                                       bool trackVectorMapping)
{
    auto tStart = std::chrono::high_resolution_clock::now();

    int numFaults  = (int)faults_.size();
    int numVectors = (int)vectors.size();

    FaultSimResult res;
    res.totalFaults      = numFaults;
    res.collapsedFrom    = collapsedFrom_;
    res.collapsedTo      = collapsedTo_;
    res.faultDetectedBy.resize(numFaults);
    res.vectorDetects.resize(numVectors);

    std::vector<bool> faultDetected(numFaults, false);

    int maxNodeId = (int)ckt_.nodeArray.size() + 10;
    nodeFaultLists_.resize(maxNodeId);

    for (int vi = 0; vi < numVectors; vi++) {
        // Clear per-node fault lists
        for (auto& fl : nodeFaultLists_) fl.clear();

        // 1. Good-circuit simulate
        sim_.applyVector(vectors[vi]);
        sim_.simulate();
        auto goodVals = sim_.snapshot();

        // 2. Seed faults at primary inputs
        seedFaults(goodVals, faultDetected);

        // 3. Propagate faults level by level
        propagateFaults(goodVals, faultDetected);

        // 4. Collect detected faults at primary outputs
        std::unordered_set<int> newlyDetected = collectDetected();

        for (int fi : newlyDetected) {
            if (!faultDetected[fi]) {
                faultDetected[fi] = true;
            }
            if (trackVectorMapping) {
                res.faultDetectedBy[fi].push_back(vi);
                res.vectorDetects[vi].insert(fi);
            }
        }
    }

    auto tEnd = std::chrono::high_resolution_clock::now();
    res.computationTimeMs = std::chrono::duration_cast<std::chrono::microseconds>(
        tEnd - tStart).count() / 1000.0;

    res.detectedFaults = 0;
    for (int fi = 0; fi < numFaults; fi++) {
        if (faultDetected[fi]) res.detectedFaults++;
    }
    res.undetectedFaults = numFaults - res.detectedFaults;
    res.faultCoverage = numFaults > 0
        ? 100.0 * res.detectedFaults / numFaults : 0.0;

    return res;
}
