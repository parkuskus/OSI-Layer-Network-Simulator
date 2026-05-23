/*
Milestone 5 fragmentation and reassembly tests.
*/

#include "../test_common.hpp"

#include "core/interface.hpp"
#include "core/link.hpp"
#include "layer2/ethernet.hpp"
#include "layer2/host.hpp"
#include "layer3/ipv4.hpp"
#include "layer3/ip_utils.hpp"
#include "layer4/udp.hpp"
#include "layer7/magi_socket.hpp"
#include "layer7/udp_socket.hpp"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace
{

    std::vector<uint8_t> bytesFromString(const std::string &text)
    {
        return std::vector<uint8_t>(text.begin(), text.end());
    }

    std::vector<uint8_t> makePatternBytes(size_t size)
    {
        std::vector<uint8_t> bytes(size);
        for (size_t i = 0; i < size; ++i)
        {
            bytes[i] = static_cast<uint8_t>('A' + (i % 26));
        }
        return bytes;
    }

    std::vector<magi::IPv4Packet> fragmentIpv4(const magi::IPv4Packet &packet, uint32_t mtu)
    {
        const size_t headerSize = static_cast<size_t>(packet.ihl) * 4;
        const size_t maxPayloadPerFragment = ((mtu > headerSize) ? (mtu - headerSize) : 0) / 8 * 8;

        if (maxPayloadPerFragment == 0 || packet.payload.size() <= maxPayloadPerFragment)
        {
            return std::vector<magi::IPv4Packet>{packet};
        }

        std::vector<magi::IPv4Packet> fragments;
        size_t offset = 0;
        while (offset < packet.payload.size())
        {
            const size_t remaining = packet.payload.size() - offset;
            const size_t chunkSize = (remaining > maxPayloadPerFragment) ? maxPayloadPerFragment : remaining;

            magi::IPv4Packet fragment = packet;
            fragment.flags = static_cast<uint8_t>(packet.flags & static_cast<uint8_t>(~0x1));
            if (remaining > chunkSize)
            {
                fragment.flags = static_cast<uint8_t>(fragment.flags | 0x1);
            }
            fragment.fragmentOffset = static_cast<uint16_t>(offset / 8);
            fragment.payload.assign(packet.payload.begin() + static_cast<long>(offset),
                                    packet.payload.begin() + static_cast<long>(offset + chunkSize));
            fragment.updateChecksum();
            fragments.push_back(fragment);

            offset += chunkSize;
        }

        return fragments;
    }

    magi::EthernetFrame makeEthernetFrame(const std::string &srcMac,
                                          const std::string &dstMac,
                                          int vlanId,
                                          const magi::IPv4Packet &packet)
    {
        magi::EthernetFrame frame;
        frame.srcMac = srcMac;
        frame.dstMac = dstMac;
        frame.vlanId = vlanId;
        frame.etherType = static_cast<short>(0x0800);
        frame.payload = packet.toBytes();
        return frame;
    }

} // namespace

