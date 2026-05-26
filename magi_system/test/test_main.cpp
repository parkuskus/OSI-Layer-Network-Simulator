/*
Run instructions:
1) From the repository root, compile this runner together with the milestone tests and the production sources.
2) Example manual command (PowerShell / g++):
    g++ -std=c++11 -I. test/test_main.cpp test/milestone-1/test_m1.cpp test/milestone-2/test_m2.cpp test/milestone-3/test_m3.cpp test/milestone-4/test_m4.cpp test/milestone-5/test_m5.cpp core/packet.cpp core/interface.cpp core/link.cpp core/node.cpp layer2/ethernet.cpp layer2/arp.cpp layer2/host.cpp layer2/switch.cpp layer3/ipv4.cpp layer3/icmp.cpp layer4/tcp.cpp layer4/udp.cpp layer4/tcp_socket.cpp layer7/magi_socket.cpp layer7/udp_socket.cpp layer7/dhcp_server.cpp layer7/dhcp_client.cpp layer7/dns_server.cpp layer7/dns_client.cpp layer7/http_server.cpp layer7/http_client.cpp layer3/acl.cpp layer3/nat.cpp layer3/rip.cpp -o unit_tests.exe
3) Execute the resulting binary:
    .\unit_tests.exe

Milestone 3 is covered by a single test file, and that file now includes the TCP teardown checks as well.
Milestone 4 currently starts with the TCP socket wrapper smoke test.
Milestone 5 covers IP fragmentation & reassembly.

This file is intentionally self-contained and does not require Makefile changes.
*/

#include <iostream>
#include <string>

bool runMilestone1Tests();
bool runMilestone2Tests();
bool runMilestone3Tests();
bool runMilestone4Tests();
bool runMilestone5Tests();
bool runComprehensiveTests();

int main()
{
    std::cout << "MAGI System test runner" << std::endl;
    std::cout << "Milestone 1-5 + Comprehensive Edge-Case Tests" << std::endl;

    int failedMilestones = 0;

    if (!runMilestone1Tests())
    {
        ++failedMilestones;
    }
    if (!runMilestone2Tests())
    {
        ++failedMilestones;
    }
    if (!runMilestone3Tests())
    {
        ++failedMilestones;
    }
    if (!runMilestone4Tests())
    {
        ++failedMilestones;
    }
    if (!runMilestone5Tests())
    {
        ++failedMilestones;
    }
    if (!runComprehensiveTests())
    {
        ++failedMilestones;
    }

    if (failedMilestones == 0)
    {
        std::cout << std::endl
                  << "All milestone and comprehensive tests passed." << std::endl;
        return 0;
    }

    std::cout << std::endl
              << failedMilestones << " test group(s) failed." << std::endl;
    return 1;
}
