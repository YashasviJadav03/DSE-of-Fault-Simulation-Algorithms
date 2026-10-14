#pragma once

// ============================================================
//  circuit.h  —  Unified Circuit Data Structures
//
//  Provides:
//    Node       — represents a wire/gate in the circuit
//    Circuit    — 1D array of linked-list nodes
//    Fault      — SA0/SA1 descriptor on a specific wire
//    NodeType   — INPUT, OUTPUT, GATE, BRNH
//    GateType   — AND, OR, NOT, XOR, NAND, NOR, XNOR, BUFF
// ============================================================

#include <string>
#include <vector>
#include <unordered_map>
#include <set>

// ---- Enums ----

enum class NodeType { INPUT, OUTPUT, GATE, BRNH };

enum class GateType { NONE, AND, OR, NOT, XOR, NAND, NOR, XNOR, BUFF };

// ---- Node (linked list node) ----

struct Node {
    int              id;
    std::string      name;
    NodeType         nodeType;
    GateType         gateType;
    std::vector<int> inputs;   // IDs of driving nodes
    std::vector<int> outputs;  // IDs of nodes this feeds
    bool             faultSA0; // stuck-at-0 fault site
    bool             faultSA1; // stuck-at-1 fault site
    int              value;    // logic value during simulation (-1 = unknown)
    int              level;    // topological level (-1 = unassigned)
    Node*            next;     // next node in linked list chain

    Node(int _id, const std::string& _name, NodeType _nt, GateType _gt = GateType::NONE)
        : id(_id), name(_name), nodeType(_nt), gateType(_gt),
          faultSA0(true), faultSA1(true), value(-1), level(-1), next(nullptr) {}
};

// ---- Fault Descriptor ----

struct Fault {
    int         nodeId;
    std::string nodeName;
    int         stuckAt;   // 0 = SA0, 1 = SA1
};

// ---- Circuit (1D array of linked lists) ----

struct Circuit {
    std::string name;

    // 1D array indexed by wire ID — each slot is the head of a linked list
    std::vector<Node*> nodeArray;

    // Wire name → node ID lookup
    std::unordered_map<std::string, int> nameToId;

    // Categorised ID lists
    std::vector<int> inputIds;
    std::vector<int> outputIds;
    std::vector<int> gateIds;
    std::vector<int> branchIds;

    int nextId = 1;

    Circuit(const std::string& _name = "circuit") : name(_name) {
        nodeArray.push_back(nullptr); // index 0 unused
    }

    ~Circuit() {
        for (auto* n : nodeArray) {
            while (n) { Node* tmp = n->next; delete n; n = tmp; }
        }
    }

    // Non-copyable (owns heap Nodes)
    Circuit(const Circuit&) = delete;
    Circuit& operator=(const Circuit&) = delete;

    int getOrCreateId(const std::string& wireName) {
        auto it = nameToId.find(wireName);
        if (it != nameToId.end()) return it->second;
        int id = nextId++;
        nameToId[wireName] = id;
        nodeArray.push_back(nullptr);
        return id;
    }

    Node* getNode(int id) const {
        if (id < 0 || id >= (int)nodeArray.size()) return nullptr;
        return nodeArray[id];
    }

    Node* getNodeByName(const std::string& n) const {
        auto it = nameToId.find(n);
        return (it == nameToId.end()) ? nullptr : getNode(it->second);
    }

    void addNode(Node* n) {
        if (n->id >= (int)nodeArray.size())
            nodeArray.resize(n->id + 1, nullptr);
        if (!nodeArray[n->id]) {
            nodeArray[n->id] = n;
        } else {
            Node* cur = nodeArray[n->id];
            while (cur->next) cur = cur->next;
            cur->next = n;
        }
    }

    // Total nodes (excluding pure output aliases)
    // Output nodes are NOT counted separately — an output wire is
    // really the gate output or PI that drives it.
    int totalNodes()  const {
        return (int)(inputIds.size() + outputIds.size()
                     + gateIds.size() + branchIds.size());
    }

    // Total stuck-at faults = 2 × (PIs + gates + branches)
    int totalFaults() const {
        return 2 * (int)(inputIds.size() + gateIds.size() + branchIds.size());
    }
};
