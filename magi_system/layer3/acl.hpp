#ifndef MAGI_LAYER3_ACL_HPP
#define MAGI_LAYER3_ACL_HPP

#include <cstdint>
#include <string>
#include <vector>
#include <memory>

namespace magi {

enum class ACLAction {
    PERMIT,
    DENY
};

enum class ACLProtocol {
    ANY,
    TCP,
    UDP,
    ICMP
};

struct ACLPortRange {
    uint16_t minPort;
    uint16_t maxPort;
    bool isAny;  // true if port range is not specified (match any)

    ACLPortRange() : minPort(0), maxPort(65535), isAny(true) {}
    ACLPortRange(uint16_t port) : minPort(port), maxPort(port), isAny(false) {}
    ACLPortRange(uint16_t min, uint16_t max) : minPort(min), maxPort(max), isAny(false) {}

    bool matches(uint16_t port) const {
        if (isAny) return true;
        return port >= minPort && port <= maxPort;
    }
};

struct ACLRule {
    int ruleId;
    ACLAction action;
    std::string sourceIpCidr;     // CIDR notation (e.g., "192.168.1.0/24" or "any")
    std::string destIpCidr;       // CIDR notation
    ACLProtocol protocol;          // TCP, UDP, ICMP, or ANY
    ACLPortRange sourcePortRange;  // For TCP/UDP
    ACLPortRange destPortRange;    // For TCP/UDP

    ACLRule()
        : ruleId(-1),
          action(ACLAction::PERMIT),
          sourceIpCidr("any"),
          destIpCidr("any"),
          protocol(ACLProtocol::ANY) {}
};

class ACLList {
private:
    std::vector<ACLRule> rules;
    int nextRuleId;

public:
    ACLList() : nextRuleId(1) {}

    int addRule(const ACLRule& rule);
    bool removeRule(int ruleId);
    void clearRules();
    
    // Check if packet should be permitted or denied
    // Returns: true if PERMIT, false if DENY, true if no match (default PERMIT)
    bool checkPacket(const std::string& srcIp, const std::string& dstIp,
                     ACLProtocol protocol, uint16_t srcPort = 0, uint16_t dstPort = 0) const;

    void printRules() const;
    const std::vector<ACLRule>& getRules() const { return rules; }
    size_t getRuleCount() const { return rules.size(); }
};

}  // namespace magi

#endif
