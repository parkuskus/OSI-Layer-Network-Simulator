#include "cli.hpp"
#include "core/interface.hpp"
#include "core/link.hpp"
#include <iostream>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <cctype>

namespace magi {

CLI::CLI() : running(true) {
}

std::string CLI::trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\n\r");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\n\r");
    return str.substr(first, last - first + 1);
}

std::vector<std::string> CLI::parseCommand(const std::string& input) {
    std::vector<std::string> args;
    std::stringstream ss(input);
    std::string arg;
    
    while (ss >> arg) {
        args.push_back(arg);
    }
    
    return args;
}

void CLI::executeCommand(const std::vector<std::string>& args) {
    if (args.empty()) return;
    
    std::string cmd = args[0];
    std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::tolower);
    
    if (cmd == "create") {
        cmdCreate(args);
    } else if (cmd == "link") {
        cmdLink(args);
    } else if (cmd == "unlink") {
        cmdUnlink(args);
    } else if (cmd == "topology") {
        cmdTopology();
    } else if (cmd == "save") {
        cmdSave(args);
    } else if (cmd == "load") {
        cmdLoad(args);
    } else if (cmd == "show") {
        cmdShow(args);
    } else if (cmd == "help" || cmd == "?") {
        cmdHelp();
    } else if (cmd == "exit" || cmd == "quit") {
        running = false;
        std::cout << "Menghentikan Magi System Simulator..." << std::endl;
    } else {
        std::cout << "Perintah tidak dikenal: " << cmd << std::endl;
        std::cout << "Ketik 'help' untuk melihat daftar perintah." << std::endl;
    }
}

void CLI::cmdCreate(const std::vector<std::string>& args) {
    if (args.size() < 3) {
        std::cout << "Penggunaan: create <name> <host|switch|router> [jumlah_port]" << std::endl;
        return;
    }
    
    std::string name = args[1];
    std::string type = args[2];
    std::transform(type.begin(), type.end(), type.begin(), ::tolower);
    
    if (nodes.find(name) != nodes.end()) {
        std::cout << "Error: Node dengan nama '" << name << "' sudah ada." << std::endl;
        return;
    }
    
    std::shared_ptr<Node> node = nullptr;
    
    if (type == "host") {
        if (args.size() >= 4) {
            std::cout << "Error: Host hanya dapat memiliki 1 interface." << std::endl;
            std::cout << "Penggunaan: create <name> host" << std::endl;
            return;
        }
        node = std::make_shared<Host>(name);
        std::cout << "Host '" << name << "' berhasil dibuat." << std::endl;
    } else if (type == "switch") {
        uint32_t numPorts = 24;
        if (args.size() >= 4) {
            numPorts = std::stoul(args[3]);
        }
        node = std::make_shared<Switch>(name, numPorts);
        std::cout << "Switch '" << name << "' dengan " << numPorts << " port berhasil dibuat." << std::endl;
    } else if (type == "router") {
        node = std::make_shared<Router>(name);
        std::cout << "Router '" << name << "' berhasil dibuat." << std::endl;
    } else {
        std::cout << "Error: Tipe node tidak valid. Gunakan: host, switch, atau router." << std::endl;
        return;
    }
    
    nodes[name] = node;
}

bool CLI::parseEndpoint(const std::string& endpoint, std::string& nodeName, uint32_t& port) {
    size_t colonPos = endpoint.find(':');
    if (colonPos != std::string::npos) {
        nodeName = endpoint.substr(0, colonPos);
        port = std::stoul(endpoint.substr(colonPos + 1));
    } else {
        nodeName = endpoint;
        port = 1;
    }
    return true;
}

