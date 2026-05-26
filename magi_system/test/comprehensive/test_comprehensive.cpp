/*
Comprehensive edge-case tests for MAGI System.
Run via test/test_main.cpp runner.
*/

#include "../test_common.hpp"

#include "cli.hpp"
#include "core/interface.hpp"
#include "core/link.hpp"
#include "core/node.hpp"
#include "layer2/ethernet.hpp"
#include "layer2/arp.hpp"
#include "layer2/host.hpp"
#include "layer2/switch.hpp"
#include "layer3/ipv4.hpp"
#include "layer3/ip_utils.hpp"
#include "layer3/icmp.hpp"
#include "layer3/acl.hpp"
#include "layer3/nat.hpp"
#include "layer3/rip.hpp"
#include "layer4/tcp.hpp"
#include "layer4/tcp_socket.hpp"
#include "layer4/udp.hpp"
#include "layer7/magi_socket.hpp"
#include "layer7/udp_socket.hpp"
#include "layer7/dhcp_client.hpp"
#include "layer7/dns_client.hpp"
#include "layer7/http_client.hpp"

#include <fstream>
#include <memory>
#include <string>
#include <vector>

namespace
{

    std::vector<uint8_t> bytesFromString(const std::string &text)
    {
        return std::vector<uint8_t>(text.begin(), text.end());
    }

    bool testCLICreateEdgeCases(magi_test::TestStats &stats)
    {
        magi_test::printSection("Comprehensive - CLI create edge cases");

        magi::CLI cli;

        std::string out1 = cli.executeLine("create host H1", false);
        std::string out2 = cli.executeLine("create host H1", false);
        magi_test::expect(stats,magi_test::contains(out2, "sudah ada"),"CLI rejects duplicate node name");

        // Invalid node type
        std::string out3 = cli.executeLine("create firewall FW1", false);
        magi_test::expect(stats,magi_test::contains(out3, "Format create tidak valid"),"CLI rejects invalid node type");

        // Host with extra ports
        std::string out4 = cli.executeLine("create host H2 4", false);
        magi_test::expect(stats,magi_test::contains(out4, "Host hanya dapat memiliki 1 interface"),"CLI rejects host with extra ports");

        // Switch with 0 ports
        std::string out5 = cli.executeLine("create switch SW0 0", false);
        magi_test::expect(stats,magi_test::contains(out5, "harus lebih dari 0"),"CLI rejects switch with 0 ports");

        // Router with 0 ports
        std::string out6 = cli.executeLine("create router R0 0", false);
        magi_test::expect(stats,magi_test::contains(out6, "harus lebih dari 0"),"CLI rejects router with 0 ports");

        return stats.failed == 0;
    }

    bool testCLILinkEdgeCases(magi_test::TestStats &stats)
    {
        magi_test::printSection("Comprehensive - CLI link edge cases");

        magi::CLI cli;
        cli.executeLine("create host H1", false);
        cli.executeLine("create host H2", false);
        cli.executeLine("create switch SW1 2", false);

        // Link non-existent node
        std::string out1 = cli.executeLine("link H1 SW99:1", false);
        magi_test::expect(stats,magi_test::contains(out1, "tidak ditemukan"),"CLI rejects link to non-existent node");

        // Link invalid port
        std::string out2 = cli.executeLine("link H1 SW1:99", false);
        magi_test::expect(stats,magi_test::contains(out2, "tidak ditemukan"),"CLI rejects link to invalid port");

        // Valid link
        std::string out3 = cli.executeLine("link H1 SW1:1", false);
        magi_test::expect(stats,magi_test::contains(out3, "Berhasil menghubungkan"),"CLI accepts valid link");

        std::string out4 = cli.executeLine("link H2 SW1:1", false);
        magi_test::expect(stats,magi_test::contains(out4, "sudah terhubung"),"CLI rejects link to already connected port");

        std::string out5 = cli.executeLine("unlink H1 SW1:2", false);
        magi_test::expect(stats,magi_test::contains(out5, "tidak ditemukan"),"CLI rejects unlink for non-existent connection");

        std::string out6 = cli.executeLine("link H1:0 H2", false);
        magi_test::expect(stats,magi_test::contains(out6, "Format endpoint tidak valid"),"CLI rejects invalid endpoint format (port 0)");


        std::string out7 = cli.executeLine("link H1 SW1:2 0 10", false);
        magi_test::expect(stats,magi_test::contains(out7, "mtu terlalu kecil"),"CLI rejects MTU below minimum");

        return stats.failed == 0;
    }

