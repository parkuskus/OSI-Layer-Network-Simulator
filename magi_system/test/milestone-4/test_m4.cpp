/*
Milestone 4 socket wrapper smoke test
*/

#include "../test_common.hpp"

#include "core/link.hpp"
#include "core/interface.hpp"
#include "layer2/host.hpp"
#include "layer7/magi_socket.hpp"

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
              << "Running Milestone 4 socket wrapper smoke test" << std::endl;

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

    std::cout << std::endl
              << "Milestone 4 summary: " << (stats.checks - stats.failed)
              << "/" << stats.checks << " checks passed" << std::endl;

    return stats.failed == 0;
}
