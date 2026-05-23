/*
Milestone 4 socket wrapper smoke test
*/

#include "../test_common.hpp"

#include "core/link.hpp"
#include "core/interface.hpp"
#include "layer2/host.hpp"
#include "layer7/dhcp_client.hpp"
#include "layer7/dhcp_server.hpp"
#include "layer7/magi_socket.hpp"
#include "layer7/dns_client.hpp"
#include "layer7/dns_server.hpp"
#include "layer7/http_client.hpp"
#include "layer7/http_server.hpp"

#include <memory>
#include <cstdint>
#include <string>
#include <vector>

namespace
{

    std::vector<uint8_t> bytesFromString(const std::string &text)
    {
        return std::vector<uint8_t>(text.begin(), text.end());
    }

} // namespace

bool runMilestone4Tests()
{
    std::cout << std::endl
              << "Running Milestone 4 socket wrapper smoke test and DHCP DORA test" << std::endl;

    magi_test::TestStats stats;

    magi::Host clientHost("HC", "10.0.0.2/24", "10.0.0.1");
    magi::Host serverHost("HS", "10.0.0.3/24", "10.0.0.1");
    clientHost.getInterface(1)->setMacAddress("00:00:00:00:40:01");
    serverHost.getInterface(1)->setMacAddress("00:00:00:00:40:02");
    magi::Link::create(clientHost.getInterface(1), serverHost.getInterface(1));

    magi::MagiSocket serverSocket(&serverHost, magi::MagiSocket::AF_INET, magi::MagiSocket::SOCK_STREAM);
    magi_test::expect(stats, serverSocket.bind("10.0.0.3", 80), "Server socket bind succeeds");
    magi_test::expect(stats, serverSocket.listen(5), "Server socket listen succeeds");

    magi::MagiSocket clientSocket(&clientHost, magi::MagiSocket::AF_INET, magi::MagiSocket::SOCK_STREAM);
    magi_test::expect(stats, clientSocket.bind("10.0.0.2", 49152), "Client socket bind succeeds");
    magi_test::expect(stats, clientSocket.connect("10.0.0.3", 80), "Client socket connect succeeds");
    magi_test::expect(stats, clientSocket.isConnected(), "Client socket reaches ESTABLISHED");
    magi_test::expect(stats, serverSocket.isConnected(), "Server listening socket reaches ESTABLISHED");

    std::shared_ptr<magi::MagiSocket> accepted = serverSocket.accept();
    magi_test::expect(stats, accepted != nullptr, "Server accept returns connected socket");
    if (accepted)
    {
        magi_test::expect(stats, accepted->isConnected(), "Accepted socket is connected");
    }

    const std::vector<uint8_t> payload = bytesFromString("MAGI_M4_SOCKET_SMOKE");
    magi_test::expect(stats, clientSocket.send(payload) == payload.size(), "Client socket send succeeds");
    magi_test::expect(stats, serverSocket.recv(1024) == payload, "Server socket receives payload");

    std::shared_ptr<magi::MagiSocket> acceptedAfterData = serverSocket.accept();
    if (acceptedAfterData)
    {
        magi_test::expect(stats, acceptedAfterData->recv(1024).empty(), "Accepted socket shares drained buffer state");
    }

    magi_test::expect(stats, clientSocket.close(), "Client socket close succeeds");
    magi_test::expect(stats, serverSocket.close(), "Server socket close succeeds");

    magi::Host dhcpServerHost("DHCP-S", "10.20.0.1/24", "10.20.0.254");
    magi::Host dhcpClientHost("DHCP-C", "10.20.0.2/24", "10.20.0.1");
    dhcpServerHost.getInterface(1)->setMacAddress("00:00:00:00:40:11");
    dhcpClientHost.getInterface(1)->setMacAddress("00:00:00:00:40:12");
    magi::Link::create(dhcpServerHost.getInterface(1), dhcpClientHost.getInterface(1));

    std::string assignedIp;
    std::string dhcpLog = magi_test::captureStdout([&]()
                                                   {
        dhcpServerHost.startDhcpServer();
        assignedIp = magi::DHCPClient::discover(&dhcpClientHost, 2000, 3);
        dhcpServerHost.stopDhcpServer(); });

    if (assignedIp.empty())
    {
        std::cout << dhcpLog << std::endl;
    }

    magi_test::expect(stats, assignedIp == "10.20.0.99", "DHCP client receives expected lease");
    magi_test::expect(stats, dhcpClientHost.getIpAddress() == "10.20.0.99/24", "DHCP client host IP is updated");
    magi_test::expect(stats, magi_test::contains(dhcpLog, "Attempt 1/3"), "DHCP log records discovery attempt");
    magi_test::expect(stats, magi_test::contains(dhcpLog, "Received DISCOVER"), "DHCP log records OFFER path");
    magi_test::expect(stats, magi_test::contains(dhcpLog, "Received REQUEST"), "DHCP log records ACK path");
    magi_test::expect(stats, magi_test::contains(dhcpLog, "Lease acquired"), "DHCP log records successful lease");

    // --- DNS -> HTTP integration (inlined into the same stats) ---
    std::cout << std::endl
              << "Running Milestone 4 DNS->HTTP integration test" << std::endl;

    auto server = std::make_shared<magi::Host>("HS", "192.168.1.11/24", "192.168.1.1");
    auto client = std::make_shared<magi::Host>("HC", "192.168.1.10/24", "192.168.1.1");
    server->getInterface(1)->setMacAddress("00:00:00:00:50:01");
    client->getInterface(1)->setMacAddress("00:00:00:00:50:02");
    magi::Link::create(server->getInterface(1), client->getInterface(1));

    server->startDnsServer();
    server->startHttpServer("index.html");

    std::vector<std::shared_ptr<magi::Host>> allHosts = {server, client};

    std::string out = magi_test::captureStdout([&]()
                                               { magi::HTTPClient::get(client, "www.HS.com", allHosts); });

    bool dnsQueried = magi_test::contains(out, "[DNS] QUERY");
    bool httpReceived = magi_test::contains(out, "HTTP/1.1 200") || magi_test::contains(out, "[HTTP Client] Menerima HTTP Response");

    magi_test::expect(stats, dnsQueried, "DNS server received a query from client");
    magi_test::expect(stats, httpReceived, "HTTP client received a response after DNS resolution");

    server->stopHttpServer();
    server->stopDnsServer();

    // --- end DNS->HTTP integration ---

    std::cout << std::endl
              << "Milestone 4 summary: " << (stats.checks - stats.failed)
              << "/" << stats.checks << " checks passed" << std::endl;

    return stats.failed == 0;
}