    bool testCLISetIpGatewayEdgeCases(magi_test::TestStats &stats)
    {
        magi_test::printSection("Comprehensive - CLI setip/setgw edge cases");

        magi::CLI cli;
        cli.executeLine("create host H1", false);

        
        std::string out1 = cli.executeLine("setip H1 999.999.999.999/24", false);
        magi_test::expect(stats,magi_test::contains(out1, "tidak valid"),"CLI rejects invalid IP address");

        // Valid IP
        std::string out2 = cli.executeLine("setip H1 192.168.1.10/24", false);
        magi_test::expect(stats,magi_test::contains(out2, "di-set ke"),"CLI accepts valid IP address");

        // Invalid gateway (setgw does not validate IP format)
        std::string out3 = cli.executeLine("setgw H1 not_an_ip", false);
        magi_test::expect(stats,magi_test::contains(out3, "di-set ke"),"CLI accepts any gateway string (no validation)");

        // Valid gateway
        std::string out4 = cli.executeLine("setgw H1 192.168.1.1", false);
        magi_test::expect(stats,magi_test::contains(out4, "di-set ke"),"CLI accepts valid gateway");

        // Set IP on non-host (switch)
        cli.executeLine("create switch SW1 2", false);
        std::string out5 = cli.executeLine("setip SW1 192.168.1.2/24", false);
        magi_test::expect(stats,magi_test::contains(out5, "Gunakan format router"),"CLI rejects setip on switch");

        return stats.failed == 0;
    }

    bool testCLITopologyShowSaveLoad(magi_test::TestStats &stats)
    {
        magi_test::printSection("Comprehensive - CLI topology/show/save/load");

        magi::CLI cli;
        cli.executeLine("create host H1", false);
        cli.executeLine("create switch SW1 4", false);
        cli.executeLine("link H1 SW1:1", false);
        cli.executeLine("setip H1 10.0.0.2/24", false);
        cli.executeLine("setgw H1 10.0.0.1", false);

        std::string topoOut = cli.executeLine("topology", false);
        magi_test::expect(stats,magi_test::contains(topoOut, "H1 (host)"),"topology shows host node");
        magi_test::expect(stats,magi_test::contains(topoOut, "SW1 (switch)"),"topology shows switch node");
        magi_test::expect(stats,magi_test::contains(topoOut, "H1:1 <-> SW1:1"),"topology shows link");

        std::string showOut = cli.executeLine("show H1", false);
        magi_test::expect(stats,magi_test::contains(showOut, "10.0.0.2/24"),"show displays host IP");

        // Save topology
        std::string saveOut = cli.executeLine("save __test_topology.json", false);
        magi_test::expect(stats,magi_test::contains(saveOut, "berhasil disimpan"),"save succeeds");

        // Load topology into fresh CLI
        magi::CLI cli2;
        std::string loadOut = cli2.executeLine("load __test_topology.json", false);
        magi_test::expect(stats,magi_test::contains(loadOut, "berhasil dimuat"),"load succeeds");

        std::string topoAfterLoad = cli2.executeLine("topology", false);
        magi_test::expect(stats,magi_test::contains(topoAfterLoad, "H1 (host)"),"loaded topology contains host");

        std::ofstream minified("__test_topology_minified.json");
        minified << "{\"hosts\":[{\"name\":\"MH1\",\"ip_address\":\"192.168.9.2/24\",\"default_gateway\":\"192.168.9.1\"}],\"switches\":[],\"routers\":[],\"links\":[]}";
        minified.close();

        magi::CLI cli3;
        std::string minifiedLoadOut = cli3.executeLine("load __test_topology_minified.json", false);
        magi_test::expect(stats,magi_test::contains(minifiedLoadOut, "berhasil dimuat"),"load accepts valid minified JSON");
        std::string topoMinified = cli3.executeLine("topology", false);
        magi_test::expect(stats,magi_test::contains(topoMinified, "MH1 (host)"),"minified JSON load creates host");

        // Cleanup test file
        std::remove("__test_topology.json");
        std::remove("__test_topology_minified.json");

        return stats.failed == 0;
    }



