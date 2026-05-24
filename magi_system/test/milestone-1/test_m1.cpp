/*
Milestone 1 tests
Run via test/test_main.cpp runner. The tests below verify:
- Ethernet and ARP serialization round-trip
- Switch MAC learning and VLAN-aware flooding
- Host ARP queueing + same-subnet ping flow
*/

#include "../test_common.hpp"

#include "layer2/arp.hpp"
#include "layer2/ethernet.hpp"
#include "layer2/host.hpp"
#include "layer2/switch.hpp"
#include "core/interface.hpp"
#include "core/link.hpp"
#include "core/node.hpp"
#include "layer3/ip_utils.hpp"

#include <memory>
#include <string>
#include <vector>

namespace
{

    class CaptureNode : public magi::Node
    {
    public:
        std::vector<std::vector<uint8_t>> receivedFrames;

        explicit CaptureNode(const std::string &name, const std::string &macAddress = "00:11:22:33:44:55")
            : magi::Node(name)
        {
            addInterface(macAddress);
        }

        void handleReceive(magi::Interface *, const std::vector<uint8_t> &rawBytes) override
        {
            receivedFrames.push_back(rawBytes);
        }

        std::string getType() const override { return "capture"; }

        std::string toJson() const override { return "{}"; }
    };

    bool testPacketSerialization(magi_test::TestStats &stats)
    {
        magi_test::printSection("Milestone 1 - packet serialization");

        magi::EthernetFrame frame;
        frame.dstMac = "ff:ff:ff:ff:ff:ff";
        frame.srcMac = "00:11:22:33:44:55";
        frame.etherType = static_cast<short>(0x0800);
        frame.vlanId = 10;
        frame.payload = {1, 2, 3, 4};

        const std::vector<uint8_t> frameBytes = frame.toBytes();
        magi::EthernetFrame frameDecoded;
        frameDecoded.fromBytes(frameBytes);

        magi_test::expect(stats, frameDecoded.dstMac == frame.dstMac, "Ethernet dstMac round-trip");
        magi_test::expect(stats, frameDecoded.srcMac == frame.srcMac, "Ethernet srcMac round-trip");
        magi_test::expect(stats, frameDecoded.etherType == frame.etherType, "EtherType round-trip");
        magi_test::expect(stats, frameDecoded.vlanId == frame.vlanId, "Ethernet vlan round-trip");
        magi_test::expect(stats, frameDecoded.payload == frame.payload, "Ethernet payload round-trip");

        magi::ARPMessage arp;
        arp.opcode = 1;
        arp.senderMac = "00:11:22:33:44:55";
        arp.senderIp = "10.0.0.2";
        arp.targetMac = "00:00:00:00:00:00";
        arp.targetIp = "10.0.0.3";

        const std::vector<uint8_t> arpBytes = arp.toBytes();
        magi::ARPMessage arpDecoded;
        arpDecoded.fromBytes(arpBytes);

        magi_test::expect(stats, arpDecoded.opcode == arp.opcode, "ARP opcode round-trip");
        magi_test::expect(stats, arpDecoded.senderMac == arp.senderMac, "ARP senderMac round-trip");
        magi_test::expect(stats, arpDecoded.senderIp == arp.senderIp, "ARP senderIp round-trip");
        magi_test::expect(stats, arpDecoded.targetMac == arp.targetMac, "ARP targetMac round-trip");
        magi_test::expect(stats, arpDecoded.targetIp == arp.targetIp, "ARP targetIp round-trip");

        return stats.failed == 0;
    }

