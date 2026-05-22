/*
TCP teardown tests for Milestone 3
*/

#include "../test_common.hpp"

#include "layer4/tcp_socket.hpp"

bool runMilestone3TeardownTests()
{
    std::cout << std::endl
              << "Running Milestone 3 TCP teardown tests" << std::endl;
    magi_test::TestStats stats;

    // Setup handshake
    magi::TCPSocket client("10.0.0.2", 40000, "10.0.0.3", 80);
    magi::TCPSocket server("10.0.0.3", 80, "10.0.0.2", 40000);
    client.seqNum = 1000;
    server.seqNum = 7000;
    server.setState(magi::TCPState::LISTEN);

    auto syn = client.initiateConnection();
    magi::TCPSegment synSeg = *syn;
    auto synAck = server.respondToSyn(synSeg);
    magi::TCPSegment synAckSeg = *synAck;
    auto ack = client.acknowledgeConnection(synAckSeg);
    magi::TCPSegment ackSeg = *ack;
    server.handleIncomingSegment(ackSeg);

    magi_test::expect(stats, client.isConnected(), "Client in ESTABLISHED after handshake");
    magi_test::expect(stats, server.isConnected(), "Server in ESTABLISHED after handshake");

    // Client initiates close
    auto finFromClient = client.initiateClose();
    magi_test::expect(stats, finFromClient != nullptr, "Client produced FIN segment");

    auto ackFromServer = server.respondToFin(*finFromClient);
    magi_test::expect(stats, ackFromServer != nullptr, "Server responded to FIN with ACK");

    // Client processes ACK
    client.handleIncomingSegment(*ackFromServer);
    magi_test::expect(stats, client.getStateString() == std::string("FIN_WAIT_2"), "Client in FIN_WAIT_2 after ACK for FIN");
    magi_test::expect(stats, server.getStateString() == std::string("CLOSE_WAIT"), "Server in CLOSE_WAIT after responding to FIN");

    // Server closes (application calls close)
    auto finFromServer = server.initiateClose();
    magi_test::expect(stats, finFromServer != nullptr, "Server produced FIN to close (LAST_ACK)");

    // Client handles server FIN and should reply with ACK and enter TIME_WAIT
    auto ackFromClient = client.handleIncomingSegment(*finFromServer);
    magi_test::expect(stats, ackFromClient != nullptr, "Client replied with ACK to server FIN");
    magi_test::expect(stats, client.getStateString() == std::string("TIME_WAIT"), "Client entered TIME_WAIT after receiving FIN in FIN_WAIT_2");

    // Server processes ACK for its FIN and should reach CLOSED
    server.handleIncomingSegment(*ackFromClient);
    magi_test::expect(stats, server.isClosed(), "Server reached CLOSED after ACK for FIN");

    std::cout << std::endl
              << "Milestone 3 teardown summary: " << (stats.checks - stats.failed)
              << "/" << stats.checks << " checks passed" << std::endl;

    return stats.failed == 0;
}