bool runMilestone5Tests()
{
    std::cout << std::endl
              << "Running Milestone 5 fragmentation and reassembly tests" << std::endl;

    magi_test::TestStats stats;

    magi_test::printSection("Milestone 5 - TCP fragmentation over MTU-limited link");
    auto serverHost = std::make_shared<magi::Host>("HS", "10.50.0.2/24", "10.50.0.1");
    auto clientHost = std::make_shared<magi::Host>("HC", "10.50.0.1/24", "10.50.0.2");
    serverHost->getInterface(1)->setMacAddress("00:00:00:00:60:01");
    clientHost->getInterface(1)->setMacAddress("00:00:00:00:60:02");
    magi::Link::create(clientHost->getInterface(1), serverHost->getInterface(1), 0, 128);

    magi::MagiSocket serverSocket(serverHost.get(), magi::MagiSocket::AF_INET, magi::MagiSocket::SOCK_STREAM);
    magi::MagiSocket clientSocket(clientHost.get(), magi::MagiSocket::AF_INET, magi::MagiSocket::SOCK_STREAM);

    magi_test::expect(stats, serverSocket.bind("10.50.0.2", 8080), "Server socket bind succeeds");
    magi_test::expect(stats, serverSocket.listen(5), "Server socket listen succeeds");
    magi_test::expect(stats, clientSocket.bind("10.50.0.1", 49152), "Client socket bind succeeds");
    magi_test::expect(stats, clientSocket.connect("10.50.0.2", 8080), "Client socket connect succeeds on MTU-limited link");

    std::vector<uint8_t> bigPayload = makePatternBytes(2048);
    std::string tcpLog = magi_test::captureStdout([&]()
                                                  {
        const size_t sent = clientSocket.send(bigPayload);
        magi_test::expect(stats, sent == bigPayload.size(), "Client sends payload larger than MTU"); });

    std::vector<uint8_t> receivedTcp = serverSocket.recv(4096);
    magi_test::expect(stats, receivedTcp == bigPayload, "Server receives full TCP payload after reassembly");
    magi_test::expect(stats, magi_test::contains(tcpLog, "memecah IPv4 packet"), "Fragmentation log is emitted for large TCP payload");

    magi_test::printSection("Milestone 5 - UDP reassembly from out-of-order fragments");
    magi::Host receiver("HR", "192.168.60.2/24", "192.168.60.1");
    receiver.getInterface(1)->setMacAddress("00:00:00:00:61:02");
    auto udpSocket = std::make_shared<magi::UDPSocket>(&receiver);
    magi_test::expect(stats, udpSocket->bind("192.168.60.2", 5353), "UDP socket bind succeeds");
    receiver.registerUdpSocket(5353, udpSocket);

    magi::IPv4Packet udpPacket;
    udpPacket.srcIp = "192.168.60.1";
    udpPacket.dstIp = "192.168.60.2";
    udpPacket.protocol = 17;
    udpPacket.ttl = 64;
    udpPacket.identification = 77;

    magi::UDPSegment udpSegment;
    udpSegment.sourcePort = 4444;
    udpSegment.destinationPort = 5353;
    udpSegment.payload = makePatternBytes(180);
    udpSegment.updateChecksum(udpPacket.srcIp, udpPacket.dstIp);
    udpPacket.payload = udpSegment.toBytes();
    udpPacket.updateChecksum();

    std::vector<magi::IPv4Packet> fragments = fragmentIpv4(udpPacket, 84);
    magi_test::expect(stats, fragments.size() >= 2, "Manual fragmentation helper produces multiple fragments");

    std::string udpLog = magi_test::captureStdout([&]()
                                                  {
        for (std::vector<magi::IPv4Packet>::reverse_iterator it = fragments.rbegin(); it != fragments.rend(); ++it)
        {
            magi::EthernetFrame frame = makeEthernetFrame("00:00:00:00:61:01",
                                                          "00:00:00:00:61:02",
                                                          magi::iputil::kUntaggedVlan,
                                                          *it);
            receiver.handleReceive(receiver.getInterface(1).get(), frame.toBytes());
        } });

    std::string udpSrcIp;
    uint16_t udpSrcPort = 0;
    std::vector<uint8_t> receivedUdp = udpSocket->recvfrom(udpSrcIp, udpSrcPort, 4096);
    magi_test::expect(stats, udpSrcIp == "192.168.60.1", "UDP reassembly preserves source IP");
    magi_test::expect(stats, udpSrcPort == 4444, "UDP reassembly preserves source port");
    magi_test::expect(stats, receivedUdp == udpSegment.payload, "UDP socket receives payload after out-of-order reassembly");
    magi_test::expect(stats, magi_test::contains(udpLog, "menyimpan fragment IPv4"), "Receiver logs buffered fragments during reassembly");

    magi_test::printSection("Milestone 5 - No fragmentation below MTU");
    auto smallServer = std::make_shared<magi::Host>("HS2", "10.51.0.2/24", "10.51.0.1");
    auto smallClient = std::make_shared<magi::Host>("HC2", "10.51.0.1/24", "10.51.0.2");
    smallServer->getInterface(1)->setMacAddress("00:00:00:00:62:01");
    smallClient->getInterface(1)->setMacAddress("00:00:00:00:62:02");
    magi::Link::create(smallClient->getInterface(1), smallServer->getInterface(1), 0, 256);

    magi::MagiSocket smallServerSocket(smallServer.get(), magi::MagiSocket::AF_INET, magi::MagiSocket::SOCK_STREAM);
    magi::MagiSocket smallClientSocket(smallClient.get(), magi::MagiSocket::AF_INET, magi::MagiSocket::SOCK_STREAM);
    magi_test::expect(stats, smallServerSocket.bind("10.51.0.2", 9090), "Small server bind succeeds");
    magi_test::expect(stats, smallServerSocket.listen(5), "Small server listen succeeds");
    magi_test::expect(stats, smallClientSocket.bind("10.51.0.1", 49153), "Small client bind succeeds");
    magi_test::expect(stats, smallClientSocket.connect("10.51.0.2", 9090), "Small client connect succeeds");

    std::vector<uint8_t> smallPayload = makePatternBytes(64);
    std::string smallLog = magi_test::captureStdout([&]()
                                                    {
        const size_t sent = smallClientSocket.send(smallPayload);
        magi_test::expect(stats, sent == smallPayload.size(), "Client sends payload at or below MTU"); });

    std::vector<uint8_t> receivedSmall = smallServerSocket.recv(1024);
    magi_test::expect(stats, receivedSmall == smallPayload, "Server receives small TCP payload without fragmentation loss");
    magi_test::expect(stats, !magi_test::contains(smallLog, "memecah IPv4 packet"), "No fragmentation log appears below MTU");

    std::cout << std::endl
              << "Milestone 5 summary: " << (stats.checks - stats.failed)
              << "/" << stats.checks << " checks passed" << std::endl;

    return stats.failed == 0;
}