// ============================================================
//  parser.cpp  —  Netlist Parser Implementation
//  Supports: Verilog gate-level AND .bench format
//  Handles ANSI-style and non-ANSI-style port declarations.
// ============================================================

#include "parser.h"

#include <fstream>
#include <sstream>
#include <algorithm>
#include <regex>
#include <iostream>
#include <set>
#include <unordered_map>
#include <queue>

// ============================================================
//  INTERNAL HELPERS
// ============================================================

static std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

static std::string toUpper(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), ::toupper);
    return s;
}

static GateType stringToGate(const std::string& s) {
    std::string u = toUpper(s);
    if (u == "AND")              return GateType::AND;
    if (u == "OR")               return GateType::OR;
    if (u == "NOT")              return GateType::NOT;
    if (u == "XOR")              return GateType::XOR;
    if (u == "NAND")             return GateType::NAND;
    if (u == "NOR")              return GateType::NOR;
    if (u == "XNOR")             return GateType::XNOR;
    if (u == "BUFF" || u == "BUF") return GateType::BUFF;
    return GateType::NONE;
}

static std::vector<std::string> split(const std::string& s, char delim) {
    std::vector<std::string> result;
    std::stringstream ss(s);
    std::string token;
    while (std::getline(ss, token, delim)) {
        token = trim(token);
        if (!token.empty()) result.push_back(token);
    }
    return result;
}

// ============================================================
//  SHARED POST-PROCESSING
// ============================================================

// Insert BRNH nodes wherever a wire fans out to 2+ gates.
static void addBranchNodes(Circuit& ckt) {
    std::unordered_map<int, std::vector<int>> fanoutMap;
    for (int gid : ckt.gateIds) {
        Node* g = ckt.getNode(gid);
        if (!g) continue;
        for (int inId : g->inputs)
            if (inId > 0) fanoutMap[inId].push_back(gid);
    }

    for (auto& kv : fanoutMap) {
        int wireId = kv.first;
        auto& consumers = kv.second;
        if ((int)consumers.size() <= 1) continue;
        Node* stem = ckt.getNode(wireId);
        if (!stem) continue;

        for (size_t i = 0; i < consumers.size(); i++) {
            std::string brName = stem->name + "_br" + std::to_string(i + 1);
            int brId = ckt.getOrCreateId(brName);

            Node* brn = new Node(brId, brName, NodeType::BRNH, GateType::BUFF);
            brn->inputs.push_back(wireId);
            ckt.addNode(brn);
            ckt.branchIds.push_back(brId);

            // Redirect this consumer to use the branch node
            Node* g = ckt.getNode(consumers[i]);
            if (g) {
                for (int& inp : g->inputs) {
                    if (inp == wireId) { inp = brId; break; }
                }
            }
        }
    }
}

// Build output connections: for each node, record which nodes it feeds.
static void buildOutputConnections(Circuit& ckt) {
    for (int gid : ckt.gateIds) {
        Node* g = ckt.getNode(gid);
        if (!g) continue;
        for (int inId : g->inputs) {
            if (inId > 0) {
                Node* src = ckt.getNode(inId);
                if (src) src->outputs.push_back(gid);
            }
        }
    }
    for (int bid : ckt.branchIds) {
        Node* b = ckt.getNode(bid);
        if (!b) continue;
        for (int inId : b->inputs) {
            if (inId > 0) {
                Node* src = ckt.getNode(inId);
                if (src) src->outputs.push_back(bid);
            }
        }
    }
}

// ============================================================
//  VERILOG GATE-LEVEL PARSER
// ============================================================