void CLI::cmdLink(const std::vector<std::string>& args) {
    if (args.size() < 3) {
        std::cout << "Penggunaan: link <device1> <device2> [delay_ms]" << std::endl;
        std::cout << "  Contoh: link H1 SW1:1" << std::endl;
        std::cout << "  Contoh: link SW1:24 R1:1 10" << std::endl;
        return;
    }
    
    std::string endpoint1 = args[1];
    std::string endpoint2 = args[2];
    uint32_t delay = 0;
    
    if (args.size() >= 4) {
        delay = std::stoul(args[3]);
    }
    
    std::string nodeAName, nodeBName;
    uint32_t portA, portB;
    
    parseEndpoint(endpoint1, nodeAName, portA);
    parseEndpoint(endpoint2, nodeBName, portB);
    
    auto nodeA = findNode(nodeAName);
    auto nodeB = findNode(nodeBName);
    
    if (!nodeA) {
        std::cout << "Error: Node '" << nodeAName << "' tidak ditemukan." << std::endl;
        return;
    }
    if (!nodeB) {
        std::cout << "Error: Node '" << nodeBName << "' tidak ditemukan." << std::endl;
        return;
    }
    
    auto ifaceA = nodeA->getInterface(portA);
    auto ifaceB = nodeB->getInterface(portB);
    
    if (!ifaceA) {
        std::cout << "Error: Port " << portA << " pada '" << nodeAName << "' tidak ditemukan." << std::endl;
        return;
    }
    if (!ifaceB) {
        std::cout << "Error: Port " << portB << " pada '" << nodeBName << "' tidak ditemukan." << std::endl;
        return;
    }
    
    if (ifaceA->isConnected()) {
        std::cout << "Error: Port " << portA << " pada '" << nodeAName << "' sudah terhubung." << std::endl;
        return;
    }
    if (ifaceB->isConnected()) {
        std::cout << "Error: Port " << portB << " pada '" << nodeBName << "' sudah terhubung." << std::endl;
        return;
    }
    
    auto link = Link::create(ifaceA, ifaceB, delay);
    
    LinkConnection conn;
    conn.nodeAName = nodeAName;
    conn.portA = portA;
    conn.nodeBName = nodeBName;
    conn.portB = portB;
    conn.delay = delay;
    conn.link = link;
    connections.push_back(conn);
    
    std::cout << "Berhasil menghubungkan " << endpoint1 << " dengan " << endpoint2;
    if (delay > 0) {
        std::cout << " (delay: " << delay << " ms)";
    }
    std::cout << "." << std::endl;
}

void CLI::cmdUnlink(const std::vector<std::string>& args) {
    if (args.size() < 3) {
        std::cout << "Penggunaan: unlink <device1> <device2>" << std::endl;
        return;
    }
    
    std::string endpoint1 = args[1];
    std::string endpoint2 = args[2];
    
    std::string nodeAName, nodeBName;
    uint32_t portA, portB;
    
    parseEndpoint(endpoint1, nodeAName, portA);
    parseEndpoint(endpoint2, nodeBName, portB);
    
    bool found = false;
    for (auto it = connections.begin(); it != connections.end(); ++it) {
        if (((it->nodeAName == nodeAName && it->portA == portA &&
              it->nodeBName == nodeBName && it->portB == portB) ||
             (it->nodeAName == nodeBName && it->portA == portB &&
              it->nodeBName == nodeAName && it->portB == portA))) {
            it->link->disconnect();
            connections.erase(it);
            found = true;
            break;
        }
    }
    
    if (found) {
        std::cout << "Berhasil memutuskan koneksi antara " << endpoint1 << " dan " << endpoint2 << "." << std::endl;
    } else {
        std::cout << "Error: Koneksi antara " << endpoint1 << " dan " << endpoint2 << " tidak ditemukan." << std::endl;
    }
}

void CLI::cmdTopology() {
    std::cout << "=== TOPOLOGY ===" << std::endl;
    std::cout << "Nodes:" << std::endl;
    for (const auto& pair : nodes) {
        std::cout << "  " << pair.first << " (" << pair.second->getType() << ")" << std::endl;
    }
    
    std::cout << "\nLinks:" << std::endl;
    for (const auto& conn : connections) {
        std::cout << "  " << conn.nodeAName << ":" << conn.portA 
                  << " <-> " << conn.nodeBName << ":" << conn.portB;
        if (conn.delay > 0) {
            std::cout << " [" << conn.delay << " ms]";
        }
        std::cout << std::endl;
    }
}

void CLI::cmdSave(const std::vector<std::string>& args) {
    std::string filename = "topology.json";
    if (args.size() >= 2) {
        filename = args[1];
    }
    
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cout << "Error: Tidak dapat membuka file '" << filename << "' untuk menulis." << std::endl;
        return;
    }
    
    file << "{\n";
    
    file << "  \"hosts\": [\n";
    bool firstHost = true;
    for (const auto& pair : nodes) {
        if (pair.second->getType() == "host") {
            if (!firstHost) file << ",\n";
            firstHost = false;
            file << pair.second->toJson();
        }
    }
    file << "\n  ],\n";
    
    file << "  \"switches\": [\n";
    bool firstSwitch = true;
    for (const auto& pair : nodes) {
        if (pair.second->getType() == "switch") {
            if (!firstSwitch) file << ",\n";
            firstSwitch = false;
            file << pair.second->toJson();
        }
    }
    file << "\n  ],\n";
    
    file << "  \"routers\": [\n";
    bool firstRouter = true;
    for (const auto& pair : nodes) {
        if (pair.second->getType() == "router") {
            if (!firstRouter) file << ",\n";
            firstRouter = false;
            file << pair.second->toJson();
        }
    }
    file << "\n  ],\n";
    
    file << "  \"links\": [\n";
    for (size_t i = 0; i < connections.size(); ++i) {
        if (i > 0) file << ",\n";
        const auto& conn = connections[i];
        file << "    {\n";
        file << "      \"endpoints\": [\"" << conn.nodeAName;
        if (conn.portA != 1 || findNode(conn.nodeAName)->getType() != "host") {
            file << ":" << conn.portA;
        }
        file << "\", \"" << conn.nodeBName;
        if (conn.portB != 1 || findNode(conn.nodeBName)->getType() != "host") {
            file << ":" << conn.portB;
        }
        file << "\"],\n";
        file << "      \"delay\": " << conn.delay << "\n";
        file << "    }";
    }
    file << "\n  ]\n";
    
    file << "}\n";
    file.close();
    
    std::cout << "Topologi berhasil disimpan ke '" << filename << "'." << std::endl;
}

