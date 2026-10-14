// ============================================================
//  simulator.cpp  —  Good-Circuit Logic Simulator
//
//  Single-value simulation: evaluates each gate in topological
//  order using the values of its input nodes.
//
//  64-bit parallel simulation: packs up to 64 test vectors
//  into uint64_t words and evaluates using bitwise operations.
// ============================================================

#include "simulator.h"
#include <algorithm>
#include <iostream>

// ============================================================
//  CONSTRUCTOR
// ============================================================

Simulator::Simulator(const Circuit& ckt, const std::vector<int>& topoOrder)
    : ckt_(ckt), topoOrder_(topoOrder)
{
    values_.resize(ckt.nodeArray.size(), -1);
}

// ============================================================
//  APPLY VECTOR — set PI values
// ============================================================

void Simulator::applyVector(const TestVector& tv) {
    // Reset all values
    std::fill(values_.begin(), values_.end(), -1);

    for (int pid : ckt_.inputIds) {
        Node* n = ckt_.getNode(pid);
        if (!n) continue;
        auto it = tv.find(n->name);
        if (it != tv.end()) {
            values_[pid] = it->second;
            n->value = it->second;
        }
    }
}

// ============================================================
//  EVALUATE GATE — single value
// ============================================================

int Simulator::evaluateGate(const Node* n) const {
    std::vector<int> inVals;
    inVals.reserve(n->inputs.size());

    for (int inId : n->inputs) {
        if (inId == -1) { inVals.push_back(0); continue; }  // GND
        if (inId == -2) { inVals.push_back(1); continue; }  // VCC
        int v = (inId >= 0 && inId < (int)values_.size()) ? values_[inId] : -1;
        inVals.push_back(v);
    }

    // Check for unknown inputs
    bool hasUnknown = false;
    for (int v : inVals) if (v < 0) { hasUnknown = true; break; }

    switch (n->gateType) {
        case GateType::AND: {
            for (int v : inVals) if (v == 0) return 0;
            if (hasUnknown) return -1;
            return 1;
        }
        case GateType::NAND: {
            for (int v : inVals) if (v == 0) return 1;
            if (hasUnknown) return -1;
            return 0;
        }
        case GateType::OR: {
            for (int v : inVals) if (v == 1) return 1;
            if (hasUnknown) return -1;
            return 0;
        }
        case GateType::NOR: {
            for (int v : inVals) if (v == 1) return 0;
            if (hasUnknown) return -1;
            return 1;
        }
        case GateType::XOR: {
            if (hasUnknown) return -1;
            int result = 0;
            for (int v : inVals) result ^= v;
            return result;
        }
        case GateType::XNOR: {
            if (hasUnknown) return -1;
            int result = 0;
            for (int v : inVals) result ^= v;
            return result ^ 1;
        }
        case GateType::NOT: {
            if (inVals.empty() || inVals[0] < 0) return -1;
            return inVals[0] ^ 1;
        }
        case GateType::BUFF: {
            if (inVals.empty() || inVals[0] < 0) return -1;
            return inVals[0];
        }
        default:
            return -1;
    }
}

// ============================================================
//  SIMULATE — single vector
// ============================================================

void Simulator::simulate() {
    for (int nodeId : topoOrder_) {
        Node* n = ckt_.getNode(nodeId);
        if (!n) continue;

        if (n->nodeType == NodeType::INPUT) {
            // Already set by applyVector
            continue;
        }

        if (n->nodeType == NodeType::BRNH) {
            // Branch: copy stem value
            if (!n->inputs.empty() && n->inputs[0] > 0) {
                values_[nodeId] = values_[n->inputs[0]];
            }
        } else {
            // Gate: evaluate
            values_[nodeId] = evaluateGate(n);
        }

        n->value = values_[nodeId];
    }
}

// ============================================================
//  GET VALUE / SNAPSHOT
// ============================================================

int Simulator::getValue(int nodeId) const {
    if (nodeId < 0 || nodeId >= (int)values_.size()) return -1;
    return values_[nodeId];
}

std::vector<int> Simulator::snapshot() const {
    return values_;
}

// ============================================================
//  64-BIT PARALLEL EVALUATION
// ============================================================

uint64_t evalGate64(GateType gt, const std::vector<uint64_t>& inputWords) {
    if (inputWords.empty()) return 0ULL;

    switch (gt) {
        case GateType::AND: {
            uint64_t result = ~0ULL;
            for (auto w : inputWords) result &= w;
            return result;
        }
        case GateType::NAND: {
            uint64_t result = ~0ULL;
            for (auto w : inputWords) result &= w;
            return ~result;
        }
        case GateType::OR: {
            uint64_t result = 0ULL;
            for (auto w : inputWords) result |= w;
            return result;
        }
        case GateType::NOR: {
            uint64_t result = 0ULL;
            for (auto w : inputWords) result |= w;
            return ~result;
        }
        case GateType::XOR: {
            uint64_t result = 0ULL;
            for (auto w : inputWords) result ^= w;
            return result;
        }
        case GateType::XNOR: {
            uint64_t result = 0ULL;
            for (auto w : inputWords) result ^= w;
            return ~result;
        }
        case GateType::NOT:
            return ~inputWords[0];
        case GateType::BUFF:
            return inputWords[0];
        default:
            return 0ULL;
    }
}

// ============================================================
//  APPLY VECTORS (64-bit packing)
// ============================================================

void Simulator::applyVectors(const std::vector<TestVector>& vecs,
                              int start, int count,
                              std::vector<uint64_t>& packedWords) const {
    int maxId = (int)ckt_.nodeArray.size();
    packedWords.assign(maxId, 0ULL);

    for (int k = 0; k < count && (start + k) < (int)vecs.size(); k++) {
        const TestVector& tv = vecs[start + k];
        for (int pid : ckt_.inputIds) {
            Node* n = ckt_.getNode(pid);
            if (!n) continue;
            auto it = tv.find(n->name);
            if (it != tv.end() && it->second == 1)
                packedWords[pid] |= (1ULL << k);
        }
    }
}

// ============================================================
//  SIMULATE 64-BIT
// ============================================================

void Simulator::simulate64(std::vector<uint64_t>& packedWords) const {
    for (int nodeId : topoOrder_) {
        Node* n = ckt_.getNode(nodeId);
        if (!n) continue;

        if (n->nodeType == NodeType::INPUT) continue;

        if (n->nodeType == NodeType::BRNH) {
            if (!n->inputs.empty() && n->inputs[0] > 0)
                packedWords[nodeId] = packedWords[n->inputs[0]];
            continue;
        }

        // Gather input words
        std::vector<uint64_t> inWords;
        inWords.reserve(n->inputs.size());
        for (int inId : n->inputs) {
            if (inId == -1)      inWords.push_back(0ULL);          // GND
            else if (inId == -2) inWords.push_back(~0ULL);         // VCC
            else if (inId >= 0 && inId < (int)packedWords.size())
                inWords.push_back(packedWords[inId]);
            else
                inWords.push_back(0ULL);
        }

        packedWords[nodeId] = evalGate64(n->gateType, inWords);
    }
}

// ============================================================
//  RUN ALL — convenience for Phase 2 verification
// ============================================================

std::vector<SimRecord> runAll(Simulator& sim, const std::vector<TestVector>& vectors) {
    std::vector<SimRecord> records;
    records.reserve(vectors.size());

    for (auto& tv : vectors) {
        sim.applyVector(tv);
        sim.simulate();

        SimRecord rec;
        rec.inputs = tv;
        rec.success = true;
        records.push_back(rec);
    }

    return records;
}
