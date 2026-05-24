#ifndef MAGI_LAYER7_DHCP_SERVER_HPP
#define MAGI_LAYER7_DHCP_SERVER_HPP

#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <memory>

namespace magi {

class Host;
class MagiSocket;

class DHCPServer {
public:
    DHCPServer(Host *host);
    ~DHCPServer();

    bool start();
    void stop();

private:
    Host *host;
    std::shared_ptr<MagiSocket> socket;
    std::vector<std::string> pool;
    std::map<std::string, std::string> leases; // mac -> ip

    void buildPoolFromHost();
    void handleMessage(const std::string &srcIp, uint16_t srcPort, const std::vector<uint8_t> &data);
    std::string chooseOfferForMac(const std::string &mac);
};

} // namespace magi

#endif // MAGI_LAYER7_DHCP_SERVER_HPP