class VerilogParser {
public:
    static bool parse(const std::string& filename, Circuit& ckt) {
        std::ifstream f(filename);
        if (!f.is_open()) {
            std::cerr << "[ERROR] Cannot open: " << filename << "\n";
            return false;
        }

        std::string fullText, line;
        while (std::getline(f, line)) {
            size_t cm = line.find("//");
            if (cm != std::string::npos) line = line.substr(0, cm);
            fullText += " " + line;
        }
        f.close();

        // Extract module name
        std::regex modRe(R"(module\s+(\w+)\s*\()");
        std::smatch m;
        if (std::regex_search(fullText, m, modRe))
            ckt.name = m[1].str();

        parsePortDeclarations(fullText, ckt);
        parseGateInstances(fullText, ckt);
        addBranchNodes(ckt);
        buildOutputConnections(ckt);
        return true;
    }

private:
    static void parsePortDeclarations(const std::string& text, Circuit& ckt) {
        std::set<int> seenIds;

        auto registerWire = [&](const std::string& kind, const std::string& rawName) {
            std::string wname = trim(rawName);
            if (wname.empty()) return;
            if (wname == "input" || wname == "output" || wname == "wire") return;
            for (char c : wname)
                if (!isalnum(c) && c != '_') return;

            int id = ckt.getOrCreateId(wname);
            if (seenIds.count(id)) return;
            seenIds.insert(id);

            if (kind == "input") {
                Node* n = new Node(id, wname, NodeType::INPUT);
                ckt.addNode(n);
                ckt.inputIds.push_back(id);
            } else if (kind == "output") {
                Node* n = new Node(id, wname, NodeType::OUTPUT);
                ckt.addNode(n);
                ckt.outputIds.push_back(id);
            }
        };

        // --- ANSI style: inside module( ... ); ---
        {
            size_t modPos = text.find("module");
            if (modPos == std::string::npos) modPos = text.find("MODULE");
            if (modPos != std::string::npos) {
                size_t lp = text.find('(', modPos);
                size_t rp = text.find(");", modPos);
                if (lp != std::string::npos && rp != std::string::npos && lp < rp) {
                    std::string portList = text.substr(lp + 1, rp - lp - 1);
                    std::string curKind;
                    std::stringstream ss(portList);
                    std::string token;
                    while (std::getline(ss, token, ',')) {
                        token = trim(token);
                        std::string low = token;
                        std::transform(low.begin(), low.end(), low.begin(), ::tolower);
                        for (const std::string& kw : {"input", "output", "wire"}) {
                            if (low.substr(0, kw.size()) == kw &&
                                (low.size() == kw.size() || !isalnum(low[kw.size()]))) {
                                curKind = kw;
                                token = trim(token.substr(kw.size()));
                                if (!token.empty() && token[0] == '[') {
                                    size_t rb = token.find(']');
                                    if (rb != std::string::npos)
                                        token = trim(token.substr(rb + 1));
                                }
                                break;
                            }
                        }
                        if (!curKind.empty() && !token.empty())
                            registerWire(curKind, token);
                    }
                }
            }
        }

        // --- Non-ANSI style: input/output/wire lines in module body ---
        {
            size_t modPos = text.find("module");
            if (modPos == std::string::npos) modPos = text.find("MODULE");
            std::string body = text;
            if (modPos != std::string::npos) {
                size_t closePos = text.find(");", modPos);
                if (closePos != std::string::npos)
                    body = text.substr(closePos + 2);
            }
            std::regex portRe(R"(\b(input|output|wire)\b\s*(?:\[\d+:\d+\]\s*)?([^;)]+);)");
            auto begin = std::sregex_iterator(body.begin(), body.end(), portRe);
            auto end   = std::sregex_iterator();
            for (auto it = begin; it != end; ++it) {
                std::string kind  = (*it)[1].str();
                std::string names = (*it)[2].str();
                std::regex bwRe(R"(\[\d+:\d+\]\s*)");
                names = std::regex_replace(names, bwRe, "");
                for (auto& t : split(names, ','))
                    registerWire(kind, t);
            }
        }
    }

