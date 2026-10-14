#pragma once

// ============================================================
//  testvector.h  —  Test Vector Generation
//
//  Provides:
//    genExhaustive  — all 2^n vectors (n ≤ 20)
//    genRandom      — pseudo-random vectors with seed
//    genForFaultSim — smart: exhaustive if small, random if large
//    genSweep       — batches of increasing size for DSE
// ============================================================

#include "circuit.h"
#include "simulator.h"

#include <vector>
#include <string>
#include <cstdint>

// Generate all 2^n input combinations (throws if n > 20)
std::vector<TestVector> genExhaustive(const Circuit& ckt);

// Generate count random vectors with reproducible seed
std::vector<TestVector> genRandom(const Circuit& ckt, int count, uint32_t seed = 42);

// Smart generator: exhaustive if n ≤ 16, random otherwise
std::vector<TestVector> genForFaultSim(const Circuit& ckt);

// Verification vectors (same as genForFaultSim)
std::vector<TestVector> genForVerif(const Circuit& ckt);

// Sweep batches for DSE
struct SweepBatch {
    int count;
    std::vector<TestVector> vectors;
};
std::vector<SweepBatch> genSweep(const Circuit& ckt, const std::vector<int>& sizes);
