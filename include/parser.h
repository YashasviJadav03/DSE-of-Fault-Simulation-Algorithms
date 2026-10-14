#pragma once

// ============================================================
//  parser.h  —  Structural Verilog Gate-Level Netlist Parser
// ============================================================

#include "circuit.h"
#include <string>
#include <vector>

// Parse a netlist file and populate the Circuit object.
// Returns true on success, false on failure.
bool parseNetlist(const std::string& filename, Circuit& ckt);

// Utility: find a file by basename, searching CWD and common paths.
std::string findFile(const std::string& basename);

// Topological sort: returns node IDs in level order for simulation.
std::vector<int> topologicalSort(const Circuit& ckt);
