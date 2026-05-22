/*
Run instructions:
1) From the repository root, compile this runner together with the milestone tests and the production sources.
2) Example manual command (PowerShell / g++):
   g++ -std=c++11 -I. test/test_main.cpp test/milestone-1/test_m1.cpp test/milestone-2/test_m2.cpp test/milestone-3/test_m3.cpp core/packet.cpp core/interface.cpp core/link.cpp core/node.cpp layer2/ethernet.cpp layer2/arp.cpp layer2/host.cpp layer2/switch.cpp layer3/ipv4.cpp layer3/icmp.cpp layer4/tcp.cpp layer4/udp.cpp layer4/tcp_socket.cpp -o magi_system_tests
3) Execute the resulting binary:
   ./magi_system_tests

This file is intentionally self-contained and does not require Makefile changes.
*/

#include <iostream>
#include <string>

bool runMilestone1Tests();
bool runMilestone2Tests();
bool runMilestone3Tests();
bool runMilestone3TeardownTests();

int main()
{
    std::cout << "MAGI System test runner" << std::endl;
    std::cout << "Milestone 1, 2, and 3 tests only" << std::endl;

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
    if (!runMilestone3TeardownTests())
    {
        ++failedMilestones;
    }

    if (failedMilestones == 0)
    {
        std::cout << std::endl
                  << "All milestone tests passed." << std::endl;
        return 0;
    }

    std::cout << std::endl
              << failedMilestones << " milestone test group(s) failed." << std::endl;
    return 1;
}