    bool testSwitchMacLearningAndForwarding(magi_test::TestStats &stats)
    {
        magi_test::printSection("Comprehensive - switch MAC learning and forwarding");

        auto sender = std::make_shared<magi::Host>("S1", "10.0.0.1/24", "10.0.0.254");
        auto receiver = std::make_shared<magi::Host>("R1", "10.0.0.2/24", "10.0.0.254");
        auto bystander = std::make_shared<magi::Host>("B1", "10.0.0.3/24", "10.0.0.254");
        magi::Switch sw("SW1", 3);

        sender->getInterface(1)->setMacAddress("00:00:00:00:00:11");
        receiver->getInterface(1)->setMacAddress("00:00:00:00:00:22");
        bystander->getInterface(1)->setMacAddress("00:00:00:00:00:33");

        magi::Link::create(sender->getInterface(1), sw.getInterface(1));
        magi::Link::create(receiver->getInterface(1), sw.getInterface(2));
        magi::Link::create(bystander->getInterface(1), sw.getInterface(3));

        // Unicast frame from sender to receiver
        magi::EthernetFrame frame;
        frame.dstMac = "00:00:00:00:00:22";
        frame.srcMac = "00:00:00:00:00:11";
        frame.etherType = static_cast<short>(0x0800);
        frame.vlanId = magi::iputil::kUntaggedVlan;
        frame.payload = {1, 2, 3};


        sender->getInterface(1)->send(frame.toBytes());

        const std::string macTable = magi_test::captureStdout([&]()
                                                              { sw.printMacTable(); });
        magi_test::expect(stats,magi_test::contains(macTable, "00:00:00:00:00:11"),"Switch learns sender MAC");

        magi::EthernetFrame frame2;
        frame2.dstMac = "00:00:00:00:00:11";
        frame2.srcMac = "00:00:00:00:00:22";
        frame2.etherType = static_cast<short>(0x0800);
        frame2.vlanId = magi::iputil::kUntaggedVlan;
        frame2.payload = {4, 5, 6};
        receiver->getInterface(1)->send(frame2.toBytes());

        const std::string macTable2 = magi_test::captureStdout([&]()
                                                               { sw.printMacTable(); });
        magi_test::expect(stats,magi_test::contains(macTable2, "00:00:00:00:00:22"),"Switch learns receiver MAC");

        return stats.failed == 0;
    }

    bool testSwitchTrunkBehavior(magi_test::TestStats &stats)
    {
        magi_test::printSection("Comprehensive - switch trunk behavior");

        auto hostA = std::make_shared<magi::Host>("HA", "10.0.0.1/24", "10.0.0.254");
        auto hostB = std::make_shared<magi::Host>("HB", "10.0.0.2/24", "10.0.0.254");
        magi::Switch sw("SW1", 2);

        hostA->getInterface(1)->setMacAddress("00:00:00:00:00:AA");
        hostB->getInterface(1)->setMacAddress("00:00:00:00:00:BB");

        sw.setTrunkVlan(1, 1);
        sw.setTrunkVlan(2, 1);

        magi::Link::create(hostA->getInterface(1), sw.getInterface(1));
        magi::Link::create(hostB->getInterface(1), sw.getInterface(2));

        // Broadcast on trunk should reach the other trunk port
        magi::EthernetFrame frame;
        frame.dstMac = "ff:ff:ff:ff:ff:ff";
        frame.srcMac = "00:00:00:00:00:AA";
        frame.etherType = static_cast<short>(0x0800);
        frame.vlanId = 10; // tagged with VLAN 10
        frame.payload = {1, 2, 3};

        hostA->getInterface(1)->send(frame.toBytes());

        const std::string macTable = magi_test::captureStdout([&]()
                                                              { sw.printMacTable(); });
        magi_test::expect(stats,magi_test::contains(macTable, "10:00:00:00:00:00:AA"),"Trunk port learns tagged MAC with VLAN 10");

        return stats.failed == 0;
    }

    bool testRouterLongestPrefixMatch(magi_test::TestStats &stats)
    {
        magi_test::printSection("Comprehensive - router longest prefix match");

        magi::Router router("R1", 3);
        router.configureInterface(1, magi::iputil::kUntaggedVlan, "10.0.0.1/24");
        router.configureInterface(2, magi::iputil::kUntaggedVlan, "10.0.1.1/24");
        router.configureInterface(3, magi::iputil::kUntaggedVlan, "10.0.2.1/24");

        // Add overlapping routes
        router.addRoute("10.0.0.0/16", "10.0.1.2", 2, magi::iputil::kUntaggedVlan);
        router.addRoute("10.0.0.0/24", "10.0.0.254", 1, magi::iputil::kUntaggedVlan);

        const std::string table = magi_test::captureStdout([&](){ router.printRoutingTable(); });

        magi_test::expect(stats,magi_test::contains(table, "10.0.0.0/24"),"Routing table contains /24 route");
        magi_test::expect(stats,magi_test::contains(table, "10.0.0.0/16"),"Routing table contains /16 route");

        return stats.failed == 0;
    }

