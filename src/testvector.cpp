// ============================================================
//  testvector.cpp  —  Test Vector Generation
// ============================================================

#include "testvector.h"
#include <algorithm>
#include <random>
#include <stdexcept>
#include <set>

// ---- Helper: get input names in stable circuit order ----
static std::vector<std::string> getInputNames(const Circuit& ckt) {
    std::vector<std::string> names;
    names.reserve(ckt.inputIds.size());
    for (int id : ckt.inputIds) {
        Node* n = ckt.getNode(id);
        if (n) names.push_back(n->name);
    }
    return names;
}

// ---- Helper: build one vector from a bitmask ----
static TestVector maskToVec(const std::vector<std::string>& names, long long mask) {
    int n = (int)names.size();
    TestVector tv;
    tv.reserve(n);
    for (int i = 0; i < n; i++)
        tv[names[i]] = (int)((mask >> (n - 1 - i)) & 1);
    return tv;
}

// ============================================================
//  EXHAUSTIVE — all 2^n combinations
// ============================================================

std::vector<TestVector> genExhaustive(const Circuit& ckt) {
    auto names = getInputNames(ckt);
    int  n     = (int)names.size();

    if (n > 20)
        throw std::runtime_error(
            "[genExhaustive] " + std::to_string(n) +
            " inputs → 2^n too large. Use genForFaultSim() instead.");

    long long total = 1LL << n;
    std::vector<TestVector> vecs;
    vecs.reserve((size_t)total);
    for (long long mask = 0; mask < total; mask++)
        vecs.push_back(maskToVec(names, mask));
    return vecs;
}

// ============================================================
//  RANDOM — pseudo-random with seed
// ============================================================

std::vector<TestVector> genRandom(const Circuit& ckt, int count, uint32_t seed) {
    auto names = getInputNames(ckt);
    int  n     = (int)names.size();

    std::mt19937 rng(seed);
    std::uniform_int_distribution<int> dist(0, 1);

    // Use a set to avoid duplicates
    std::set<std::vector<int>> seen;
    std::vector<TestVector> vecs;
    vecs.reserve(count);

    int attempts = 0;
    int maxAttempts = count * 10;

    while ((int)vecs.size() < count && attempts < maxAttempts) {
        attempts++;
        std::vector<int> bits(n);
        for (int i = 0; i < n; i++) bits[i] = dist(rng);

        if (seen.count(bits)) continue;
        seen.insert(bits);

        TestVector tv;
        tv.reserve(n);
        for (int i = 0; i < n; i++)
            tv[names[i]] = bits[i];
        vecs.push_back(tv);
    }

    return vecs;
}

// ============================================================
//  SMART GENERATOR — exhaustive if small, random if large
// ============================================================

std::vector<TestVector> genForFaultSim(const Circuit& ckt) {
    int n = (int)ckt.inputIds.size();
    if (n <= 16) {
        return genExhaustive(ckt);
    } else {
        // Use a reasonable budget: 2048 random vectors
        return genRandom(ckt, 2048, 42);
    }
}

std::vector<TestVector> genForVerif(const Circuit& ckt) {
    return genForFaultSim(ckt);
}

// ============================================================
//  SWEEP — batches of increasing size for DSE
// ============================================================

std::vector<SweepBatch> genSweep(const Circuit& ckt, const std::vector<int>& sizes) {
    auto fullSet = genForFaultSim(ckt);
    int maxVecs = (int)fullSet.size();

    std::vector<SweepBatch> batches;
    for (int s : sizes) {
        int actual = std::min(s, maxVecs);
        SweepBatch b;
        b.count = actual;
        b.vectors.assign(fullSet.begin(), fullSet.begin() + actual);
        batches.push_back(b);
    }
    return batches;
}
