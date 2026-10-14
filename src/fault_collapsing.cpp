// ============================================================
//  fault_collapsing.cpp  —  Structural Fault Collapsing
//
//  ORDER: DOMINANCE first, then EQUIVALENCE
//
//  Gate-specific dominance rules (checkpoint theorem):
//    AND  : output SA1 dominated → remove
//    NAND : output SA0 dominated → remove
//    OR   : output SA0 dominated → remove
//    NOR  : output SA1 dominated → remove
//    NOT  : output SA0 and SA1 dominated → remove both
//    BUFF : output SA0 and SA1 dominated → remove both
//
//  Gate-specific equivalence rules (on survivors):
//    AND/NAND : surviving input SA0s equivalent → keep one
//    OR/NOR   : surviving input SA1s equivalent → keep one
//    NOT      : input SA0 ≡ output SA1 (if both survive)
//    BUFF     : input SA0 ≡ output SA0 (if both survive)
// ============================================================

#include "fault_collapsing.h"

#include <iostream>
#include <iomanip>
#include <algorithm>
#include <unordered_set>

// ============================================================
//  CONSTRUCTOR
// ============================================================

FaultCollapser::FaultCollapser(const Circuit& ckt)
    : ckt_(ckt)
{
    result_ = {};
}

// ============================================================
//  BUILD ORIGINAL FAULT LIST
// ============================================================

void FaultCollapser::buildOriginalFaultList() {
    original_.clear();

    auto addFaults = [&](int id) {
        Node* n = ckt_.getNode(id);
        if (!n) return;
        original_.push_back({ id, n->name, 0 });  // SA0
        original_.push_back({ id, n->name, 1 });  // SA1
    };

    for (int id : ckt_.inputIds)  addFaults(id);
    for (int id : ckt_.gateIds)   addFaults(id);
    for (int id : ckt_.branchIds) addFaults(id);
}

// ============================================================
//  FIND FAULT INDEX IN original_
// ============================================================

int FaultCollapser::findFault(int nodeId, int stuckAt) const {
    for (int i = 0; i < (int)original_.size(); i++) {
        if (original_[i].nodeId == nodeId && original_[i].stuckAt == stuckAt)
            return i;
    }
    return -1;
}

// ============================================================
//  STEP 1 — DOMINANCE (Checkpoint Theorem)
//
//  Gate-specific rules: remove the DOMINATED output fault.
//    AND  : output SA1 is dominated by each input SA1 → remove output SA1
//    NAND : output SA0 is dominated by each input SA0 → remove output SA0
//    OR   : output SA0 is dominated by each input SA0 → remove output SA0
//    NOR  : output SA1 is dominated by each input SA1 → remove output SA1
//    NOT  : output SA0 dominated by input SA1, output SA1 dominated by input SA0
//    BUFF : output SA0 dominated by input SA0, output SA1 dominated by input SA1
//    XOR/XNOR: no structural dominance
// ============================================================

std::vector<bool> FaultCollapser::applyDominance() {
    int n = (int)original_.size();
    std::vector<bool> survivors(n, true);

    for (int gid : ckt_.gateIds) {
        Node* g = ckt_.getNode(gid);
        if (!g) continue;

        switch (g->gateType) {
            case GateType::AND: {
                // output SA1 dominated by input SA1s → remove output SA1
                int idx = findFault(gid, 1);  // output SA1
                if (idx >= 0) survivors[idx] = false;
                break;
            }
            case GateType::NAND: {
                // output SA0 dominated by input SA0s → remove output SA0
                int idx = findFault(gid, 0);
                if (idx >= 0) survivors[idx] = false;
                break;
            }
            case GateType::OR: {
                // output SA0 dominated by input SA0s → remove output SA0
                int idx = findFault(gid, 0);
                if (idx >= 0) survivors[idx] = false;
                break;
            }
            case GateType::NOR: {
                // output SA1 dominated by input SA1s → remove output SA1
                int idx = findFault(gid, 1);
                if (idx >= 0) survivors[idx] = false;
                break;
            }
            case GateType::NOT: {
                // output SA0 dominated by input SA1 → remove output SA0
                // output SA1 dominated by input SA0 → remove output SA1
                int idx0 = findFault(gid, 0);
                int idx1 = findFault(gid, 1);
                if (idx0 >= 0) survivors[idx0] = false;
                if (idx1 >= 0) survivors[idx1] = false;
                break;
            }
            case GateType::BUFF: {
                // output SA0 dominated by input SA0 → remove output SA0
                // output SA1 dominated by input SA1 → remove output SA1
                int idx0 = findFault(gid, 0);
                int idx1 = findFault(gid, 1);
                if (idx0 >= 0) survivors[idx0] = false;
                if (idx1 >= 0) survivors[idx1] = false;
                break;
            }
            default:
                // XOR, XNOR: no structural dominance
                break;
        }
    }

    return survivors;
}

// ============================================================
//  STEP 2 — EQUIVALENCE (on dominance survivors)
//
//  For each gate, among its SURVIVING input faults of the
//  controlling stuck-at type, keep ONE and remove the rest:
//    AND/NAND : keep first surviving input SA0, remove others
//    OR/NOR   : keep first surviving input SA1, remove others
//    NOT      : if input SA0 survives AND output SA1 survives → remove input SA0
//               if input SA1 survives AND output SA0 survives → remove input SA1
//    BUFF     : if input SA0 survives AND output SA0 survives → remove input SA0
//               if input SA1 survives AND output SA1 survives → remove input SA1
// ============================================================

