#ifndef MAGI_LAYER3_IP_UTILS_HPP
#define MAGI_LAYER3_IP_UTILS_HPP

#include <array>
#include <cstdint>
#include <sstream>
#include <string>
#include <vector>

namespace magi {
namespace iputil {

constexpr int kUntaggedVlan = -1;

struct ParsedCidr {
    std::string address;
    uint8_t prefixLength;
    uint32_t addressValue;
    uint32_t mask;
    uint32_t networkValue;
};

inline std::vector<std::string> split(const std::string& input, char delimiter) {
    std::vector<std::string> parts;
    std::stringstream ss(input);
    std::string part;
    while (std::getline(ss, part, delimiter)) {
        parts.push_back(part);
    }
    return parts;
}

inline bool parseIp(const std::string& ip, uint32_t& value) {
    const std::vector<std::string> parts = split(ip, '.');
    if (parts.size() != 4) {
        return false;
    }

    value = 0;
    for (size_t i = 0; i < parts.size(); ++i) {
        if (parts[i].empty()) {
            return false;
        }

        int octet = -1;
        try {
            octet = std::stoi(parts[i]);
        } catch (...) {
            return false;
        }

        if (octet < 0 || octet > 255) {
            return false;
        }

        value = (value << 8) | static_cast<uint32_t>(octet);
    }

    return true;
}

inline std::string toIp(uint32_t value) {
    std::stringstream ss;
    ss << ((value >> 24) & 0xFF) << "."
       << ((value >> 16) & 0xFF) << "."
       << ((value >> 8) & 0xFF) << "."
       << (value & 0xFF);
    return ss.str();
}

inline uint32_t maskFromPrefix(uint8_t prefixLength) {
    if (prefixLength == 0) {
        return 0;
    }
    if (prefixLength >= 32) {
        return 0xFFFFFFFFu;
    }
    return 0xFFFFFFFFu << (32 - prefixLength);
}

inline std::string stripCidr(const std::string& value) {
    const size_t slash = value.find('/');
    if (slash == std::string::npos) {
        return value;
    }
    return value.substr(0, slash);
}

inline bool parseCidr(const std::string& cidr, ParsedCidr& out) {
    const size_t slash = cidr.find('/');
    const std::string address = stripCidr(cidr);

    uint32_t addressValue = 0;
    if (!parseIp(address, addressValue)) {
        return false;
    }

    int prefixLength = 32;
    if (slash != std::string::npos) {
        try {
            prefixLength = std::stoi(cidr.substr(slash + 1));
        } catch (...) {
            return false;
        }
        if (prefixLength < 0 || prefixLength > 32) {
            return false;
        }
    }

    out.address = address;
    out.prefixLength = static_cast<uint8_t>(prefixLength);
    out.addressValue = addressValue;
    out.mask = maskFromPrefix(out.prefixLength);
    out.networkValue = addressValue & out.mask;
    return true;
}

inline bool isValidIp(const std::string& ip) {
    uint32_t ignored = 0;
    return parseIp(ip, ignored);
}

inline bool isValidCidr(const std::string& cidr) {
    ParsedCidr parsed;
    return parseCidr(cidr, parsed);
}

inline std::string networkAddress(const std::string& cidr) {
    ParsedCidr parsed;
    if (!parseCidr(cidr, parsed)) {
        return "";
    }
    return toIp(parsed.networkValue);
}

inline std::string canonicalCidr(const std::string& cidr) {
    ParsedCidr parsed;
    if (!parseCidr(cidr, parsed)) {
        return "";
    }
    std::stringstream ss;
    ss << toIp(parsed.networkValue) << "/" << static_cast<int>(parsed.prefixLength);
    return ss.str();
}

inline bool ipInCidr(const std::string& ip, const std::string& cidr) {
    ParsedCidr parsed;
    if (!parseCidr(cidr, parsed)) {
        return false;
    }

    uint32_t ipValue = 0;
    if (!parseIp(ip, ipValue)) {
        return false;
    }

    return (ipValue & parsed.mask) == parsed.networkValue;
}

inline bool sameSubnet(const std::string& cidr, const std::string& otherIp) {
    return ipInCidr(otherIp, cidr);
}

inline uint8_t prefixLength(const std::string& cidr, uint8_t fallback) {
    ParsedCidr parsed;
    if (!parseCidr(cidr, parsed)) {
        return fallback;
    }
    return parsed.prefixLength;
}

inline std::string makeLogicalInterfaceKey(uint32_t portNumber, int vlanId) {
    std::stringstream ss;
    ss << portNumber << ".";
    if (vlanId == kUntaggedVlan) {
        ss << "untagged";
    } else {
        ss << vlanId;
    }
    return ss.str();
}

}
}

#endif