std::string CLI::extractJsonValue(const std::string& line, const std::string& key) {
    size_t keyPos = line.find("\"" + key + "\"");
    if (keyPos == std::string::npos) return "";
    
    size_t colonPos = line.find(':', keyPos);
    if (colonPos == std::string::npos) return "";
    
    size_t quoteStart = line.find('"', colonPos);
    if (quoteStart == std::string::npos) {
        size_t valueStart = line.find_first_not_of(" \t", colonPos + 1);
        size_t valueEnd = line.find_first_of(",}\n", valueStart);
        return trim(line.substr(valueStart, valueEnd - valueStart));
    }
    
    size_t quoteEnd = line.find('"', quoteStart + 1);
    if (quoteEnd == std::string::npos) return "";
    
    return line.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
}

bool CLI::parseJsonFile(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cout << "Error: Tidak dapat membuka file '" << filename << "'." << std::endl;
        return false;
    }
    
    clearTopology();
    
    std::string line;
    std::string currentSection = "";
    
    while (std::getline(file, line)) {
        line = trim(line);
        
        if (line.find("\"hosts\"") != std::string::npos) {
            currentSection = "hosts";
        } else if (line.find("\"switches\"") != std::string::npos) {
            currentSection = "switches";
        } else if (line.find("\"routers\"") != std::string::npos) {
            currentSection = "routers";
        } else if (line.find("\"links\"") != std::string::npos) {
            currentSection = "links";
        } else if (line.find("{") != std::string::npos && !currentSection.empty()) {
            // Parse object
            if (currentSection == "hosts") {
                std::string name = "";
                std::string ip = "";
                std::string gateway = "";
                
                // Baca beberapa baris untuk data host
                while (std::getline(file, line) && line.find("}") == std::string::npos) {
                    if (line.find("\"name\"") != std::string::npos) {
                        name = extractJsonValue(line, "name");
                    } else if (line.find("\"ip_address\"") != std::string::npos) {
                        ip = extractJsonValue(line, "ip_address");
                    } else if (line.find("\"default_gateway\"") != std::string::npos) {
                        gateway = extractJsonValue(line, "default_gateway");
                    }
                }
                
                if (!name.empty()) {
                    auto host = std::make_shared<Host>(name, ip, gateway);
                    nodes[name] = host;
                }
            } else if (currentSection == "switches") {
                std::string name = "";
                uint32_t numPorts = 24;
                
                while (std::getline(file, line) && line.find("}") == std::string::npos) {
                    if (line.find("\"name\"") != std::string::npos) {
                        name = extractJsonValue(line, "name");
                    } else if (line.find("\"num_ports\"") != std::string::npos) {
                        std::string portsStr = extractJsonValue(line, "num_ports");
                        if (!portsStr.empty()) numPorts = std::stoul(portsStr);
                    }
                }
                
                if (!name.empty()) {
                    auto sw = std::make_shared<Switch>(name, numPorts);
                    nodes[name] = sw;
                }
            } else if (currentSection == "routers") {
                std::string name = "";
                
                while (std::getline(file, line) && line.find("}") == std::string::npos) {
                    if (line.find("\"name\"") != std::string::npos) {
                        name = extractJsonValue(line, "name");
                    }
                }
                
                if (!name.empty()) {
                    auto router = std::make_shared<Router>(name);
                    nodes[name] = router;
                }
            } else if (currentSection == "links") {
                std::vector<std::string> endpoints;
                uint32_t delay = 0;
                
                while (std::getline(file, line) && line.find("}") == std::string::npos) {
                    if (line.find("\"endpoints\"") != std::string::npos) {
                        // Parse array endpoints
                        size_t bracketStart = line.find('[');
                        size_t bracketEnd = line.find(']');
                        if (bracketStart != std::string::npos && bracketEnd != std::string::npos) {
                            std::string arrayContent = line.substr(bracketStart + 1, bracketEnd - bracketStart - 1);
                            std::stringstream ss(arrayContent);
                            std::string endpoint;
                            while (std::getline(ss, endpoint, ',')) {
                                endpoint = trim(endpoint);
                                // Remove quotes
                                if (endpoint.size() >= 2 && endpoint[0] == '"' && endpoint[endpoint.size()-1] == '"') {
                                    endpoint = endpoint.substr(1, endpoint.size() - 2);
                                }
                                if (!endpoint.empty()) {
                                    endpoints.push_back(endpoint);
                                }
                            }
                        }
                    } else if (line.find("\"delay\"") != std::string::npos) {
                        std::string delayStr = extractJsonValue(line, "delay");
                        if (!delayStr.empty()) delay = std::stoul(delayStr);
                    }
                }
                
                if (endpoints.size() >= 2) {
                    // Gunakan cmdLink untuk membuat koneksi
                    std::vector<std::string> linkArgs = {"link", endpoints[0], endpoints[1], std::to_string(delay)};
                    cmdLink(linkArgs);
                }
            }
        }
    }
    
    file.close();
    return true;
}