    static void parseGateInstances(const std::string& text, Circuit& ckt) {
        std::regex gateRe(
            R"(\b(AND|OR|NOT|XOR|NAND|NOR|XNOR|BUFF|BUF|nand|and|or|not|xor|nor|xnor|buff|buf)\b\s*(\w+\s*)?\(([^)]+)\)\s*;)"
        );
        auto begin = std::sregex_iterator(text.begin(), text.end(), gateRe);
        auto end   = std::sregex_iterator();

        for (auto it = begin; it != end; ++it) {
            GateType gt          = stringToGate((*it)[1].str());
            std::string portList = (*it)[3].str();
            auto ports = split(portList, ',');
            if ((int)ports.size() < 2) continue;

            std::string outName = trim(ports[0]);
            int outId = ckt.getOrCreateId(outName);

            Node* existing = ckt.getNode(outId);
            if (existing && existing->nodeType == NodeType::OUTPUT) {
                existing->nodeType = NodeType::GATE;
                existing->gateType = gt;
                ckt.gateIds.push_back(outId);
            } else if (!existing) {
                ckt.addNode(new Node(outId, outName, NodeType::GATE, gt));
                ckt.gateIds.push_back(outId);
            } else {
                existing->nodeType = NodeType::GATE;
                existing->gateType = gt;
                if (std::find(ckt.gateIds.begin(), ckt.gateIds.end(), outId) == ckt.gateIds.end())
                    ckt.gateIds.push_back(outId);
            }

            Node* gn = ckt.getNode(outId);
            for (size_t i = 1; i < ports.size(); i++) {
                std::string inName = trim(ports[i]);
                if (inName == "1'b0" || inName == "1\\'b0" || inName == "0") { gn->inputs.push_back(-1); continue; }
                if (inName == "1'b1" || inName == "1\\'b1" || inName == "1") { gn->inputs.push_back(-2); continue; }
                gn->inputs.push_back(ckt.getOrCreateId(inName));
            }
        }
    }
};

// ============================================================
//  BENCH FORMAT PARSER
// ============================================================

class BenchParser {
public:
    static bool parse(const std::string& filename, Circuit& ckt) {
        std::ifstream f(filename);
        if (!f.is_open()) {
            std::cerr << "[ERROR] Cannot open: " << filename << "\n";
            return false;
        }

        size_t slash = filename.find_last_of("/\\");
        std::string fname = (slash == std::string::npos) ? filename : filename.substr(slash + 1);
        size_t dot = fname.find_last_of('.');
        ckt.name = (dot == std::string::npos) ? fname : fname.substr(0, dot);

        std::string line;
        while (std::getline(f, line)) {
            size_t cm = line.find('#');
            if (cm != std::string::npos) line = line.substr(0, cm);
            line = trim(line);
            if (line.empty()) continue;

            std::string uline = toUpper(line);

            if (uline.substr(0, 6) == "INPUT(") {
                std::string wname = extractParens(line);
                int id = ckt.getOrCreateId(wname);
                ckt.addNode(new Node(id, wname, NodeType::INPUT));
                ckt.inputIds.push_back(id);

            } else if (uline.substr(0, 7) == "OUTPUT(") {
                std::string wname = extractParens(line);
                int id = ckt.getOrCreateId(wname);
                ckt.outputIds.push_back(id);

            } else if (line.find('=') != std::string::npos) {
                size_t eq  = line.find('=');
                size_t lp  = line.find('(', eq);
                size_t rp  = line.find(')', eq);
                if (lp == std::string::npos || rp == std::string::npos) continue;

                std::string outName = trim(line.substr(0, eq));
                std::string gateStr = trim(line.substr(eq + 1, lp - eq - 1));
                std::string inList  = line.substr(lp + 1, rp - lp - 1);

                int outId = ckt.getOrCreateId(outName);
                ckt.addNode(new Node(outId, outName, NodeType::GATE, stringToGate(gateStr)));
                ckt.gateIds.push_back(outId);

                Node* gn = ckt.getNode(outId);
                for (auto& inName : split(inList, ','))
                    gn->inputs.push_back(ckt.getOrCreateId(inName));
            }
        }
        f.close();

        addBranchNodes(ckt);
        buildOutputConnections(ckt);
        return true;
    }

private:
    static std::string extractParens(const std::string& s) {
        size_t lp = s.find('('), rp = s.find(')');
        if (lp == std::string::npos || rp == std::string::npos) return s;
        return trim(s.substr(lp + 1, rp - lp - 1));
    }
};