    bool testRouterInvalidRoutes(magi_test::TestStats &stats)
    {
        magi_test::printSection("Comprehensive - router invalid route handling");

        magi::Router router("R1", 2);
        router.configureInterface(1, magi::iputil::kUntaggedVlan, "10.0.0.1/24");

        // Invalid CIDR
        bool added1 = router.addRoute("not-a-cidr", "10.0.0.2", 1, magi::iputil::kUntaggedVlan);
        magi_test::expect(stats, !added1, "Router rejects invalid CIDR route");

        // Invalid next-hop IP
        bool added2 = router.addRoute("10.0.1.0/24", "bad-ip", 1, magi::iputil::kUntaggedVlan);
        magi_test::expect(stats, !added2, "Router rejects invalid next-hop IP");

        // Non-configured interface
        bool added3 = router.addRoute("10.0.1.0/24", "10.0.0.2", 2, magi::iputil::kUntaggedVlan);
        magi_test::expect(stats, !added3, "Router rejects route for non-configured interface");

        return stats.failed == 0;
    }

    bool testRouterTTLAndIcmpErrors(magi_test::TestStats &stats)
    {
        magi_test::printSection("Comprehensive - router TTL and ICMP errors");

        magi::Router router("R1", 2);
        router.configureInterface(1, magi::iputil::kUntaggedVlan, "10.0.0.1/24");
        router.configureInterface(2, magi::iputil::kUntaggedVlan, "10.0.1.1/24");

        magi::Host hostA("H1", "10.0.0.2/24", "10.0.0.1");
        magi::Host hostB("H2", "10.0.1.2/24", "10.0.1.1");

        hostA.getInterface(1)->setMacAddress("00:00:00:00:00:01");
        hostB.getInterface(1)->setMacAddress("00:00:00:00:00:02");
        router.getInterface(1)->setMacAddress("00:00:00:00:00:11");
        router.getInterface(2)->setMacAddress("00:00:00:00:00:12");

        magi::Link::create(hostA.getInterface(1), router.getInterface(1));
        magi::Link::create(hostB.getInterface(1), router.getInterface(2));

        // Cross-subnet ping with TTL=1 should trigger Time Exceeded from router
        // We test this via traceroute which starts with TTL=1
        const std::string traceOut = magi_test::captureStdout([&]()
                                                              { hostA.traceroute("10.0.1.2", 2); });

        magi_test::expect(stats,magi_test::contains(traceOut, "10.0.0.1"),"Traceroute shows router hop (TTL=1)");
        magi_test::expect(stats,magi_test::contains(traceOut, "10.0.1.2"),"Traceroute reaches destination");

        return stats.failed == 0;
    }

    bool testRouterRIPLifecycle(magi_test::TestStats &stats)
    {
        magi_test::printSection("Comprehensive - router RIP lifecycle");

        magi::Router router("R1", 2);
        router.configureInterface(1, magi::iputil::kUntaggedVlan, "10.0.0.1/24");

        // RIP initially disabled
        const std::string ripDisabled = magi_test::captureStdout([&]()
                                                                 { router.printRipRoutes(); });
        magi_test::expect(stats,magi_test::contains(ripDisabled, "tidak aktif"),"RIP shows inactive before enable");

        router.enableRip();
        magi_test::expect(stats, router.isRipEnabled(), "RIP enabled after enableRip");

        router.disableRip();
        magi_test::expect(stats, !router.isRipEnabled(), "RIP disabled after disableRip");

        return stats.failed == 0;
    }

    // =========================================================================
    // ACL Edge Cases
    // =========================================================================