void CLI::cmdLoad(const std::vector<std::string>& args) {
    std::string filename = "topology.json";
    if (args.size() >= 2) {
        filename = args[1];
    }
    
    if (parseJsonFile(filename)) {
        std::cout << "Topologi berhasil dimuat dari '" << filename << "'." << std::endl;
    }
}

void CLI::cmdShow(const std::vector<std::string>& args) {
    if (args.size() < 2) {
        std::cout << "Penggunaan: show <node_name>" << std::endl;
        return;
    }
    
    std::string nodeName = args[1];
    auto node = findNode(nodeName);
    
    if (node) {
        node->printInfo();
    } else {
        std::cout << "Error: Node '" << nodeName << "' tidak ditemukan." << std::endl;
    }
}

std::shared_ptr<Node> CLI::findNode(const std::string& name) {
    auto it = nodes.find(name);
    if (it != nodes.end()) {
        return it->second;
    }
    return nullptr;
}

void CLI::clearTopology() {
    nodes.clear();
    connections.clear();
}

void CLI::cmdHelp() {
    std::cout << "=== MAGI SYSTEM SIMULATOR - COMMAND LIST ===" << std::endl;
    std::cout << std::endl;
    std::cout << "Manajemen Topologi:" << std::endl;
    std::cout << "  create <name> <host|switch|router> [jumlah_port] - Membuat node baru" << std::endl;
    std::cout << "  link <device1> <device2> [delay_ms]              - Menghubungkan dua device" << std::endl;
    std::cout << "  unlink <device1> <device2>                       - Memutuskan koneksi" << std::endl;
    std::cout << "  topology                                         - Menampilkan topologi" << std::endl;
    std::cout << "  show <node_name>                                 - Menampilkan info node" << std::endl;
    std::cout << std::endl;
    std::cout << "File Operations:" << std::endl;
    std::cout << "  save [filename]                                  - Menyimpan topologi ke JSON" << std::endl;
    std::cout << "  load [filename]                                  - Memuat topologi dari JSON" << std::endl;
    std::cout << std::endl;
    std::cout << "General:" << std::endl;
    std::cout << "  help                                             - Menampilkan bantuan" << std::endl;
    std::cout << "  exit / quit                                      - Keluar dari simulator" << std::endl;
    std::cout << std::endl;
    std::cout << "Format endpoint: NodeName atau NodeName:Port" << std::endl;
    std::cout << "  Contoh: H1, H1:1, SW1:24" << std::endl;
}

void CLI::run() {
    std::cout << "╔══════════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║        MAGI SYSTEM: OSI Layer Network Simulator              ║" << std::endl;
    std::cout << "║              NERV Division - Milestone 0                     ║" << std::endl;
    std::cout << "╚══════════════════════════════════════════════════════════════╝" << std::endl;
    std::cout << std::endl;
    std::cout << "Ketik 'help' untuk melihat daftar perintah." << std::endl;
    std::cout << std::endl;
    
    std::string input;
    
    while (running) {
        std::cout << "Magi> ";
        std::getline(std::cin, input);
        
        // Parse dan eksekusi perintah
        std::vector<std::string> args = parseCommand(input);
        executeCommand(args);
        
        if (running) {
            std::cout << std::endl;
        }
    }
}

} // namespace magi