std::vector<bool> FaultCollapser::applyEquivalence(const std::vector<bool>& domSurvivors) {
    (void)original_;
    std::vector<bool> survivors(domSurvivors);

    for (int gid : ckt_.gateIds) {
        Node* g = ckt_.getNode(gid);
        if (!g) continue;

        switch (g->gateType) {
            case GateType::AND:
            case GateType::NAND: {
                // Among surviving input SA0s, keep first, remove rest
                bool keptOne = false;
                for (int inId : g->inputs) {
                    if (inId <= 0) continue;
                    int idx = findFault(inId, 0);  // input SA0
                    if (idx < 0 || !survivors[idx]) continue;
                    if (!keptOne) { keptOne = true; continue; }
                    survivors[idx] = false;  // remove extra
                }
                break;
            }
            case GateType::OR:
            case GateType::NOR: {
                // Among surviving input SA1s, keep first, remove rest
                bool keptOne = false;
                for (int inId : g->inputs) {
                    if (inId <= 0) continue;
                    int idx = findFault(inId, 1);  // input SA1
                    if (idx < 0 || !survivors[idx]) continue;
                    if (!keptOne) { keptOne = true; continue; }
                    survivors[idx] = false;
                }
                break;
            }
            case GateType::NOT: {
                // input SA0 ≡ output SA1: if output SA1 survived, remove input SA0
                // input SA1 ≡ output SA0: if output SA0 survived, remove input SA1
                if (!g->inputs.empty() && g->inputs[0] > 0) {
                    int inId = g->inputs[0];
                    int inSA0 = findFault(inId, 0);
                    int inSA1 = findFault(inId, 1);
                    int outSA0 = findFault(gid, 0);
                    int outSA1 = findFault(gid, 1);

                    if (inSA0 >= 0 && outSA1 >= 0 && survivors[inSA0] && survivors[outSA1])
                        survivors[inSA0] = false;
                    if (inSA1 >= 0 && outSA0 >= 0 && survivors[inSA1] && survivors[outSA0])
                        survivors[inSA1] = false;
                }
                break;
            }
            case GateType::BUFF: {
                // input SA0 ≡ output SA0: if output SA0 survived, remove input SA0
                // input SA1 ≡ output SA1: if output SA1 survived, remove input SA1
                if (!g->inputs.empty() && g->inputs[0] > 0) {
                    int inId = g->inputs[0];
                    int inSA0 = findFault(inId, 0);
                    int inSA1 = findFault(inId, 1);
                    int outSA0 = findFault(gid, 0);
                    int outSA1 = findFault(gid, 1);

                    if (inSA0 >= 0 && outSA0 >= 0 && survivors[inSA0] && survivors[outSA0])
                        survivors[inSA0] = false;
                    if (inSA1 >= 0 && outSA1 >= 0 && survivors[inSA1] && survivors[outSA1])
                        survivors[inSA1] = false;
                }
                break;
            }
            default:
                break;  // XOR/XNOR: no equivalence
        }
    }

    return survivors;
}

// ============================================================
//  COLLAPSE — full pipeline
// ============================================================

void FaultCollapser::collapse() {
    buildOriginalFaultList();

    int origCount = (int)original_.size();
    result_.originalFaultCount = origCount;

    // Step 1: Dominance
    auto domSurvivors = applyDominance();
    int domRemoved = 0;
    for (int i = 0; i < origCount; i++)
        if (!domSurvivors[i]) domRemoved++;

    // Step 2: Equivalence
    auto finalSurvivors = applyEquivalence(domSurvivors);
    int equivRemoved = 0;
    for (int i = 0; i < origCount; i++)
        if (domSurvivors[i] && !finalSurvivors[i]) equivRemoved++;

    result_.dominanceReductions = domRemoved;
    result_.equivalenceReductions = equivRemoved;

    // Build collapsed fault list and mapping
    collapsed_.clear();
    result_.originalToCollapsed.assign(origCount, -1);

    for (int i = 0; i < origCount; i++) {
        if (finalSurvivors[i]) {
            result_.originalToCollapsed[i] = (int)collapsed_.size();
            collapsed_.push_back(original_[i]);
        }
    }

    result_.collapsedFaultCount = (int)collapsed_.size();
    result_.reductionPct = origCount > 0
        ? 100.0 * (origCount - result_.collapsedFaultCount) / origCount : 0.0;
    result_.collapseRatio = origCount > 0
        ? (double)result_.collapsedFaultCount / origCount : 0.0;
}

// ============================================================
//  PRINT REPORT
// ============================================================

void FaultCollapser::printReport() const {
    std::cout << "\n  [Fault Collapsing]  (Dominance first -> Equivalence)\n";
    std::cout << "  ─────────────────────────────────────────────────────\n";
    std::cout << "  Original fault count     : " << result_.originalFaultCount << "\n";
    std::cout << "  After dominance          : "
              << (result_.originalFaultCount - result_.dominanceReductions) << "\n";
    std::cout << "  After equivalence        : " << result_.collapsedFaultCount << "\n";
    std::cout << "  ─────────────────────────────────────────────────────\n";
    std::cout << "  Dominance reductions     : " << result_.dominanceReductions << "\n";
    std::cout << "  Equivalence reductions   : " << result_.equivalenceReductions << "\n";
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "  Total reduction          : " << result_.reductionPct << "%\n";
    std::cout << "  Collapse Ratio (CR)      : " << result_.collapseRatio << "\n";

    // Print active fault list
    std::cout << "\n  Active (collapsed) fault list — "
              << result_.collapsedFaultCount << " faults:\n";
    for (auto& f : collapsed_) {
        std::cout << "    " << f.nodeName << " SA" << f.stuckAt << "\n";
    }
}
