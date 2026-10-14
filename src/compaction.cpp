// ============================================================
//  compaction.cpp  —  Reverse Compaction (Backward Elimination)
//
//  TRUE REVERSE COMPACTION:
//    Start with the full candidate vector set.
//    Walk backward from the last vector to the first.
//    Try removing each vector; if the remaining set still
//    covers every detectable fault, permanently drop it.
//    Result: minimal set that preserves full fault coverage.
// ============================================================

#include "compaction.h"
#include <iostream>
#include <iomanip>
#include <algorithm>

// ============================================================
//  REVERSE COMPACTION (backward elimination)
// ============================================================

CompactionResult reverseCompact(const std::vector<TestVector>& vectors,
                                const FaultSimResult& fsResult)
{
    int numVectors = (int)vectors.size();
    int numFaults  = fsResult.totalFaults;

    const auto& faultDetectedBy = fsResult.faultDetectedBy;
    const auto& vectorDetects   = fsResult.vectorDetects;

    // ---- Identify detectable faults ----
    std::vector<int> detectable;
    detectable.reserve(numFaults);
    for (int fi = 0; fi < numFaults; fi++)
        if (!faultDetectedBy[fi].empty())
            detectable.push_back(fi);

    // ---- Build initial candidate set ----
    // Start with all vectors that detect at least one fault
    std::vector<int> candidates;
    candidates.reserve(numVectors);
    for (int vi = 0; vi < numVectors; vi++)
        if (!vectorDetects[vi].empty())
            candidates.push_back(vi);

    // ---- Backward elimination ----
    for (int i = (int)candidates.size() - 1; i >= 0; i--) {
        // Check if removing candidates[i] still covers all detectable faults
        std::vector<bool> cover(numFaults, false);
        for (int j = 0; j < (int)candidates.size(); j++) {
            if (j == i) continue;
            for (int fi : vectorDetects[candidates[j]])
                cover[fi] = true;
        }

        bool allCovered = true;
        for (int fi : detectable) {
            if (!cover[fi]) { allCovered = false; break; }
        }

        if (allCovered)
            candidates.erase(candidates.begin() + i);
    }

    // ---- Build result ----
    std::vector<TestVector> compacted;
    compacted.reserve(candidates.size());
    for (int vi : candidates)
        compacted.push_back(vectors[vi]);

    // Count faults covered by compacted set
    std::vector<bool> finalCover(numFaults, false);
    for (int vi : candidates)
        for (int fi : vectorDetects[vi])
            finalCover[fi] = true;

    int covered = 0;
    for (int fi = 0; fi < numFaults; fi++)
        if (finalCover[fi]) covered++;

    CompactionResult cr;
    cr.keptIndices     = candidates;
    cr.compactedVectors = compacted;
    cr.originalCount   = numVectors;
    cr.compactedCount  = (int)candidates.size();
    cr.faultsCovered   = covered;
    cr.totalFaults     = numFaults;
    cr.reductionPct    = numVectors > 0
        ? 100.0 * (numVectors - (int)candidates.size()) / numVectors : 0.0;
    cr.coveragePct     = numFaults > 0
        ? 100.0 * covered / numFaults : 0.0;

    return cr;
}

// ============================================================
//  PRINT COMPACTION REPORT
// ============================================================

void printCompactionReport(const CompactionResult& cr, const Circuit& ckt) {
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "\n  ---- Reverse Compaction Result ----\n";
    std::cout << "  Original vectors   : " << cr.originalCount << "\n";
    std::cout << "  Compacted vectors  : " << cr.compactedCount << "\n";
    std::cout << "  Reduction          : " << cr.reductionPct << "%\n";
    std::cout << "  Faults covered     : " << cr.faultsCovered
              << " / " << cr.totalFaults << "\n";
    std::cout << "  Fault coverage     : " << cr.coveragePct << "%\n";

    // Print the compacted test vectors
    std::vector<std::string> inputNames;
    for (int id : ckt.inputIds) {
        Node* n = ckt.getNode(id);
        if (n) inputNames.push_back(n->name);
    }

    std::cout << "\n  Compacted test vectors (" << cr.compactedCount << "):\n";
    std::cout << "    " << std::setw(6) << "Vec#";
    for (auto& nm : inputNames) std::cout << std::setw(5) << nm;
    std::cout << "\n    " << std::string(6 + (int)inputNames.size() * 5, '-') << "\n";

    for (int i = 0; i < (int)cr.keptIndices.size(); i++) {
        int vi = cr.keptIndices[i];
        const TestVector& tv = cr.compactedVectors[i];
        std::cout << "    " << std::setw(5) << ("V" + std::to_string(vi));
        for (auto& nm : inputNames) {
            auto it = tv.find(nm);
            std::cout << std::setw(5) << (it != tv.end() ? std::to_string(it->second) : "?");
        }
        std::cout << "\n";
    }
}