    bool testSwitchVlanAndMacLearning(magi_test::TestStats &stats)
    {
        magi_test::printSection("Milestone 1 - switch VLAN forwarding");

        auto sender = std::make_shared<CaptureNode>("Sender", "00:00:00:00:00:11");
        auto vlan10Receiver = std::make_shared<CaptureNode>("Vlan10", "00:00:00:00:00:22");
        auto vlan20Receiver = std::make_shared<CaptureNode>("Vlan20", "00:00:00:00:00:33");
        magi::Switch sw("SW1", 3);

        sw.setAccessVlan(1, 10);
        sw.setAccessVlan(2, 10);
        sw.setAccessVlan(3, 20);

        magi::Link::create(sender->getInterface(1), sw.getInterface(1));
        magi::Link::create(vlan10Receiver->getInterface(1), sw.getInterface(2));
        magi::Link::create(vlan20Receiver->getInterface(1), sw.getInterface(3));

        magi::EthernetFrame frame;
        frame.dstMac = "ff:ff:ff:ff:ff:ff";
        frame.srcMac = sender->getInterface(1)->getMacAddress();
        frame.etherType = static_cast<short>(0x0800);
        frame.vlanId = magi::iputil::kUntaggedVlan;
        frame.payload = {9, 8, 7};

        sender->getInterface(1)->send(frame.toBytes());

        magi_test::expect(stats, vlan10Receiver->receivedFrames.size() == 1, "Same-VLAN receiver gets flooded frame");
        magi_test::expect(stats, vlan20Receiver->receivedFrames.empty(), "Different-VLAN receiver does not get frame");

        const std::string macTable = magi_test::captureStdout([&]()
                                                              { sw.printMacTable(); });

        magi_test::expect(stats,
                          magi_test::contains(macTable, sender->getInterface(1)->getMacAddress()),
                          "MAC table learns sender MAC");
        magi_test::expect(stats,
                          magi_test::contains(macTable, "port 1"),
                          "MAC table records ingress port");

        return stats.failed == 0;
    }

    bool testHostPingSameSubnet(magi_test::TestStats &stats)
    {
        magi_test::printSection("Milestone 1 - host ARP queue and ping");

        magi::Host hostA("H1", "10.0.0.2/24", "10.0.0.1");
        magi::Host hostB("H2", "10.0.0.3/24", "10.0.0.1");
        magi::Switch sw("SW2", 2);

        hostA.getInterface(1)->setMacAddress("00:00:00:00:10:01");
        hostB.getInterface(1)->setMacAddress("00:00:00:00:10:02");

        magi::Link::create(hostA.getInterface(1), sw.getInterface(1));
        magi::Link::create(hostB.getInterface(1), sw.getInterface(2));

        const std::string output = magi_test::captureStdout([&]()
                                                            { hostA.sendPing("10.0.0.3"); });

        magi_test::expect(stats,
                          magi_test::contains(output, "Reply from 10.0.0.3"),
                          "Ping completes after ARP queue flush");
        magi_test::expect(stats,
                          !magi_test::contains(output, "Request timeout"),
                          "Ping does not timeout on same subnet");

        return stats.failed == 0;
    }

    bool testHostPingRetryAfterVlanRestore(magi_test::TestStats &stats)
    {
        magi_test::printSection("Milestone 1 - ping retry after VLAN restore");

        magi::Host hostA("H1", "10.0.0.2/24", "10.0.0.1");
        magi::Host hostB("H2", "10.0.0.3/24", "10.0.0.1");
        magi::Switch sw("SW3", 2);

        hostA.getInterface(1)->setMacAddress("00:00:00:00:11:01");
        hostB.getInterface(1)->setMacAddress("00:00:00:00:11:02");

        sw.setAccessVlan(1, 10);
        sw.setAccessVlan(2, 20);

        magi::Link::create(hostA.getInterface(1), sw.getInterface(1));
        magi::Link::create(hostB.getInterface(1), sw.getInterface(2));

        const std::string blockedOutput = magi_test::captureStdout([&]()
                                                                   { hostA.sendPing("10.0.0.3"); });
        magi_test::expect(stats,
                          magi_test::contains(blockedOutput, "Request timeout"),
                          "Ping times out when hosts are isolated by VLAN");

        sw.setAccessVlan(2, 10);

        const std::string restoredOutput = magi_test::captureStdout([&]()
                                                                    { hostA.sendPing("10.0.0.3"); });
        magi_test::expect(stats,
                          magi_test::contains(restoredOutput, "Reply from 10.0.0.3"),
                          "Ping succeeds after VLAN is restored");
        magi_test::expect(stats,
                          !magi_test::contains(restoredOutput, "Request timeout"),
                          "Retry does not stay stuck in ARP timeout after VLAN restore");

        return stats.failed == 0;
    }

} // namespace

bool runMilestone1Tests()
{
    std::cout << std::endl
              << "Running Milestone 1 tests" << std::endl;

    magi_test::TestStats stats;
    bool ok = true;

    ok = testPacketSerialization(stats) && ok;
    ok = testSwitchVlanAndMacLearning(stats) && ok;
    ok = testHostPingSameSubnet(stats) && ok;
    ok = testHostPingRetryAfterVlanRestore(stats) && ok;

    std::cout << std::endl
              << "Milestone 1 summary: " << (stats.checks - stats.failed)
              << "/" << stats.checks << " checks passed" << std::endl;
    return ok && stats.failed == 0;
}
