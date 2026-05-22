/*
Milestone 3 tests
Run via test/test_main.cpp runner. The tests below verify:
- TCP 3-way handshake and basic state transitions
- TCP receive buffer reassembly for out-of-order payloads
- TCP/UDP checksum generation and serialization round-trip
*/

#include "../test_common.hpp"

#include "layer4/tcp.hpp"
#include "layer4/tcp_socket.hpp"
#include "layer4/udp.hpp"

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

bool runMilestone3Tests()
{
    std::cout << std::endl
              << "Running Milestone 3 tests" << std::endl;

    magi_test::TestStats stats;

    magi_test::printSection("Milestone 3 - TCP segment serialization and checksum");
    magi::TCPSegment segment;
    segment.sourcePort = 40000;
    segment.destinationPort = 80;
    segment.seqNum = 123456;
    segment.ackNum = 654321;
    segment.flags = static_cast<uint8_t>(TCP_FLAG_PSH | TCP_FLAG_ACK);
    segment.windowSize = 4096;
    segment.urgentPointer = 0;
    segment.payload = bytesFromString("MAGI_TCP_TEST");
    segment.updateChecksum("10.0.0.2", "10.0.0.3");

    const std::vector<uint8_t> tcpBytes = segment.toBytes();
    magi::TCPSegment tcpDecoded;
    tcpDecoded.fromBytes(tcpBytes);

    magi_test::expect(stats, tcpDecoded.sourcePort == segment.sourcePort, "TCP source port round-trip");
    magi_test::expect(stats, tcpDecoded.destinationPort == segment.destinationPort, "TCP destination port round-trip");
    magi_test::expect(stats, tcpDecoded.seqNum == segment.seqNum, "TCP seqNum round-trip");
    magi_test::expect(stats, tcpDecoded.ackNum == segment.ackNum, "TCP ackNum round-trip");
    magi_test::expect(stats, tcpDecoded.payload == segment.payload, "TCP payload round-trip");
    magi_test::expect(stats, segment.validateChecksum("10.0.0.2", "10.0.0.3"), "TCP checksum validates with pseudo-header");

    magi_test::printSection("Milestone 3 - TCP handshake state machine");
    magi::TCPSocket client("10.0.0.2", 40000, "10.0.0.3", 80);
    magi::TCPSocket server("10.0.0.3", 80, "10.0.0.2", 40000);
    client.seqNum = 1000;
    server.seqNum = 7000;
    server.setState(magi::TCPState::LISTEN);

    std::shared_ptr<magi::TCPSegment> syn = client.initiateConnection();
    magi_test::expect(stats, syn != nullptr, "Client produces SYN segment");
    magi_test::expect(stats, client.getStateString() == "SYN_SENT", "Client enters SYN_SENT");

    std::shared_ptr<magi::TCPSegment> synAck = server.respondToSyn(*syn);
    magi_test::expect(stats, synAck != nullptr, "Server produces SYN-ACK segment");
    magi_test::expect(stats, server.getStateString() == "SYN_RCVD", "Server enters SYN_RCVD");

    std::shared_ptr<magi::TCPSegment> ack = client.acknowledgeConnection(*synAck);
    magi_test::expect(stats, ack != nullptr, "Client produces final ACK");
    magi_test::expect(stats, client.isConnected(), "Client reaches ESTABLISHED");

    server.handleIncomingSegment(*ack);
    magi_test::expect(stats, server.isConnected(), "Server reaches ESTABLISHED after ACK");

    std::shared_ptr<magi::TCPSegment> data = client.sendData(bytesFromString("OK"));
    magi_test::expect(stats, data != nullptr, "Established client can send data");
    magi_test::expect(stats, data->hasPSH(), "Data segment carries PSH flag");
    magi_test::expect(stats, data->hasACK(), "Data segment carries ACK flag");

    magi_test::printSection("Milestone 3 - TCP receive buffer reassembly");
    magi::TCPSocket receiver("10.0.0.3", 80, "10.0.0.2", 40000);
    receiver.setState(magi::TCPState::ESTABLISHED);
    receiver.nextExpectedSeq = 1000;

    magi::TCPSegment outOfOrder;
    outOfOrder.seqNum = 1005;
    outOfOrder.payload = bytesFromString("world");

    magi::TCPSegment inOrder;
    inOrder.seqNum = 1000;
    inOrder.payload = bytesFromString("hello");

    magi_test::expect(stats, receiver.addToReceiveBuffer(outOfOrder), "Out-of-order TCP segment is buffered");
    magi_test::expect(stats, receiver.addToReceiveBuffer(inOrder), "In-order TCP segment is accepted");
    magi_test::expect(stats, receiver.getReceivedData() == bytesFromString("helloworld"), "Receive buffer reassembles in-order data");

    magi_test::printSection("Milestone 3 - UDP checksum and serialization");
    magi::UDPSegment udp;
    udp.sourcePort = 5000;
    udp.destinationPort = 53;
    udp.payload = bytesFromString("MAGI_UDP_TEST");
    udp.updateChecksum("10.0.0.2", "10.0.0.3");

    const std::vector<uint8_t> udpBytes = udp.toBytes();
    magi::UDPSegment udpDecoded;
    udpDecoded.fromBytes(udpBytes);

    magi_test::expect(stats, udpDecoded.sourcePort == udp.sourcePort, "UDP source port round-trip");
    magi_test::expect(stats, udpDecoded.destinationPort == udp.destinationPort, "UDP destination port round-trip");
    magi_test::expect(stats, udpDecoded.payload == udp.payload, "UDP payload round-trip");
    magi_test::expect(stats, udp.validateChecksum("10.0.0.2", "10.0.0.3"), "UDP checksum validates with pseudo-header");

    std::cout << std::endl
              << "Milestone 3 summary: " << (stats.checks - stats.failed)
              << "/" << stats.checks << " checks passed" << std::endl;
    return stats.failed == 0;
}