    bool testACLEdgeCases(magi_test::TestStats &stats)
    {
        magi_test::printSection("Comprehensive - ACL edge cases");

        magi::ACLList acl;

        // Permit any-any ICMP
        magi::ACLRule permitIcmp;
        permitIcmp.action = magi::ACLAction::PERMIT;
        permitIcmp.sourceIpCidr = "any";
        permitIcmp.destIpCidr = "any";
        permitIcmp.protocol = magi::ACLProtocol::ICMP;
        acl.addRule(permitIcmp);

        // Deny any-any TCP port 22
        magi::ACLRule denySsh;
        denySsh.action = magi::ACLAction::DENY;
        denySsh.sourceIpCidr = "any";
        denySsh.destIpCidr = "any";
        denySsh.protocol = magi::ACLProtocol::TCP;
        denySsh.destPortRange = magi::ACLPortRange(22);
        acl.addRule(denySsh);

        // Default deny (implicit) for anything not matched
        magi_test::expect(stats,acl.checkPacket("192.168.1.1", "192.168.1.2", magi::ACLProtocol::ICMP, 0, 0),"ACL permits ICMP from any to any");

        magi_test::expect(stats,!acl.checkPacket("192.168.1.1", "192.168.1.2", magi::ACLProtocol::TCP, 12345, 22),"ACL denies TCP to port 22");

        magi_test::expect(stats,acl.checkPacket("192.168.1.1", "192.168.1.2", magi::ACLProtocol::TCP, 12345, 80),"ACL permits TCP to non-22 port (no rule matches -> default permit)");

        // Test with invalid rule removal
        magi_test::expect(stats,!acl.removeRule(999),"ACL removeRule returns false for non-existent rule ID");

        return stats.failed == 0;
    }



    bool testNATEdgeCases(magi_test::TestStats &stats)
    {
        magi_test::printSection("Comprehensive - NAT edge cases");

        magi::NATTable nat;

        // Lookup non-existent mapping
        std::string extIp;
        uint16_t extPort = 0;
        bool found = nat.lookupInternal("10.0.0.1", 1234, 6, extIp, extPort);
        magi_test::expect(stats, !found, "NAT lookupInternal returns false for non-existent mapping");

        // Add duplicate mapping
        nat.addStaticMapping("10.0.0.1", 80, "203.0.113.1", 8080, 6);
        int dupIndex = nat.addStaticMapping("10.0.0.1", 80, "203.0.113.1", 8080, 6);
        magi_test::expect(stats, dupIndex >= 0, "NAT allows duplicate static mapping (overwrites)");

        // Remove non-existent mapping
        bool removed = nat.removeMapping("10.0.0.99", 9999, 6);
        magi_test::expect(stats, !removed, "NAT removeMapping returns false for non-existent mapping");

        return stats.failed == 0;
    }


    bool testHostPingEdgeCases(magi_test::TestStats &stats)
    {
        magi_test::printSection("Comprehensive - host ping edge cases");

        magi::Host hostA("H1", "", "");
        magi::Host hostB("H2", "10.0.0.2/24", "10.0.0.1");

        hostA.getInterface(1)->setMacAddress("00:00:00:00:00:01");
        hostB.getInterface(1)->setMacAddress("00:00:00:00:00:02");
        magi::Link::create(hostA.getInterface(1), hostB.getInterface(1));

        // Ping without IP configured - verify host has no primary IP
        magi_test::expect(stats,hostA.getIpAddress().empty(),"Host without IP has empty IP address");

        // Ping invalid IP
        magi::Host hostC("H3", "10.0.0.3/24", "10.0.0.1");
        const std::string badIpOut = magi_test::captureStdout([&](){ hostC.sendPing("not_an_ip"); });
        magi_test::expect(stats,magi_test::contains(badIpOut, "Target IP tidak valid"),"Ping fails with invalid target IP");

        // Ping different subnet without gateway
        magi::Host hostD("H4", "192.168.1.2/24", "");
        const std::string noGwOut = magi_test::captureStdout([&]()
                                                             { hostD.sendPing("10.0.0.2"); });
        magi_test::expect(stats,magi_test::contains(noGwOut, "default gateway belum di-set"),"Ping fails when no gateway for different subnet");

        return stats.failed == 0;
    }

