// ============================================================
//  parallel_pattern_sim.cpp  —  Parallel Pattern Fault Simulation
//
//  PPFS Algorithm:
//    Outer loop: next W=64 patterns
//      → pack into 64-bit words, good-circuit simulate
//    Inner loop: next fault f
//      → inject fault, re-propagate using evalGate64
//      → compare output words: bit k differs → detected by vector k
//
//  Includes fault collapsing (dominance + equivalence)
//  using Checkpoint Theorem with gate-specific rules.
// ============================================================

#include "parallel_pattern_sim.h"
#include "fault_collapsing.h"
#include <chrono>
#include <iostream>
#include <algorithm>
#include <unordered_map>

// ============================================================
//  CONSTRUCTOR
// ============================================================

ParallelPatternFaultSim::ParallelPatternFaultSim(const Circuit& ckt,
                                                   const std::vector<int>& topoOrder)
    : ckt_(ckt), topoOrder_(topoOrder),
      sim_(ckt, topoOrder),
      collapsedFrom_(0), collapsedTo_(0)
{
    buildFaultList();
}

// ============================================================
//  BUILD FAULT LIST with collapsing
// ============================================================

void ParallelPatternFaultSim::buildFaultList() {
    collapsedFrom_ = ckt_.totalFaults();

    // Use the FaultCollapser for proper gate-specific collapsing
    FaultCollapser fc(ckt_);
    fc.collapse();

    faults_ = fc.collapsedFaults();
    collapsedTo_ = (int)faults_.size();
}

// ============================================================
//  SIMULATE ONE FAULT (64-bit parallel)
// ============================================================

uint64_t ParallelPatternFaultSim::simulateFault(
    const std::vector<uint64_t>& goodWords,
    const Fault& fault, int packedCount) const
{
    int maxId = (int)goodWords.size();
    if (fault.nodeId >= maxId) return 0ULL;

    // Copy good words
    std::vector<uint64_t> faultyWords(goodWords);

    // Inject fault: force all bits to stuck-at value
    uint64_t mask = (packedCount >= 64) ? ~0ULL : ((1ULL << packedCount) - 1);
    if (fault.stuckAt == 0)
        faultyWords[fault.nodeId] = 0ULL;
    else
        faultyWords[fault.nodeId] = mask;

    // Re-propagate from fault site forward
    for (int nodeId : topoOrder_) {
        if (nodeId == fault.nodeId) continue; // Never overwrite injected fault
        Node* n = ckt_.getNode(nodeId);
        if (!n || n->nodeType == NodeType::INPUT) continue;

        // Check if any input is downstream of fault
        bool needsReeval = false;
        for (int inId : n->inputs) {
            if (inId < 0) continue;
            if (inId < maxId && faultyWords[inId] != goodWords[inId]) {
                needsReeval = true;
                break;
            }
        }
        if (!needsReeval) continue;

        if (n->nodeType == NodeType::BRNH) {
            if (!n->inputs.empty() && n->inputs[0] > 0 && n->inputs[0] < maxId)
                faultyWords[nodeId] = faultyWords[n->inputs[0]];
            continue;
        }

        // Gather input words
        std::vector<uint64_t> inWords;
        inWords.reserve(n->inputs.size());
        for (int inId : n->inputs) {
            if (inId == -1)      inWords.push_back(0ULL);
            else if (inId == -2) inWords.push_back(mask);
            else if (inId >= 0 && inId < maxId)
                inWords.push_back(faultyWords[inId]);
            else
                inWords.push_back(0ULL);
        }

        faultyWords[nodeId] = evalGate64(n->gateType, inWords);
    }

    // Compare outputs: build detection mask
    uint64_t detected = 0ULL;
    for (int oid : ckt_.outputIds) {
        if (oid <= 0 || oid >= maxId) continue;
        detected |= (faultyWords[oid] ^ goodWords[oid]);

        Node* outNode = ckt_.getNode(oid);
        if (outNode) {
            for (int bid : ckt_.branchIds) {
                Node* br = ckt_.getNode(bid);
                if (br && !br->inputs.empty() && br->inputs[0] == oid && bid < maxId) {
                    detected |= (faultyWords[bid] ^ goodWords[bid]);
                }
            }
        }
    }

    return detected & mask;
}

// ============================================================
//  RUN — main simulation loop
// ============================================================

FaultSimResult ParallelPatternFaultSim::run(const std::vector<TestVector>& vectors,
                                             bool trackVectorMapping)
{
    auto tStart = std::chrono::high_resolution_clock::now();

    int numFaults  = (int)faults_.size();
    int numVectors = (int)vectors.size();

    FaultSimResult res;
    res.totalFaults    = numFaults;
    res.collapsedFrom  = collapsedFrom_;
    res.collapsedTo    = collapsedTo_;
    res.faultDetectedBy.resize(numFaults);
    res.vectorDetects.resize(numVectors);

    std::vector<bool> faultDetected(numFaults, false);

    // Process vectors in chunks of WORD_SIZE=64
    for (int base = 0; base < numVectors; base += WORD_SIZE) {
        int count = std::min(WORD_SIZE, numVectors - base);

        // Pack vectors and simulate good circuit
        std::vector<uint64_t> goodWords;
        sim_.applyVectors(vectors, base, count, goodWords);
        sim_.simulate64(goodWords);

        // Simulate each fault
        for (int fi = 0; fi < numFaults; fi++) {
            if (faultDetected[fi]) continue;  // fault dropping

            uint64_t detMask = simulateFault(goodWords, faults_[fi], count);

            if (detMask) {
                faultDetected[fi] = true;
                if (trackVectorMapping) {
                    for (int k = 0; k < count; k++) {
                        if ((detMask >> k) & 1) {
                            int vi = base + k;
                            res.faultDetectedBy[fi].push_back(vi);
                            res.vectorDetects[vi].insert(fi);
                        }
                    }
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
