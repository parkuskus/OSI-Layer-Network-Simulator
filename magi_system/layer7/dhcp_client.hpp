#ifndef MAGI_LAYER7_DHCP_CLIENT_HPP
#define MAGI_LAYER7_DHCP_CLIENT_HPP

#include <string>
#include "layer2/host.hpp"

namespace magi {

class DHCPClient {
public:
    // Perform DHCP discover on given host. Returns assigned IP or empty string on failure.
    static std::string discover(Host *host, int timeoutMs = 3000, int maxAttempts = 3);
};

} // namespace magi

#endif // MAGI_LAYER7_DHCP_CLIENT_HPP