    bool testHostArpCacheBehavior(magi_test::TestStats &stats)
    {
        magi_test::printSection("Comprehensive - host ARP cache behavior");

        magi::Host hostA("H1", "10.0.0.1/24", "10.0.0.254");
        magi::Host hostB("H2", "10.0.0.2/24", "10.0.0.254");
        magi::Switch sw("SW1", 2);

        hostA.getInterface(1)->setMacAddress("00:00:00:00:00:01");
        hostB.getInterface(1)->setMacAddress("00:00:00:00:00:02");

        magi::Link::create(hostA.getInterface(1), sw.getInterface(1));
        magi::Link::create(hostB.getInterface(1), sw.getInterface(2));

        // First ping populates ARP cache
        magi_test::captureStdout([&]()
                                 { hostA.sendPing("10.0.0.2"); });

        const std::string arpOut = magi_test::captureStdout([&]()
                                                            { hostA.printArpCache(); });
        magi_test::expect(stats,magi_test::contains(arpOut, "10.0.0.2"),"ARP cache contains target IP after ping");

        return stats.failed == 0;
    }

  

    bool testTCPInvalidInputs(magi_test::TestStats &stats)
    {
        magi_test::printSection("Comprehensive - TCP invalid inputs");

        magi::TCPSegment seg;
        seg.sourcePort = 0; // edge case port
        seg.destinationPort = 65535; // max port
        seg.seqNum = 0;
        seg.ackNum = 0;
        seg.flags = static_cast<uint8_t>(TCP_FLAG_SYN);
        seg.updateChecksum("10.0.0.1", "10.0.0.2");

        // Even with bad ports, serialization should still work
        const std::vector<uint8_t> bytes = seg.toBytes();
        magi_test::expect(stats, !bytes.empty(), "TCP segment serializes despite invalid ports");

        // Checksum should still compute
        magi_test::expect(stats,seg.validateChecksum("10.0.0.1", "10.0.0.2"),"TCP checksum validates despite invalid ports");

        // State machine: connection to self should be possible in simulator
        magi::TCPSocket client("10.0.0.1", 50000, "10.0.0.1", 50001);
        client.seqNum = 100;
        auto syn = client.initiateConnection();
        magi_test::expect(stats, syn != nullptr, "TCP client produces SYN even with same IP");

        return stats.failed == 0;
    }

    bool testUDPInvalidInputs(magi_test::TestStats &stats)
    {
        magi_test::printSection("Comprehensive - UDP invalid inputs");

        magi::UDPSegment udp;
        udp.sourcePort = 0;
        udp.destinationPort = 65535;
        udp.payload = {};
        udp.updateChecksum("10.0.0.1", "10.0.0.2");

        const std::vector<uint8_t> bytes = udp.toBytes();
        magi_test::expect(stats, !bytes.empty(), "UDP segment serializes with empty payload");

        magi_test::expect(stats,udp.validateChecksum("10.0.0.1", "10.0.0.2"),"UDP checksum validates with empty payload");

        return stats.failed == 0;
    }


    bool testDHCPNoServer(magi_test::TestStats &stats)
    {
        magi_test::printSection("Comprehensive - DHCP discovery without server");

        magi::Host clientHost("H1", "0.0.0.0/0", "0.0.0.0");
        clientHost.getInterface(1)->setMacAddress("00:00:00:00:00:01");

        const std::string dhcpOut = magi_test::captureStdout([&](){
            std::string ip = magi::DHCPClient::discover(&clientHost, 500, 1);
            if (ip.empty()){
                std::cout << "[DHCP] Discovery failed as expected" << std::endl;
            }});

        magi_test::expect(stats,magi_test::contains(dhcpOut, "failed"),"DHCP discovery fails when no server is present");

        return stats.failed == 0;
    }

    bool testDNSQueryNonExistent(magi_test::TestStats &stats)
    {
        magi_test::printSection("Comprehensive - DNS query for non-existent domain");

        magi::Host server("DNS", "192.168.1.1/24", "192.168.1.254");
        magi::Host client("CLI", "192.168.1.2/24", "192.168.1.254");
        server.getInterface(1)->setMacAddress("00:00:00:00:00:01");
        client.getInterface(1)->setMacAddress("00:00:00:00:00:02");
        magi::Link::create(server.getInterface(1), client.getInterface(1));

        server.startDnsServer();

        auto clientPtr = std::make_shared<magi::Host>(client);
        std::vector<std::shared_ptr<magi::Host>> allHosts = {std::make_shared<magi::Host>(server)};

        // Query a name that does not exist in topology
        std::string resolved = magi::DNSClient::resolve(clientPtr, "nonexistent.host", allHosts);
        magi_test::expect(stats,resolved.empty(),"DNS resolution returns empty for non-existent host");

        server.stopDnsServer();

        return stats.failed == 0;
    }

