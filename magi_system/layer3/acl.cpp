#include "acl.hpp"
#include "ip_utils.hpp"
#include <iostream>
#include <iomanip>

namespace magi {

int ACLList::addRule(const ACLRule& rule) {
    ACLRule newRule = rule;
    if (newRule.ruleId == -1) {
        newRule.ruleId = nextRuleId++;
    } else if (newRule.ruleId >= nextRuleId) {
        nextRuleId = newRule.ruleId + 1;
    }
    
    // Insert maintaining rule order (earlier rules take precedence)
    rules.push_back(newRule);
    return newRule.ruleId;
}

bool ACLList::removeRule(int ruleId) {
    for (size_t i = 0; i < rules.size(); ++i) {
        if (rules[i].ruleId == ruleId) {
            rules.erase(rules.begin() + static_cast<long>(i));
            return true;
        }
    }
    return false;
}

void ACLList::clearRules() {
    rules.clear();
}

bool ACLList::checkPacket(const std::string& srcIp, const std::string& dstIp,
                         ACLProtocol protocol, uint16_t srcPort, uint16_t dstPort) const {
    // Check each rule in order
    for (size_t i = 0; i < rules.size(); ++i) {
        const ACLRule& rule = rules[i];

        // Check source IP
        if (rule.sourceIpCidr != "any") {
            if (!iputil::ipInCidr(srcIp, rule.sourceIpCidr)) {
                continue;  // Source IP doesn't match, try next rule
            }
        }

        // Check destination IP
        if (rule.destIpCidr != "any") {
            if (!iputil::ipInCidr(dstIp, rule.destIpCidr)) {
                continue;  // Destination IP doesn't match, try next rule
            }
        }

        // Check protocol
        if (rule.protocol != ACLProtocol::ANY && rule.protocol != protocol) {
            continue;  // Protocol doesn't match, try next rule
        }

        // Check source port (only for TCP/UDP)
        if ((protocol == ACLProtocol::TCP || protocol == ACLProtocol::UDP) &&
            !rule.sourcePortRange.matches(srcPort)) {
            continue;  // Source port doesn't match, try next rule
        }

        // Check destination port (only for TCP/UDP)
        if ((protocol == ACLProtocol::TCP || protocol == ACLProtocol::UDP) &&
            !rule.destPortRange.matches(dstPort)) {
            continue;  // Destination port doesn't match, try next rule
        }

        // Rule matches - return based on action
        return rule.action == ACLAction::PERMIT;
    }

    // No rule matched - default to PERMIT
    return true;
}

void ACLList::printRules() const {
    std::cout << "[ACL Rules]" << std::endl;
    if (rules.empty()) {
        std::cout << "  (empty)" << std::endl;
        return;
    }

    std::cout << "  ID   Action  Protocol  Source IP/CIDR       Dst IP/CIDR          Src Port    Dst Port" << std::endl;
    std::cout << "  ---  ------  --------  -------------------  -------------------  ----------  ----------" << std::endl;

    for (size_t i = 0; i < rules.size(); ++i) {
        const ACLRule& rule = rules[i];
        
        std::string actionStr = (rule.action == ACLAction::PERMIT) ? "permit" : "deny";
        std::string protocStr = "any";
        if (rule.protocol == ACLProtocol::TCP) protocStr = "tcp";
        else if (rule.protocol == ACLProtocol::UDP) protocStr = "udp";
        else if (rule.protocol == ACLProtocol::ICMP) protocStr = "icmp";

        std::string srcPortStr = rule.sourcePortRange.isAny ? "any" : 
                                 (rule.sourcePortRange.minPort == rule.sourcePortRange.maxPort ?
                                  std::to_string(rule.sourcePortRange.minPort) :
                                  std::to_string(rule.sourcePortRange.minPort) + "-" + 
                                  std::to_string(rule.sourcePortRange.maxPort));

        std::string dstPortStr = rule.destPortRange.isAny ? "any" :
                                 (rule.destPortRange.minPort == rule.destPortRange.maxPort ?
                                  std::to_string(rule.destPortRange.minPort) :
                                  std::to_string(rule.destPortRange.minPort) + "-" +
                                  std::to_string(rule.destPortRange.maxPort));

        std::cout << "  " << std::setw(2) << rule.ruleId << "   "
                  << std::setw(6) << actionStr << "  "
                  << std::setw(7) << protocStr << "  "
                  << std::setw(19) << rule.sourceIpCidr << "  "
                  << std::setw(19) << rule.destIpCidr << "  "
                  << std::setw(10) << srcPortStr << "  "
                  << std::setw(10) << dstPortStr << std::endl;
    }
}

}  // namespace magi
