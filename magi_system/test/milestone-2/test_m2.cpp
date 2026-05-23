/*
Milestone 2 tests
Run via test/test_main.cpp runner. The tests below verify:
- IPv4 checksum generation and validation
- Router routing table registration
- Cross-subnet ping and traceroute behavior (TTL + ICMP)
*/

#include "../test_common.hpp"

#include "layer2/host.hpp"
#include "core/interface.hpp"
#include "core/link.hpp"
#include "core/node.hpp"
#include "layer3/ipv4.hpp"
#include "layer3/ip_utils.hpp"

#include <memory>
#include <string>
#include <vector>

bool runMilestone2Tests()
{
    std::cout << std::endl
              << "Running Milestone 2 tests" << std::endl;

    magi_test::TestStats stats;

    magi_test::printSection("Milestone 2 - IPv4 checksum round-trip");
    magi::IPv4Packet packet;
    packet.srcIp = "10.0.0.2";
    packet.dstIp = "10.0.1.2";
    packet.protocol = 1;
    packet.ttl = 64;
    packet.identification = 42;
    packet.payload = {0x08, 0x00, 0x12, 0x34};
    packet.updateChecksum();

    const std::vector<uint8_t> packetBytes = packet.toBytes();
    magi::IPv4Packet decoded;
    decoded.fromBytes(packetBytes);

    magi_test::expect(stats, decoded.srcIp == packet.srcIp, "IPv4 srcIp round-trip");
    magi_test::expect(stats, decoded.dstIp == packet.dstIp, "IPv4 dstIp round-trip");
    magi_test::expect(stats, decoded.protocol == packet.protocol, "IPv4 protocol round-trip");
    magi_test::expect(stats, decoded.validateChecksum(), "IPv4 checksum stays valid");

    magi_test::printSection("Milestone 2 - router route table registration");
    magi::Router router("R1", 2);
    router.configureInterface(1, magi::iputil::kUntaggedVlan, "10.0.0.1/24");
    router.configureInterface(2, magi::iputil::kUntaggedVlan, "10.0.1.1/24");
    const bool routeAdded = router.addRoute("10.0.2.0/24", "10.0.1.2", 2, magi::iputil::kUntaggedVlan);
    magi_test::expect(stats, routeAdded, "Router accepts valid static route");

    const std::string routingTable = magi_test::captureStdout([&]()
                                                              { router.printRoutingTable(); });
    magi_test::expect(stats,
                      magi_test::contains(routingTable, "10.0.2.0/24"),
                      "Routing table prints added route");

    magi_test::printSection("Milestone 2 - cross-subnet ping and traceroute");
    magi::Host hostA("H1", "10.0.0.2/24", "10.0.0.1");
    magi::Host hostB("H2", "10.0.1.2/24", "10.0.1.1");

    hostA.getInterface(1)->setMacAddress("00:00:00:00:20:01");
    hostB.getInterface(1)->setMacAddress("00:00:00:00:20:02");
    router.getInterface(1)->setMacAddress("00:00:00:00:20:11");
    router.getInterface(2)->setMacAddress("00:00:00:00:20:12");

    magi::Link::create(hostA.getInterface(1), router.getInterface(1));
    magi::Link::create(hostB.getInterface(1), router.getInterface(2));

    const std::string pingOutput = magi_test::captureStdout([&]()
                                                            { hostA.sendPing("10.0.1.2"); });
    magi_test::expect(stats,
                      magi_test::contains(pingOutput, "Reply from 10.0.1.2"),
                      "Cross-subnet ping reaches destination");

    const std::string tracerouteOutput = magi_test::captureStdout([&]()
                                                                  { hostA.traceroute("10.0.1.2", 4); });
    magi_test::expect(stats,
                      magi_test::contains(tracerouteOutput, "traceroute to 10.0.1.2"),
                      "Traceroute starts with expected target");
    magi_test::expect(stats,
                      magi_test::contains(tracerouteOutput, "10.0.0.1"),
                      "Traceroute shows first router hop");
    magi_test::expect(stats,
                      magi_test::contains(tracerouteOutput, "10.0.1.2"),
                      "Traceroute reaches final destination");

    std::cout << std::endl
              << "Milestone 2 summary: " << (stats.checks - stats.failed)
              << "/" << stats.checks << " checks passed" << std::endl;
    return stats.failed == 0;
}