// ============================================================
//  PUBLIC API
// ============================================================

bool parseNetlist(const std::string& filename, Circuit& ckt) {
    std::ifstream f(filename);
    if (!f.is_open()) {
        std::cerr << "[ERROR] Cannot open: " << filename << "\n";
        return false;
    }
    std::string line;
    bool isBench = false, isVerilog = false;
    while (std::getline(f, line)) {
        std::string ul = toUpper(trim(line));
        if (ul.substr(0, 6) == "INPUT(" || ul.substr(0, 7) == "OUTPUT(")
            { isBench = true; break; }
        if (ul.find("MODULE") != std::string::npos)
            { isVerilog = true; break; }
    }
    f.close();

    bool ok = false;
    if (isBench)   ok = BenchParser::parse(filename, ckt);
    else if (isVerilog) ok = VerilogParser::parse(filename, ckt);
    else {
        std::cerr << "[ERROR] Unknown format: " << filename << "\n";
        return false;
    }

    if (ok) {
        std::cout << "[Parser] '" << filename << "': "
                  << ckt.inputIds.size() << " PIs, "
                  << ckt.outputIds.size() << " POs, "
                  << ckt.gateIds.size() << " gates, "
                  << ckt.branchIds.size() << " branches\n";
    }
    return ok;
}

std::string findFile(const std::string& basename) {
    if (std::ifstream(basename).good()) return basename;
    std::string bp = "benchmarks/" + basename;
    if (std::ifstream(bp).good()) return bp;
    bp = "../benchmarks/" + basename;
    if (std::ifstream(bp).good()) return bp;
    return "";
}

std::vector<int> topologicalSort(const Circuit& ckt) {
    std::unordered_map<int, int> inDegree;
    std::set<int> all;
    for (int id : ckt.gateIds)   all.insert(id);
    for (int id : ckt.branchIds) all.insert(id);
    for (int id : all) inDegree[id] = 0;

    for (int id : all) {
        Node* n = ckt.getNode(id);
        if (!n) continue;
        for (int inId : n->inputs)
            if (all.count(inId)) inDegree[id]++;
    }

    std::vector<int> queue;
    for (auto& kv : inDegree)
        if (kv.second == 0) queue.push_back(kv.first);

    std::vector<int> order;
    size_t qi = 0;
    while (qi < queue.size()) {
        int cur = queue[qi++];
        order.push_back(cur);
        Node* n = ckt.getNode(cur);
        if (!n) continue;

        int maxInLevel = 0;
        for (int inId : n->inputs) {
            Node* inNode = ckt.getNode(inId);
            if (inNode && inNode->level > maxInLevel)
                maxInLevel = inNode->level;
        }
        n->level = (n->nodeType == NodeType::BRNH) ? maxInLevel : (maxInLevel + 1);

        for (int outId : n->outputs)
            if (inDegree.count(outId) && --inDegree[outId] == 0)
                queue.push_back(outId);
    }

    for (int pid : ckt.inputIds) {
        Node* n = ckt.getNode(pid);
        if (n) n->level = 0;
    }

    std::vector<int> fullOrder;
    fullOrder.reserve(ckt.inputIds.size() + order.size());
    for (int pid : ckt.inputIds) fullOrder.push_back(pid);
    for (int id : order) fullOrder.push_back(id);

    return fullOrder;
}
