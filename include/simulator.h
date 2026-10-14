#pragma once

// ============================================================
//  simulator.h  —  Good-Circuit Logic Simulator
//
//  Provides:
//    Simulator class  — single-vector and 64-bit parallel simulation
//    SimRecord        — record of one simulation run
//    TestVector       — alias for input name → value map
//    evalGate64       — free function for 64-bit gate evaluation
// ============================================================

#include "circuit.h"

#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>

// ---- Test Vector ----
using TestVector = std::unordered_map<std::string, int>;

// ---- Simulation Record (one vector's results) ----
struct SimRecord {
    std::unordered_map<std::string, int> inputs;
    std::unordered_map<std::string, int> outputs;
    bool success;
};

// ---- Simulator Class ----
class Simulator {
public:
    Simulator(const Circuit& ckt, const std::vector<int>& topoOrder);

    // Apply a test vector to circuit inputs
    void applyVector(const TestVector& tv);

    // Simulate the circuit once (single-value)
    void simulate();

    // Get value of a node after simulation
    int getValue(int nodeId) const;

    // Collect all node values into a snapshot vector
    std::vector<int> snapshot() const;

    // 64-bit parallel: pack W vectors, simulate, return packed words
    void applyVectors(const std::vector<TestVector>& vecs, int start, int count,
                      std::vector<uint64_t>& packedWords) const;
    void simulate64(std::vector<uint64_t>& packedWords) const;

private:
    const Circuit&          ckt_;
    const std::vector<int>& topoOrder_;

    // Local value storage for single-vector simulation
    std::vector<int> values_;

    int evaluateGate(const Node* n) const;
};

// ---- Free function: 64-bit gate evaluation ----
uint64_t evalGate64(GateType gt, const std::vector<uint64_t>& inputWords);

// ---- Convenience: run all vectors, return SimRecords ----
std::vector<SimRecord> runAll(Simulator& sim, const std::vector<TestVector>& vectors);
