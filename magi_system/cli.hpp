#ifndef CLI_HPP
#define CLI_HPP

#include "core/node.hpp"
#include "core/link.hpp"
#include <map>
#include <string>
#include <memory>
#include <vector>

namespace magi {

struct LinkConnection {
    std::string nodeAName;
    uint32_t portA;
    std::string nodeBName;
    uint32_t portB;
    uint32_t delay;
    std::shared_ptr<Link> link;
};

class CLI {
private:
    std::map<std::string, std::shared_ptr<Node>> nodes;  // nama node -> Node
    std::vector<LinkConnection> connections;             // daftar koneksi
    bool running;
    
    std::vector<std::string> parseCommand(const std::string& input);
    
    void executeCommand(const std::vector<std::string>& args);
    
    void cmdCreate(const std::vector<std::string>& args);
    void cmdLink(const std::vector<std::string>& args);
    void cmdUnlink(const std::vector<std::string>& args);
    void cmdTopology();
    void cmdSave(const std::vector<std::string>& args);
    void cmdLoad(const std::vector<std::string>& args);
    void cmdShow(const std::vector<std::string>& args);
    void cmdHelp();
    
    std::shared_ptr<Node> findNode(const std::string& name);
    bool parseEndpoint(const std::string& endpoint, std::string& nodeName, uint32_t& port);
    void clearTopology();
    
    bool parseJsonFile(const std::string& filename);
    std::string trim(const std::string& str);
    std::string extractJsonString(const std::string& line);
    std::string extractJsonValue(const std::string& line, const std::string& key);

public:
    CLI();
    
    void run();
    
    void stop() { running = false; }
    
    bool isRunning() const { return running; }
};

} 

#endif 