    bool testHTTPGetWithoutServer(magi_test::TestStats &stats)
    {
        magi_test::printSection("Comprehensive - HTTP GET without running server");

        auto client = std::make_shared<magi::Host>("HC", "192.168.1.10/24", "192.168.1.1");
        auto server = std::make_shared<magi::Host>("HS", "192.168.1.11/24", "192.168.1.1");
        client->getInterface(1)->setMacAddress("00:00:00:00:00:01");
        server->getInterface(1)->setMacAddress("00:00:00:00:00:02");
        magi::Link::create(client->getInterface(1), server->getInterface(1));

        server->startDnsServer();
        // Do NOT start HTTP server

        std::vector<std::shared_ptr<magi::Host>> allHosts = {server, client};

        const std::string httpOut = magi_test::captureStdout([&]()
                                                             { magi::HTTPClient::get(client, "www.HS.com", allHosts); });

        magi_test::expect(stats,!magi_test::contains(httpOut, "HTTP/1.1 200"),"HTTP GET does not return 200 when server is not running");

        server->stopDnsServer();

        return stats.failed == 0;
    }

    bool testHTTPGetWithoutDnsServer(magi_test::TestStats &stats)
    {
        magi_test::printSection("Comprehensive - HTTP GET without DNS server");

        auto client = std::make_shared<magi::Host>("CLI", "192.168.2.10/24", "192.168.2.1");
        auto server = std::make_shared<magi::Host>("WEB", "192.168.2.11/24", "192.168.2.1");
        client->getInterface(1)->setMacAddress("00:00:00:00:02:01");
        server->getInterface(1)->setMacAddress("00:00:00:00:02:02");
        magi::Link::create(client->getInterface(1), server->getInterface(1));

        server->startHttpServer("index.html");
        std::vector<std::shared_ptr<magi::Host>> allHosts = {server, client};

        const std::string httpOut = magi_test::captureStdout([&]()
                                                             { magi::HTTPClient::get(client, "www.web.com", allHosts); });

        magi_test::expect(stats,!magi_test::contains(httpOut, "HTTP/1.1 200"),"HTTP GET without DNS server does not resolve implicitly");
        magi_test::expect(stats,magi_test::contains(httpOut, "Tidak dapat melakukan resolusi"),"HTTP GET reports DNS resolution failure when no DNS server is active");

        server->stopHttpServer();
        return stats.failed == 0;
    }

    

    bool testIPUtilsEdgeCases(magi_test::TestStats &stats)
    {
        magi_test::printSection("Comprehensive - IP utility edge cases");

        magi_test::expect(stats, !magi::iputil::isValidIp(""), "Empty string is invalid IP");
        magi_test::expect(stats, !magi::iputil::isValidIp("256.1.1.1"), "Octet >255 is invalid IP");
        magi_test::expect(stats, !magi::iputil::isValidIp("1.2.3"), "Only 3 octets is invalid IP");
        magi_test::expect(stats, !magi::iputil::isValidIp("1.2.3.4.5"), "5 octets is invalid IP");
        magi_test::expect(stats, magi::iputil::isValidIp("0.0.0.0"), "0.0.0.0 is valid IP");
        magi_test::expect(stats, magi::iputil::isValidIp("255.255.255.255"), "255.255.255.255 is valid IP");

        magi_test::expect(stats, !magi::iputil::isValidCidr("10.0.0.0/33"), "/33 is invalid CIDR");
        magi_test::expect(stats, !magi::iputil::isValidCidr("10.0.0.0/-1"), "Negative prefix is invalid CIDR");
        magi_test::expect(stats, magi::iputil::isValidCidr("10.0.0.0/0"), "/0 is valid CIDR");
        magi_test::expect(stats, magi::iputil::isValidCidr("10.0.0.0/32"), "/32 is valid CIDR");

        magi_test::expect(stats,
                          magi::iputil::stripCidr("192.168.1.1/24") == "192.168.1.1",
                          "stripCidr removes prefix");
        magi_test::expect(stats,
                          magi::iputil::stripCidr("192.168.1.1") == "192.168.1.1",
                          "stripCidr leaves bare IP unchanged");

        magi_test::expect(stats,
                          magi::iputil::ipInCidr("10.0.0.5", "10.0.0.0/24"),
                          "IP in same CIDR");
        magi_test::expect(stats,
                          !magi::iputil::ipInCidr("10.0.1.5", "10.0.0.0/24"),
                          "IP not in different CIDR");
        magi_test::expect(stats,
                          !magi::iputil::ipInCidr("bad-ip", "10.0.0.0/24"),
                          "Invalid IP not in CIDR");
        magi_test::expect(stats,
                          !magi::iputil::ipInCidr("10.0.0.5", "bad-cidr"),
                          "IP not in invalid CIDR");

        return stats.failed == 0;
    }

    

