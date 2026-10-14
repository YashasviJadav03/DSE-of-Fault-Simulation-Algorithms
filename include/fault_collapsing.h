#pragma once

// ============================================================
//  fault_collapsing.h  —  Structural Fault Collapsing
//
//  ORDER: DOMINANCE first, then EQUIVALENCE
//
//  1. FAULT DOMINANCE (Checkpoint Theorem):
//     Gate-specific rules for which output faults are dominated:
//       AND  : output SA1 dominated by each input SA1 → remove output SA1
//       NAND : output SA0 dominated by each input SA0 → remove output SA0
//       OR   : output SA0 dominated by each input SA0 → remove output SA0
//       NOR  : output SA1 dominated by each input SA1 → remove output SA1
//       NOT  : output SA0 dominated by input SA1 → remove output SA0
//              output SA1 dominated by input SA0 → remove output SA1
//       BUFF : output SA0 dominated by input SA0 → remove output SA0
//              output SA1 dominated by input SA1 → remove output SA1
//
//  2. FAULT EQUIVALENCE (on survivors only):
//       AND  : surviving input SA0s equivalent → keep one
//       NAND : surviving input SA0s equivalent → keep one
//       OR   : surviving input SA1s equivalent → keep one
//       NOR  : surviving input SA1s equivalent → keep one
//       NOT  : input SA0 ≡ output SA1, input SA1 ≡ output SA0
//       BUFF : input SA0 ≡ output SA0, input SA1 ≡ output SA1
//       XOR/XNOR: no equivalence
// ============================================================

#include "circuit.h"

#include <vector>
#include <string>
#include <unordered_map>

// ---- Equivalence Class ----

struct EquivClass {
    std::vector<Fault> members;
    int representativeIndex;  // index into original fault list
};

// ---- Fault Collapsing Result ----

struct FaultCollapsingResult {
    int originalFaultCount;
    int collapsedFaultCount;    // number that survived
    int dominanceReductions;    // removed in step 1
    int equivalenceReductions;  // removed in step 2

    double reductionPct;   // (1 - collapsed/original) * 100
    double collapseRatio;  // collapsed / original

    // Maps original fault index → collapsed list index (-1 = removed)
    std::vector<int> originalToCollapsed;
};

// ---- Fault Collapser Class ----

class FaultCollapser {
public:
    explicit FaultCollapser(const Circuit& ckt);

    // Run the full collapsing pipeline
    void collapse();

    // ---- Query ----
    const FaultCollapsingResult& result() const { return result_; }
    const std::vector<Fault>& originalFaults() const { return original_; }
    const std::vector<Fault>& collapsedFaults() const { return collapsed_; }
    const std::vector<EquivClass>& equivClasses() const { return equivClasses_; }

    // Print a detailed report
    void printReport() const;

private:
    const Circuit& ckt_;
    std::vector<Fault> original_;
    std::vector<Fault> collapsed_;
    std::vector<EquivClass> equivClasses_;
    FaultCollapsingResult result_;

    void buildOriginalFaultList();
    std::vector<bool> applyDominance();
    std::vector<bool> applyEquivalence(const std::vector<bool>& domSurvivors);
    int findFault(int nodeId, int stuckAt) const;
};