    bool testPacketSerializationEdgeCases(magi_test::TestStats &stats)
    {
        magi_test::printSection("Comprehensive - packet serialization edge cases");

        // Ethernet frame with empty payload
        magi::EthernetFrame emptyFrame;
        emptyFrame.dstMac = "ff:ff:ff:ff:ff:ff";
        emptyFrame.srcMac = "00:11:22:33:44:55";
        emptyFrame.etherType = static_cast<short>(0x0800);
        emptyFrame.vlanId = 1;
        emptyFrame.payload = {};
        const std::vector<uint8_t> emptyBytes = emptyFrame.toBytes();
        magi_test::expect(stats, !emptyBytes.empty(), "Ethernet frame serializes with empty payload");

        magi::EthernetFrame decodedEmpty;
        decodedEmpty.fromBytes(emptyBytes);
        magi_test::expect(stats, decodedEmpty.payload.empty(), "Decoded empty payload remains empty");

        // ARP with broadcast MAC
        magi::ARPMessage arp;
        arp.opcode = 1;
        arp.senderMac = "00:00:00:00:00:00";
        arp.senderIp = "0.0.0.0";
        arp.targetMac = "ff:ff:ff:ff:ff:ff";
        arp.targetIp = "255.255.255.255";
        const std::vector<uint8_t> arpBytes = arp.toBytes();
        magi::ARPMessage arpDecoded;
        arpDecoded.fromBytes(arpBytes);
        magi_test::expect(stats, arpDecoded.targetIp == "255.255.255.255", "ARP round-trip with broadcast IP");

        // IPv4 with maximum TTL
        magi::IPv4Packet ip;
        ip.srcIp = "1.2.3.4";
        ip.dstIp = "5.6.7.8";
        ip.protocol = 6;
        ip.ttl = 255;
        ip.identification = 65535;
        ip.payload = {0xAA, 0xBB};
        ip.updateChecksum();
        magi_test::expect(stats, ip.validateChecksum(), "IPv4 checksum valid with max TTL and ID");

        return stats.failed == 0;
    }

} // namespace

bool runComprehensiveTests()
{
    std::cout << std::endl
              << "Running Comprehensive Edge-Case Tests" << std::endl;

    magi_test::TestStats stats;
    bool ok = true;

    ok = testCLICreateEdgeCases(stats) && ok;
    ok = testCLILinkEdgeCases(stats) && ok;
    ok = testCLISetIpGatewayEdgeCases(stats) && ok;
    ok = testCLITopologyShowSaveLoad(stats) && ok;

    ok = testSwitchMacLearningAndForwarding(stats) && ok;
    ok = testSwitchTrunkBehavior(stats) && ok;

    ok = testRouterLongestPrefixMatch(stats) && ok;
    ok = testRouterInvalidRoutes(stats) && ok;
    ok = testRouterTTLAndIcmpErrors(stats) && ok;
    ok = testRouterRIPLifecycle(stats) && ok;

    ok = testACLEdgeCases(stats) && ok;
    ok = testNATEdgeCases(stats) && ok;

    ok = testHostPingEdgeCases(stats) && ok;
    ok = testHostArpCacheBehavior(stats) && ok;

    ok = testTCPInvalidInputs(stats) && ok;
    ok = testUDPInvalidInputs(stats) && ok;

    ok = testDHCPNoServer(stats) && ok;
    ok = testDNSQueryNonExistent(stats) && ok;
    ok = testHTTPGetWithoutServer(stats) && ok;
    ok = testHTTPGetWithoutDnsServer(stats) && ok;

    ok = testIPUtilsEdgeCases(stats) && ok;
    ok = testPacketSerializationEdgeCases(stats) && ok;

    std::cout << std::endl
              << "Comprehensive summary: " << (stats.checks - stats.failed)
              << "/" << stats.checks << " checks passed" << std::endl;

    return ok && stats.failed == 0;
}
