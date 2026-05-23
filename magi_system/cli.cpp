#include "cli.hpp"
#include "core/interface.hpp"
#include "core/link.hpp"
#include "layer3/ip_utils.hpp"
#include "layer4/udp.hpp"
#include "layer4/tcp_socket.hpp"
#include "layer7/http_server.hpp"
#include "layer7/http_client.hpp"
#include "layer7/dhcp_client.hpp"
#include <iostream>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <iomanip>

namespace magi
{

    CLI::CLI() : running(true), ripAutoInterval(3)
    {
    }

    std::string CLI::trim(const std::string &str)
    {
        size_t first = str.find_first_not_of(" \t\n\r");
        if (first == std::string::npos)
            return "";
        size_t last = str.find_last_not_of(" \t\n\r");
        return str.substr(first, last - first + 1);
    }

    std::vector<std::string> CLI::parseCommand(const std::string &input)
    {
        std::vector<std::string> args;
        std::stringstream ss(input);
        std::string arg;

        while (ss >> arg)
        {
            args.push_back(arg);
        }

        return args;
    }

    void CLI::executeCommand(const std::vector<std::string> &args)
    {
        if (args.empty())
            return;

        std::string cmd = args[0];
        std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::tolower);

        if (cmd == "create")
        {
            cmdCreate(args);
        }
        else if (cmd == "setip")
        {
            cmdSetIp(args);
        }
        else if (cmd == "setgw")
        {
            cmdSetGateway(args);
        }
        else if (cmd == "vlan")
        {
            cmdVlan(args);
        }
        else if (cmd == "link")
        {
            cmdLink(args);
        }
        else if (cmd == "unlink")
        {
            cmdUnlink(args);
        }
        else if (cmd == "topology")
        {
            cmdTopology();
        }
        else if (cmd == "save")
        {
            cmdSave(args);
        }
        else if (cmd == "load")
        {
            cmdLoad(args);
        }
        else if (cmd == "show")
        {
            cmdShow(args);
        }
        else if (cmd == "route")
        {
            cmdRoute(args);
        }
        else if (cmd == "acl")
        {
            cmdACL(args);
        }
        else if (cmd == "nat")
        {
            cmdNAT(args);
        }
        else if (cmd == "rip")
        {
            cmdRip(args);
        }
        else if (cmd == "help" || cmd == "?")
        {
            cmdHelp();
        }
        else if (cmd == "exit" || cmd == "quit")
        {
            running = false;
            std::cout << "Menghentikan Magi System Simulator..." << std::endl;
        }
        else
        {
            auto dispatchEntityCommand = [&](const std::string &entityName, const std::string &rawSubcmd, const std::vector<std::string> &extraArgs) -> bool
            {
                auto node = findNode(entityName);
                if (!node)
                    return false;

                std::string subcmd = rawSubcmd;
                std::transform(subcmd.begin(), subcmd.end(), subcmd.begin(), ::tolower);

                if (subcmd == "mac")
                {
                    cmdMac({"mac", entityName});
                    return true;
                }
                if (subcmd == "arp")
                {
                    cmdArp({"arp", entityName});
                    return true;
                }
                if (subcmd == "ping" && !extraArgs.empty())
                {
                    cmdPing({"ping", entityName, extraArgs[0]});
                    return true;
                }
                if (subcmd == "traceroute" && !extraArgs.empty())
                {
                    cmdTraceroute({"traceroute", entityName, extraArgs[0]});
                    return true;
                }
                if (subcmd == "route")
                {
                    std::vector<std::string> routeArgs;
                    routeArgs.push_back("route");
                    routeArgs.push_back(entityName);
                    routeArgs.insert(routeArgs.end(), extraArgs.begin(), extraArgs.end());
                    cmdRoute(routeArgs);
                    return true;
                }

                if (subcmd == "tcp_connect" && extraArgs.size() >= 2)
                {
                    std::vector<std::string> tcpArgs = {"tcp_connect", entityName, extraArgs[0], extraArgs[1]};
                    cmdTcpConnect(tcpArgs);
                    return true;
                }
                if (subcmd == "tcp_close" && extraArgs.size() >= 2)
                {
                    std::vector<std::string> tcpArgs = {"tcp_close", entityName, extraArgs[0], extraArgs[1]};
                    // optional client local port
                    if (extraArgs.size() > 2)
                        tcpArgs.push_back(extraArgs[2]);
                    cmdTcpClose(tcpArgs);
                    return true;
                }
                if (subcmd == "udp_send" && extraArgs.size() >= 3)
                {
                    std::vector<std::string> udpArgs = {"udp_send", entityName, extraArgs[0], extraArgs[1], extraArgs[2]};
                    if (extraArgs.size() > 3)
                    {
                        udpArgs.insert(udpArgs.end(), extraArgs.begin() + 3, extraArgs.end());
                    }
                    cmdUdpSend(udpArgs);
                    return true;
                }
                if (subcmd == "http_get" && !extraArgs.empty())
                {
                    std::vector<std::string> httpArgs = {"http_get", entityName, extraArgs[0]};
                    cmdHttpGet(httpArgs);
                    return true;
                }
                if (subcmd == "http_server" && !extraArgs.empty())
                {
                    std::vector<std::string> httpServerArgs = {"http_server", entityName};
                    httpServerArgs.insert(httpServerArgs.end(), extraArgs.begin(), extraArgs.end());
                    cmdHttpServer(httpServerArgs);
                    return true;
                }
                if (subcmd == "dhcp_discover")
                {
                    std::vector<std::string> dhcpArgs = {"dhcp_discover", entityName};
                    cmdDhcpDiscover(dhcpArgs);
                    return true;
                }
                if (subcmd == "dns_server" && !extraArgs.empty())
                {
                    std::vector<std::string> dnsArgs = {"dns_server", entityName};
                    dnsArgs.insert(dnsArgs.end(), extraArgs.begin(), extraArgs.end());
                    cmdDnsServer(dnsArgs);
                    return true;
                }
                return false;
            };

            // Format 1: <entity> <subcommand> [args]
            if (args.size() >= 2)
            {
                std::vector<std::string> extraArgs;
                if (args.size() > 2)
                {
                    extraArgs.assign(args.begin() + 2, args.end());
                }
                if (dispatchEntityCommand(args[0], args[1], extraArgs))
                {
                    return;
                }
            }

            // Format 2 (alias): <subcommand> <entity> [args]
            if ((cmd == "mac" || cmd == "arp" || cmd == "ping" || cmd == "traceroute" ||
                 cmd == "tcp_connect" || cmd == "udp_send" || cmd == "http_get" || cmd == "http_server" ||
                 cmd == "dhcp_discover" || cmd == "dns_server") &&
                args.size() >= 2)
            {
                std::vector<std::string> extraArgs;
                if (args.size() > 2)
                {
                    extraArgs.assign(args.begin() + 2, args.end());
                }
                if (dispatchEntityCommand(args[1], cmd, extraArgs))
                {
                    return;
                }
            }

            std::cout << "Perintah tidak dikenal: " << cmd << std::endl;
            std::cout << "Ketik 'help' untuk melihat daftar perintah." << std::endl;
        }
    }

    void CLI::cmdTcpClose(const std::vector<std::string> &args)
    {
        if (args.size() < 4)
        {
            std::cout << "Penggunaan: <host_name> tcp_close <target_ip> <port> [client_local_port]" << std::endl;
            return;
        }

        auto sourceNode = findNode(args[1]);
        if (!sourceNode)
        {
            std::cout << "Error: Node '" << args[1] << "' tidak ditemukan." << std::endl;
            return;
        }

        auto sourceHost = std::dynamic_pointer_cast<Host>(sourceNode);
        if (!sourceHost)
        {
            std::cout << "Error: Node '" << args[1] << "' bukan host." << std::endl;
            return;
        }

        const std::string targetIp = args[2];
        if (!iputil::isValidIp(targetIp))
        {
            std::cout << "Error: Target IP tidak valid." << std::endl;
            return;
        }

        uint16_t targetPort = 0;
        try
        {
            const unsigned long parsedPort = std::stoul(args[3]);
            if (parsedPort == 0 || parsedPort > 65535)
            {
                std::cout << "Error: Port harus di rentang 1-65535." << std::endl;
                return;
            }
            targetPort = static_cast<uint16_t>(parsedPort);
        }
        catch (...)
        {
            std::cout << "Error: Port tidak valid." << std::endl;
            return;
        }

        // optional client local port (ephemeral). Default to 49152 to match tcp_connect.
        uint16_t clientLocalPort = 49152;
        if (args.size() >= 5)
        {
            try
            {
                const unsigned long parsed = std::stoul(args[4]);
                if (parsed == 0 || parsed > 65535)
                    throw std::runtime_error("bad");
                clientLocalPort = static_cast<uint16_t>(parsed);
            }
            catch (...)
            {
                std::cout << "Error: client_local_port tidak valid." << std::endl;
                return;
            }
        }

        // find target host in topology
        std::shared_ptr<Host> targetHost;
        for (std::map<std::string, std::shared_ptr<Node>>::const_iterator it = nodes.begin(); it != nodes.end(); ++it)
        {
            std::shared_ptr<Host> candidate = std::dynamic_pointer_cast<Host>(it->second);
            if (!candidate)
                continue;
            if (iputil::stripCidr(candidate->getIpAddress()) == targetIp)
            {
                targetHost = candidate;
                break;
            }
        }

        if (!targetHost)
        {
            std::cout << "Error: Tidak ada host dengan IP " << targetIp << " di topologi." << std::endl;
            return;
        }

        const std::string sourceIp = iputil::stripCidr(sourceHost->getIpAddress());
        if (!iputil::isValidIp(sourceIp))
        {
            std::cout << "Error: IP host sumber belum valid/terkonfigurasi." << std::endl;
            return;
        }

        // Step 1: send FIN from client -> server
        bool sentClientFin = sourceHost->initiateCloseToRemote(sourceIp, targetIp, targetPort);
        if (!sentClientFin)
        {
            std::cout << "[TCP] Gagal mengirim FIN dari client (tidak ada socket aktif)." << std::endl;
        }
        else
        {
            std::cout << "[TCP] FIN dikirim dari " << args[1] << " ke " << targetIp << ":" << targetPort << std::endl;
        }

        // Step 2: send FIN from server -> client (use clientLocalPort)
        bool sentServerFin = targetHost->initiateCloseToRemote(targetIp, sourceIp, clientLocalPort);
        if (!sentServerFin)
        {
            std::cout << "[TCP] Gagal mengirim FIN dari server (tidak ada socket aktif)." << std::endl;
        }
        else
        {
            std::cout << "[TCP] FIN dikirim dari target server ke client (ephemeral port " << clientLocalPort << ")." << std::endl;
        }
    }

    void CLI::cmdPing(const std::vector<std::string> &args)
    {
        if (args.size() < 3)
        {
            std::cout << "Penggunaan: ping <host_name> <target_ip> atau <host_name> ping <target_ip>" << std::endl;
            return;
        }

        auto node = findNode(args[1]);
        if (!node)
        {
            std::cout << "Error: Node '" << args[1] << "' tidak ditemukan." << std::endl;
            return;
        }

        auto host = std::dynamic_pointer_cast<Host>(node);
        if (!host)
        {
            std::cout << "Error: Node '" << args[1] << "' bukan host." << std::endl;
            return;
        }

        host->sendPing(args[2]);
    }

    void CLI::cmdTraceroute(const std::vector<std::string> &args)
    {
        if (args.size() < 3)
        {
            std::cout << "Penggunaan: traceroute <host_name> <target_ip> atau <host_name> traceroute <target_ip>" << std::endl;
            return;
        }

        auto node = findNode(args[1]);
        if (!node)
        {
            std::cout << "Error: Node '" << args[1] << "' tidak ditemukan." << std::endl;
            return;
        }

        auto host = std::dynamic_pointer_cast<Host>(node);
        if (!host)
        {
            std::cout << "Error: Node '" << args[1] << "' bukan host." << std::endl;
            return;
        }

        host->traceroute(args[2]);
    }

    void CLI::cmdTcpConnect(const std::vector<std::string> &args)
    {
        if (args.size() < 4)
        {
            std::cout << "Penggunaan: <host_name> tcp_connect <target_ip> <port>" << std::endl;
            return;
        }

        auto sourceNode = findNode(args[1]);
        if (!sourceNode)
        {
            std::cout << "Error: Node '" << args[1] << "' tidak ditemukan." << std::endl;
            return;
        }

        auto sourceHost = std::dynamic_pointer_cast<Host>(sourceNode);
        if (!sourceHost)
        {
            std::cout << "Error: Node '" << args[1] << "' bukan host." << std::endl;
            return;
        }

        const std::string targetIp = args[2];
        if (!iputil::isValidIp(targetIp))
        {
            std::cout << "Error: Target IP tidak valid." << std::endl;
            return;
        }

        uint16_t targetPort = 0;
        try
        {
            const unsigned long parsedPort = std::stoul(args[3]);
            if (parsedPort == 0 || parsedPort > 65535)
            {
                std::cout << "Error: Port harus di rentang 1-65535." << std::endl;
                return;
            }
            targetPort = static_cast<uint16_t>(parsedPort);
        }
        catch (...)
        {
            std::cout << "Error: Port tidak valid." << std::endl;
            return;
        }

        std::shared_ptr<Host> targetHost;
        for (std::map<std::string, std::shared_ptr<Node>>::const_iterator it = nodes.begin(); it != nodes.end(); ++it)
        {
            std::shared_ptr<Host> candidate = std::dynamic_pointer_cast<Host>(it->second);
            if (!candidate)
            {
                continue;
            }

            if (iputil::stripCidr(candidate->getIpAddress()) == targetIp)
            {
                targetHost = candidate;
                break;
            }
        }

        if (!targetHost)
        {
            std::cout << "Error: Tidak ada host dengan IP " << targetIp << " di topologi." << std::endl;
            return;
        }

        const std::string sourceIp = iputil::stripCidr(sourceHost->getIpAddress());
        if (!iputil::isValidIp(sourceIp))
        {
            std::cout << "Error: IP host sumber belum valid/terkonfigurasi." << std::endl;
            return;
        }

        const uint16_t ephemeralPort = 49152;

        auto client = std::make_shared<TCPSocket>(sourceIp, ephemeralPort, targetIp, targetPort);
        auto serverSock = std::make_shared<TCPSocket>(targetIp, targetPort, sourceIp, ephemeralPort);
        serverSock->setState(TCPState::LISTEN);

        // Register sockets on hosts so TCP segments travel via IPv4/Ethernet
        targetHost->registerListeningSocket(targetPort, serverSock);
        sourceHost->registerActiveSocket(sourceIp, ephemeralPort, targetIp, targetPort, client);

        std::cout << "[TCP] " << args[1] << " tcp_connect " << targetIp << ":" << targetPort << std::endl;

        std::shared_ptr<TCPSegment> syn = client->initiateConnection();
        if (!syn)
        {
            std::cout << "[TCP] Gagal mengirim SYN." << std::endl;
            return;
        }

        // Build IPv4 packet carrying TCP SYN and send via source host
        IPv4Packet synPacket;
        synPacket.srcIp = sourceIp;
        synPacket.dstIp = targetIp;
        synPacket.protocol = 6;
        synPacket.ttl = 64;
        synPacket.identification = 0;
        synPacket.payload = syn->toBytes();
        synPacket.updateChecksum();

        if (!sourceHost->sendIpv4(synPacket))
        {
            std::cout << "[TCP] Gagal mengirim SYN (network error)." << std::endl;
            return;
        }

        // At this point the network delivery triggers the server to respond and responses
        // are delivered back to the source host synchronously via Interface/Link.
        // Check socket states after the exchange.
        if (client->isConnected() && serverSock->isConnected())
        {
            std::cout << "[TCP] 3-Way Handshake sukses (SYN -> SYN-ACK -> ACK)." << std::endl;
        }
        else
        {
            std::cout << "[TCP] Handshake selesai tapi state belum ESTABLISHED di kedua sisi." << std::endl;
        }
    }

    void CLI::cmdUdpSend(const std::vector<std::string> &args)
    {
        if (args.size() < 5)
        {
            std::cout << "Penggunaan: <host_name> udp_send <target_ip> <source_port> <destination_port> [payload]" << std::endl;
            return;
        }

        auto sourceNode = findNode(args[1]);
        if (!sourceNode)
        {
            std::cout << "Error: Node '" << args[1] << "' tidak ditemukan." << std::endl;
            return;
        }

        auto sourceHost = std::dynamic_pointer_cast<Host>(sourceNode);
        if (!sourceHost)
        {
            std::cout << "Error: Node '" << args[1] << "' bukan host." << std::endl;
            return;
        }

        const std::string sourceIp = iputil::stripCidr(sourceHost->getIpAddress());
        if (!iputil::isValidIp(sourceIp))
        {
            std::cout << "Error: IP host sumber belum valid/terkonfigurasi." << std::endl;
            return;
        }

        const std::string targetIp = args[2];
        if (!iputil::isValidIp(targetIp))
        {
            std::cout << "Error: Target IP tidak valid." << std::endl;
            return;
        }

        uint16_t sourcePort = 0;
        uint16_t destinationPort = 0;
        try
        {
            const unsigned long parsedSourcePort = std::stoul(args[3]);
            const unsigned long parsedDestinationPort = std::stoul(args[4]);
            if (parsedSourcePort == 0 || parsedSourcePort > 65535 ||
                parsedDestinationPort == 0 || parsedDestinationPort > 65535)
            {
                std::cout << "Error: Port harus di rentang 1-65535." << std::endl;
                return;
            }
            sourcePort = static_cast<uint16_t>(parsedSourcePort);
            destinationPort = static_cast<uint16_t>(parsedDestinationPort);
        }
        catch (...)
        {
            std::cout << "Error: Port tidak valid." << std::endl;
            return;
        }

        std::ostringstream payloadStream;
        for (size_t i = 5; i < args.size(); ++i)
        {
            if (i > 5)
            {
                payloadStream << ' ';
            }
            payloadStream << args[i];
        }
        const std::string payloadText = payloadStream.str().empty() ? "MAGI_UDP_TEST" : payloadStream.str();

        UDPSegment segment;
        segment.sourcePort = sourcePort;
        segment.destinationPort = destinationPort;
        segment.payload.assign(payloadText.begin(), payloadText.end());
        segment.updateChecksum(sourceIp, targetIp);

        std::shared_ptr<Host> targetHost;
        for (std::map<std::string, std::shared_ptr<Node>>::const_iterator it = nodes.begin(); it != nodes.end(); ++it)
        {
            std::shared_ptr<Host> candidate = std::dynamic_pointer_cast<Host>(it->second);
            if (candidate && iputil::stripCidr(candidate->getIpAddress()) == targetIp)
            {
                targetHost = candidate;
                break;
            }
        }

        std::cout << "[UDP] " << args[1] << " udp_send " << targetIp << ":" << destinationPort << std::endl;
        std::cout << "[UDP] from " << sourceIp << ":" << sourcePort << std::endl;
        std::cout << "[UDP] payload: " << payloadText << std::endl;
        std::cout << "[UDP] payload size: " << segment.getPayloadSize() << " bytes" << std::endl;
        std::cout << "[UDP] checksum: 0x" << std::hex << std::setw(4) << std::setfill('0')
                  << segment.checksum << std::dec << std::setfill(' ') << std::endl;

        if (targetHost)
        {
            std::cout << "[UDP] target host ditemukan di topologi: " << targetHost->getName() << std::endl;
        }
        else
        {
            std::cout << "[UDP] target host belum ditemukan di topologi; ini uji pembentukan UDP segment." << std::endl;
        }
    }

    void CLI::cmdHttpGet(const std::vector<std::string> &args)
    {
        if (args.size() < 3)
        {
            std::cout << "Penggunaan: <host_name> http_get <url>" << std::endl;
            return;
        }

        auto sourceNode = findNode(args[1]);
        if (!sourceNode)
        {
            std::cout << "Error: Node '" << args[1] << "' tidak ditemukan." << std::endl;
            return;
        }

        auto sourceHost = std::dynamic_pointer_cast<Host>(sourceNode);
        if (!sourceHost)
        {
            std::cout << "Error: Node '" << args[1] << "' bukan host." << std::endl;
            return;
        }

        std::vector<std::shared_ptr<Host>> allHosts;
        for (const auto &pair : nodes)
        {
            auto host = std::dynamic_pointer_cast<Host>(pair.second);
            if (host)
            {
                allHosts.push_back(host);
            }
        }

        HTTPClient::get(sourceHost, args[2], allHosts);
    }

    void CLI::cmdHttpServer(const std::vector<std::string> &args)
    {
        if (args.size() < 3)
        {
            std::cout << "Penggunaan: <host_name> http_server <start|stop> [file]" << std::endl;
            return;
        }

        auto sourceNode = findNode(args[1]);
        if (!sourceNode)
        {
            std::cout << "Error: Node '" << args[1] << "' tidak ditemukan." << std::endl;
            return;
        }

        auto sourceHost = std::dynamic_pointer_cast<Host>(sourceNode);
        if (!sourceHost)
        {
            std::cout << "Error: Node '" << args[1] << "' bukan host." << std::endl;
            return;
        }

        std::string action = args[2];
        std::transform(action.begin(), action.end(), action.begin(), ::tolower);

        if (action == "start")
        {
            const std::string file = (args.size() >= 4) ? args[3] : "index.html";
            sourceHost->startHttpServer(file);
            return;
        }

        if (action == "stop")
        {
            sourceHost->stopHttpServer();
            return;
        }

        std::cout << "Error: Gunakan 'start' atau 'stop'." << std::endl;
    }

    void CLI::cmdDhcpDiscover(const std::vector<std::string> &args)
    {
        if (args.size() < 2)
        {
            std::cout << "Penggunaan: <host_name> dhcp_discover [timeout_ms] [attempts]" << std::endl;
            return;
        }

        auto sourceNode = findNode(args[1]);
        if (!sourceNode)
        {
            std::cout << "Error: Node '" << args[1] << "' tidak ditemukan." << std::endl;
            return;
        }

        auto sourceHost = std::dynamic_pointer_cast<Host>(sourceNode);
        if (!sourceHost)
        {
            std::cout << "Error: Node '" << args[1] << "' bukan host." << std::endl;
            return;
        }

        int timeoutMs = 3000;
        int attempts = 3;

        if (args.size() >= 3)
        {
            timeoutMs = std::max(500, std::atoi(args[2].c_str()));
        }
        if (args.size() >= 4)
        {
            attempts = std::max(1, std::atoi(args[3].c_str()));
        }

        std::cout << "[DHCP] Starting discovery on " << sourceHost->getName()
                  << " (timeout=" << timeoutMs << "ms, attempts=" << attempts << ")..." << std::endl;
        std::string assigned = DHCPClient::discover(sourceHost.get(), timeoutMs, attempts);
        if (!assigned.empty())
        {
            std::cout << "[DHCP] Assigned IP: " << assigned << std::endl;
        }
        else
        {
            std::cout << "[DHCP] Discovery failed." << std::endl;
        }
    }

    void CLI::cmdDhcpServer(const std::vector<std::string> &args)
    {
        if (args.size() < 3)
        {
            std::cout << "Penggunaan: <host_name> dhcp_server <start|stop>" << std::endl;
            return;
        }

        auto sourceNode = findNode(args[1]);
        if (!sourceNode)
        {
            std::cout << "Error: Node '" << args[1] << "' tidak ditemukan." << std::endl;
            return;
        }

        auto sourceHost = std::dynamic_pointer_cast<Host>(sourceNode);
        if (!sourceHost)
        {
            std::cout << "Error: Node '" << args[1] << "' bukan host." << std::endl;
            return;
        }

        std::string action = args[2];
        std::transform(action.begin(), action.end(), action.begin(), ::tolower);

        if (action == "start")
        {
            sourceHost->startDhcpServer();
            return;
        }

        if (action == "stop")
        {
            sourceHost->stopDhcpServer();
            return;
        }

        std::cout << "Error: Gunakan 'start' atau 'stop'." << std::endl;
    }

    void CLI::cmdDnsServer(const std::vector<std::string> &args)
    {
        if (args.size() < 3)
        {
            std::cout << "Penggunaan: <host_name> dns_server <start|stop>" << std::endl;
            return;
        }

        auto sourceNode = findNode(args[1]);
        if (!sourceNode)
        {
            std::cout << "Error: Node '" << args[1] << "' tidak ditemukan." << std::endl;
            return;
        }

        auto sourceHost = std::dynamic_pointer_cast<Host>(sourceNode);
        if (!sourceHost)
        {
            std::cout << "Error: Node '" << args[1] << "' bukan host." << std::endl;
            return;
        }

        std::string action = args[2];
        std::transform(action.begin(), action.end(), action.begin(), ::tolower);

        if (action == "start")
        {
            sourceHost->startDnsServer();
            return;
        }

        if (action == "stop")
        {
            sourceHost->stopDnsServer();
            return;
        }

        std::cout << "Error: Gunakan 'start' atau 'stop'." << std::endl;
    }

    void CLI::cmdCreate(const std::vector<std::string> &args)
    {
        if (args.size() < 3)
        {
            std::cout << "Penggunaan: create <name> <host|switch|router> [jumlah_port]" << std::endl;
            return;
        }

        std::string name = args[1];
        std::string type = args[2];
        std::transform(type.begin(), type.end(), type.begin(), ::tolower);

        if (nodes.find(name) != nodes.end())
        {
            std::cout << "Error: Node dengan nama '" << name << "' sudah ada." << std::endl;
            return;
        }

        std::shared_ptr<Node> node = nullptr;

        if (type == "host")
        {
            if (args.size() >= 4)
            {
                std::cout << "Error: Host hanya dapat memiliki 1 interface." << std::endl;
                std::cout << "Penggunaan: create <name> host" << std::endl;
                return;
            }
            node = std::make_shared<Host>(name);
            std::cout << "Host '" << name << "' berhasil dibuat." << std::endl;
        }
        else if (type == "switch")
        {
            uint32_t numPorts = 24;
            if (args.size() >= 4)
            {
                numPorts = std::stoul(args[3]);
            }
            node = std::make_shared<Switch>(name, numPorts);
            std::cout << "Switch '" << name << "' dengan " << numPorts << " port berhasil dibuat." << std::endl;
        }
        else if (type == "router")
        {
            uint32_t numPorts = 4;
            if (args.size() >= 4)
            {
                numPorts = std::stoul(args[3]);
            }
            node = std::make_shared<Router>(name, numPorts);
            std::cout << "Router '" << name << "' dengan " << numPorts << " port berhasil dibuat." << std::endl;
        }
        else
        {
            std::cout << "Error: Tipe node tidak valid. Gunakan: host, switch, atau router." << std::endl;
            return;
        }

        nodes[name] = node;
    }

    bool CLI::parseEndpoint(const std::string &endpoint, std::string &nodeName, uint32_t &port)
    {
        size_t colonPos = endpoint.find(':');
        if (colonPos != std::string::npos)
        {
            nodeName = endpoint.substr(0, colonPos);
            port = std::stoul(endpoint.substr(colonPos + 1));
        }
        else
        {
            nodeName = endpoint;
            port = 1;
        }
        return true;
    }

    bool CLI::parsePortVlanSpec(const std::string &spec, uint32_t &port, int &vlanId)
    {
        port = 0;
        vlanId = iputil::kUntaggedVlan;

        if (spec.empty())
        {
            return false;
        }

        size_t dotPos = spec.find('.');
        std::string portPart = spec;
        std::string vlanPart = "";
        if (dotPos != std::string::npos)
        {
            portPart = spec.substr(0, dotPos);
            vlanPart = spec.substr(dotPos + 1);
        }

        try
        {
            port = static_cast<uint32_t>(std::stoul(portPart));
            if (!vlanPart.empty())
            {
                vlanId = std::stoi(vlanPart);
            }
        }
        catch (...)
        {
            return false;
        }

        return port > 0;
    }

    bool CLI::parseRouterInterfaceSpec(const std::string &spec, std::string &nodeName, uint32_t &port, int &vlanId)
    {
        size_t colonPos = spec.find(':');
        if (colonPos == std::string::npos)
        {
            return false;
        }

        nodeName = spec.substr(0, colonPos);
        return parsePortVlanSpec(spec.substr(colonPos + 1), port, vlanId);
    }

    void CLI::cmdLink(const std::vector<std::string> &args)
    {
        if (args.size() < 3)
        {
            std::cout << "Penggunaan: link <device1> <device2> [delay_ms]" << std::endl;
            std::cout << "  Contoh: link H1 SW1:1" << std::endl;
            std::cout << "  Contoh: link SW1:24 R1:1 10" << std::endl;
            return;
        }

        std::string endpoint1 = args[1];
        std::string endpoint2 = args[2];
        uint32_t delay = 0;

        if (args.size() >= 4)
        {
            delay = std::stoul(args[3]);
        }

        std::string nodeAName, nodeBName;
        uint32_t portA, portB;

        parseEndpoint(endpoint1, nodeAName, portA);
        parseEndpoint(endpoint2, nodeBName, portB);

        auto nodeA = findNode(nodeAName);
        auto nodeB = findNode(nodeBName);

        if (!nodeA)
        {
            std::cout << "Error: Node '" << nodeAName << "' tidak ditemukan." << std::endl;
            return;
        }
        if (!nodeB)
        {
            std::cout << "Error: Node '" << nodeBName << "' tidak ditemukan." << std::endl;
            return;
        }

        auto ifaceA = nodeA->getInterface(portA);
        auto ifaceB = nodeB->getInterface(portB);

        if (!ifaceA)
        {
            std::cout << "Error: Port " << portA << " pada '" << nodeAName << "' tidak ditemukan." << std::endl;
            return;
        }
        if (!ifaceB)
        {
            std::cout << "Error: Port " << portB << " pada '" << nodeBName << "' tidak ditemukan." << std::endl;
            return;
        }

        if (ifaceA->isConnected())
        {
            std::cout << "Error: Port " << portA << " pada '" << nodeAName << "' sudah terhubung." << std::endl;
            return;
        }
        if (ifaceB->isConnected())
        {
            std::cout << "Error: Port " << portB << " pada '" << nodeBName << "' sudah terhubung." << std::endl;
            return;
        }

        auto link = Link::create(ifaceA, ifaceB, delay);

        LinkConnection conn;
        conn.nodeAName = nodeAName;
        conn.portA = portA;
        conn.nodeBName = nodeBName;
        conn.portB = portB;
        conn.delay = delay;
        conn.link = link;
        connections.push_back(conn);

        std::cout << "Berhasil menghubungkan " << endpoint1 << " dengan " << endpoint2;
        if (delay > 0)
        {
            std::cout << " (delay: " << delay << " ms)";
        }
        std::cout << "." << std::endl;
    }

    void CLI::cmdUnlink(const std::vector<std::string> &args)
    {
        if (args.size() < 3)
        {
            std::cout << "Penggunaan: unlink <device1> <device2>" << std::endl;
            return;
        }

        std::string endpoint1 = args[1];
        std::string endpoint2 = args[2];

        std::string nodeAName, nodeBName;
        uint32_t portA, portB;

        parseEndpoint(endpoint1, nodeAName, portA);
        parseEndpoint(endpoint2, nodeBName, portB);

        bool found = false;
        for (auto it = connections.begin(); it != connections.end(); ++it)
        {
            if (((it->nodeAName == nodeAName && it->portA == portA &&
                  it->nodeBName == nodeBName && it->portB == portB) ||
                 (it->nodeAName == nodeBName && it->portA == portB &&
                  it->nodeBName == nodeAName && it->portB == portA)))
            {
                it->link->disconnect();
                connections.erase(it);
                found = true;
                break;
            }
        }

        if (found)
        {
            std::cout << "Berhasil memutuskan koneksi antara " << endpoint1 << " dan " << endpoint2 << "." << std::endl;
        }
        else
        {
            std::cout << "Error: Koneksi antara " << endpoint1 << " dan " << endpoint2 << " tidak ditemukan." << std::endl;
        }
    }

    void CLI::cmdTopology()
    {
        std::cout << "=== TOPOLOGY ===" << std::endl;
        std::cout << "Nodes:" << std::endl;
        for (const auto &pair : nodes)
        {
            std::cout << "  " << pair.first << " (" << pair.second->getType() << ")" << std::endl;
        }

        std::cout << "\nLinks:" << std::endl;
        for (const auto &conn : connections)
        {
            std::cout << "  " << conn.nodeAName << ":" << conn.portA
                      << " <-> " << conn.nodeBName << ":" << conn.portB;
            if (conn.delay > 0)
            {
                std::cout << " [" << conn.delay << " ms]";
            }
            std::cout << std::endl;
        }
    }

    void CLI::cmdSave(const std::vector<std::string> &args)
    {
        std::string filename = "topology.json";
        if (args.size() >= 2)
        {
            filename = args[1];
        }

        std::ofstream file(filename);
        if (!file.is_open())
        {
            std::cout << "Error: Tidak dapat membuka file '" << filename << "' untuk menulis." << std::endl;
            return;
        }

        file << "{\n";

        file << "  \"hosts\": [\n";
        bool firstHost = true;
        for (const auto &pair : nodes)
        {
            if (pair.second->getType() == "host")
            {
                if (!firstHost)
                    file << ",\n";
                firstHost = false;
                file << pair.second->toJson();
            }
        }
        file << "\n  ],\n";

        file << "  \"switches\": [\n";
        bool firstSwitch = true;
        for (const auto &pair : nodes)
        {
            if (pair.second->getType() == "switch")
            {
                if (!firstSwitch)
                    file << ",\n";
                firstSwitch = false;
                file << pair.second->toJson();
            }
        }
        file << "\n  ],\n";

        file << "  \"routers\": [\n";
        bool firstRouter = true;
        for (const auto &pair : nodes)
        {
            if (pair.second->getType() == "router")
            {
                if (!firstRouter)
                    file << ",\n";
                firstRouter = false;
                file << pair.second->toJson();
            }
        }
        file << "\n  ],\n";

        file << "  \"links\": [\n";
        for (size_t i = 0; i < connections.size(); ++i)
        {
            if (i > 0)
                file << ",\n";
            const auto &conn = connections[i];
            file << "    {\n";
            file << "      \"endpoints\": [\"" << conn.nodeAName;
            if (conn.portA != 1 || findNode(conn.nodeAName)->getType() != "host")
            {
                file << ":" << conn.portA;
            }
            file << "\", \"" << conn.nodeBName;
            if (conn.portB != 1 || findNode(conn.nodeBName)->getType() != "host")
            {
                file << ":" << conn.portB;
            }
            file << "\"],\n";
            file << "      \"delay\": " << conn.delay << "\n";
            file << "    }";
        }
        file << "\n  ]\n";

        file << "}\n";
        file.close();

        std::cout << "Topologi berhasil disimpan ke '" << filename << "'." << std::endl;
    }

    std::string CLI::extractJsonValue(const std::string &line, const std::string &key)
    {
        size_t keyPos = line.find("\"" + key + "\"");
        if (keyPos == std::string::npos)
            return "";

        size_t colonPos = line.find(':', keyPos);
        if (colonPos == std::string::npos)
            return "";

        size_t quoteStart = line.find('"', colonPos);
        if (quoteStart == std::string::npos)
        {
            size_t valueStart = line.find_first_not_of(" \t", colonPos + 1);
            size_t valueEnd = line.find_first_of(",}\n", valueStart);
            return trim(line.substr(valueStart, valueEnd - valueStart));
        }

        size_t quoteEnd = line.find('"', quoteStart + 1);
        if (quoteEnd == std::string::npos)
            return "";

        return line.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
    }

    bool CLI::parseJsonFile(const std::string &filename)
    {
        std::ifstream file(filename);
        if (!file.is_open())
        {
            std::cout << "Error: Tidak dapat membuka file '" << filename << "'." << std::endl;
            return false;
        }

        clearTopology();

        std::string line;
        std::string currentSection = "";

        while (std::getline(file, line))
        {
            line = trim(line);

            if (line.find("\"hosts\"") != std::string::npos)
            {
                currentSection = "hosts";
            }
            else if (line.find("\"switches\"") != std::string::npos)
            {
                currentSection = "switches";
            }
            else if (line.find("\"routers\"") != std::string::npos)
            {
                currentSection = "routers";
            }
            else if (line.find("\"links\"") != std::string::npos)
            {
                currentSection = "links";
            }
            else if (line.find("{") != std::string::npos && !currentSection.empty())
            {
                // Parse object
                if (currentSection == "hosts")
                {
                    std::string name = "";
                    std::string ip = "";
                    std::string gateway = "";

                    // Baca beberapa baris untuk data host
                    while (std::getline(file, line) && line.find("}") == std::string::npos)
                    {
                        if (line.find("\"name\"") != std::string::npos)
                        {
                            name = extractJsonValue(line, "name");
                        }
                        else if (line.find("\"ip_address\"") != std::string::npos)
                        {
                            ip = extractJsonValue(line, "ip_address");
                        }
                        else if (line.find("\"default_gateway\"") != std::string::npos)
                        {
                            gateway = extractJsonValue(line, "default_gateway");
                        }
                    }

                    if (!name.empty())
                    {
                        auto host = std::make_shared<Host>(name, ip, gateway);
                        nodes[name] = host;
                    }
                }
                else if (currentSection == "switches")
                {
                    std::string name = "";
                    uint32_t numPorts = 24;

                    while (std::getline(file, line) && line.find("}") == std::string::npos)
                    {
                        if (line.find("\"name\"") != std::string::npos)
                        {
                            name = extractJsonValue(line, "name");
                        }
                        else if (line.find("\"num_ports\"") != std::string::npos)
                        {
                            std::string portsStr = extractJsonValue(line, "num_ports");
                            if (!portsStr.empty())
                                numPorts = std::stoul(portsStr);
                        }
                    }

                    if (!name.empty())
                    {
                        auto sw = std::make_shared<Switch>(name, numPorts);
                        nodes[name] = sw;
                    }
                }
                else if (currentSection == "routers")
                {
                    std::string name = "";
                    uint32_t numPorts = 4;
                    std::vector<std::pair<std::string, std::string>> interfaceConfigs;
                    struct RouteConfig
                    {
                        std::string destination;
                        std::string nextHop;
                        std::string outInterface;
                    };
                    std::vector<RouteConfig> routeConfigs;

                    int braceDepth = 1;
                    while (braceDepth > 0 && std::getline(file, line))
                    {
                        line = trim(line);

                        if (line.find("\"name\"") != std::string::npos)
                        {
                            name = extractJsonValue(line, "name");
                        }
                        else if (line.find("\"num_ports\"") != std::string::npos)
                        {
                            std::string portsStr = extractJsonValue(line, "num_ports");
                            if (!portsStr.empty())
                                numPorts = std::stoul(portsStr);
                        }
                        else if (line.find("\"endpoint\"") != std::string::npos)
                        {
                            std::string endpoint = extractJsonValue(line, "endpoint");
                            std::string ipAddress = extractJsonValue(line, "ip_address");
                            if (!endpoint.empty() && !ipAddress.empty())
                            {
                                interfaceConfigs.push_back(std::make_pair(endpoint, ipAddress));
                            }
                        }
                        else if (line.find("\"destination\"") != std::string::npos)
                        {
                            RouteConfig routeConfig;
                            routeConfig.destination = extractJsonValue(line, "destination");
                            routeConfig.nextHop = extractJsonValue(line, "next_hop");
                            routeConfig.outInterface = extractJsonValue(line, "out_interface");
                            if (!routeConfig.destination.empty() &&
                                !routeConfig.nextHop.empty() &&
                                !routeConfig.outInterface.empty())
                            {
                                routeConfigs.push_back(routeConfig);
                            }
                        }

                        braceDepth += static_cast<int>(std::count(line.begin(), line.end(), '{'));
                        braceDepth -= static_cast<int>(std::count(line.begin(), line.end(), '}'));
                    }

                    if (!name.empty())
                    {
                        auto router = std::make_shared<Router>(name, numPorts);
                        for (size_t i = 0; i < interfaceConfigs.size(); ++i)
                        {
                            uint32_t port = 0;
                            int vlanId = iputil::kUntaggedVlan;
                            if (parsePortVlanSpec(interfaceConfigs[i].first, port, vlanId))
                            {
                                router->configureInterface(port, vlanId, interfaceConfigs[i].second);
                            }
                        }
                        for (size_t i = 0; i < routeConfigs.size(); ++i)
                        {
                            uint32_t outPort = 0;
                            int outVlanId = iputil::kUntaggedVlan;
                            if (parsePortVlanSpec(routeConfigs[i].outInterface, outPort, outVlanId))
                            {
                                router->addRoute(routeConfigs[i].destination,
                                                 routeConfigs[i].nextHop,
                                                 outPort,
                                                 outVlanId);
                            }
                        }
                        nodes[name] = router;
                    }
                }
                else if (currentSection == "links")
                {
                    std::vector<std::string> endpoints;
                    uint32_t delay = 0;

                    while (std::getline(file, line) && line.find("}") == std::string::npos)
                    {
                        if (line.find("\"endpoints\"") != std::string::npos)
                        {
                            // Parse array endpoints
                            size_t bracketStart = line.find('[');
                            size_t bracketEnd = line.find(']');
                            if (bracketStart != std::string::npos && bracketEnd != std::string::npos)
                            {
                                std::string arrayContent = line.substr(bracketStart + 1, bracketEnd - bracketStart - 1);
                                std::stringstream ss(arrayContent);
                                std::string endpoint;
                                while (std::getline(ss, endpoint, ','))
                                {
                                    endpoint = trim(endpoint);
                                    // Remove quotes
                                    if (endpoint.size() >= 2 && endpoint[0] == '"' && endpoint[endpoint.size() - 1] == '"')
                                    {
                                        endpoint = endpoint.substr(1, endpoint.size() - 2);
                                    }
                                    if (!endpoint.empty())
                                    {
                                        endpoints.push_back(endpoint);
                                    }
                                }
                            }
                        }
                        else if (line.find("\"delay\"") != std::string::npos)
                        {
                            std::string delayStr = extractJsonValue(line, "delay");
                            if (!delayStr.empty())
                                delay = std::stoul(delayStr);
                        }
                    }

                    if (endpoints.size() >= 2)
                    {
                        // Gunakan cmdLink untuk membuat koneksi
                        std::vector<std::string> linkArgs = {"link", endpoints[0], endpoints[1], std::to_string(delay)};
                        cmdLink(linkArgs);
                    }
                }
            }
        }

        file.close();
        return true;
    }

    void CLI::cmdLoad(const std::vector<std::string> &args)
    {
        std::string filename = "topology.json";
        if (args.size() >= 2)
        {
            filename = args[1];
        }

        if (parseJsonFile(filename))
        {
            std::cout << "Topologi berhasil dimuat dari '" << filename << "'." << std::endl;
        }
    }

    void CLI::cmdShow(const std::vector<std::string> &args)
    {
        if (args.size() < 2)
        {
            std::cout << "Penggunaan: show <node_name>" << std::endl;
            return;
        }

        std::string nodeName = args[1];
        auto node = findNode(nodeName);

        if (node)
        {
            node->printInfo();
        }
        else
        {
            std::cout << "Error: Node '" << nodeName << "' tidak ditemukan." << std::endl;
        }
    }

    void CLI::cmdMac(const std::vector<std::string> &args)
    {
        if (args.size() < 2)
        {
            std::cout << "Penggunaan: mac <switch_name> atau <switch_name> mac" << std::endl;
            return;
        }

        auto node = findNode(args[1]);
        if (!node)
        {
            std::cout << "Error: Node '" << args[1] << "' tidak ditemukan." << std::endl;
            return;
        }

        auto sw = std::dynamic_pointer_cast<Switch>(node);
        if (!sw)
        {
            std::cout << "Error: Node '" << args[1] << "' bukan switch." << std::endl;
            return;
        }

        sw->printMacTable();
    }

    void CLI::cmdArp(const std::vector<std::string> &args)
    {
        if (args.size() < 2)
        {
            std::cout << "Penggunaan: arp <host_name|router_name> atau <node> arp" << std::endl;
            return;
        }

        auto node = findNode(args[1]);
        if (!node)
        {
            std::cout << "Error: Node '" << args[1] << "' tidak ditemukan." << std::endl;
            return;
        }

        auto host = std::dynamic_pointer_cast<Host>(node);
        if (host)
        {
            host->printArpCache();
            return;
        }

        auto router = std::dynamic_pointer_cast<Router>(node);
        if (router)
        {
            router->printArpCache();
            return;
        }

        std::cout << "Error: Node '" << args[1] << "' tidak mendukung ARP cache." << std::endl;
    }

    void CLI::cmdRoute(const std::vector<std::string> &args)
    {
        std::string routerName;
        std::string action;
        std::string destinationCidr;
        std::string nextHopIp;
        std::string outInterfaceSpec;

        if (args.size() >= 2 && args[1] == "add")
        {
            if (args.size() < 6)
            {
                std::cout << "Penggunaan: route add <router_name> <dest_cidr> <next_hop_ip> <out_interface>" << std::endl;
                return;
            }
            routerName = args[2];
            action = "add";
            destinationCidr = args[3];
            nextHopIp = args[4];
            outInterfaceSpec = args[5];
        }
        else
        {
            if (args.size() < 2)
            {
                std::cout << "Penggunaan: route <router_name> atau <router> route add <dest_cidr> <next_hop_ip> <out_interface>" << std::endl;
                return;
            }
            routerName = args[1];
            if (args.size() >= 3)
            {
                action = args[2];
            }
            if (args.size() >= 6)
            {
                destinationCidr = args[3];
                nextHopIp = args[4];
                outInterfaceSpec = args[5];
            }
        }

        auto node = findNode(routerName);
        if (!node)
        {
            std::cout << "Error: Node '" << routerName << "' tidak ditemukan." << std::endl;
            return;
        }

        auto router = std::dynamic_pointer_cast<Router>(node);
        if (!router)
        {
            std::cout << "Error: Node '" << routerName << "' bukan router." << std::endl;
            return;
        }

        if (action.empty())
        {
            router->printRoutingTable();
            return;
        }

        std::transform(action.begin(), action.end(), action.begin(), ::tolower);
        if (action != "add")
        {
            std::cout << "Error: Aksi route tidak valid. Gunakan 'add'." << std::endl;
            return;
        }

        uint32_t outPort = 0;
        int outVlanId = iputil::kUntaggedVlan;
        if (!parsePortVlanSpec(outInterfaceSpec, outPort, outVlanId))
        {
            std::cout << "Error: Format out_interface tidak valid. Gunakan <port> atau <port.vlan>." << std::endl;
            return;
        }

        if (!iputil::isValidCidr(destinationCidr))
        {
            std::cout << "Error: Destination CIDR tidak valid." << std::endl;
            return;
        }
        if (!iputil::isValidIp(nextHopIp))
        {
            std::cout << "Error: Next-hop IP tidak valid." << std::endl;
            return;
        }

        if (!router->addRoute(destinationCidr, nextHopIp, outPort, outVlanId))
        {
            std::cout << "Error: Gagal menambah route. Pastikan interface router sudah dikonfigurasi." << std::endl;
            return;
        }

        std::cout << "Route pada router '" << routerName << "' ditambahkan: "
                  << destinationCidr << " via " << nextHopIp
                  << " dev " << outInterfaceSpec << std::endl;
    }

    void CLI::cmdACL(const std::vector<std::string> &args)
    {
        if (args.size() < 2)
        {
            std::cout << "Penggunaan:" << std::endl;
            std::cout << "  acl <router> ingress|egress [list]                 - Tampilkan ACL rules" << std::endl;
            std::cout << "  acl <router> ingress|egress add <action> <src_ip> <dst_ip> [proto] [src_port] [dst_port]" << std::endl;
            std::cout << "  acl <router> ingress|egress remove <rule_id>" << std::endl;
            std::cout << "  acl <router> ingress|egress clear" << std::endl;
            return;
        }

        std::string routerName = args[1];
        auto node = findNode(routerName);
        if (!node)
        {
            std::cout << "Error: Node '" << routerName << "' tidak ditemukan." << std::endl;
            return;
        }

        auto router = std::dynamic_pointer_cast<Router>(node);
        if (!router)
        {
            std::cout << "Error: Node '" << routerName << "' bukan router." << std::endl;
            return;
        }

        if (args.size() < 3)
        {
            std::cout << "Error: Gunakan 'ingress' atau 'egress'." << std::endl;
            return;
        }

        std::string direction = args[2];
        std::transform(direction.begin(), direction.end(), direction.begin(), ::tolower);

        bool isIngress = (direction == "ingress");
        bool isEgress = (direction == "egress");
        if (!isIngress && !isEgress)
        {
            std::cout << "Error: Gunakan 'ingress' atau 'egress'." << std::endl;
            return;
        }

        if (args.size() < 4)
        {
            // Show ACL
            if (isIngress)
                router->printIngressACL();
            else
                router->printEgressACL();
            return;
        }

        std::string subCmd = args[3];
        std::transform(subCmd.begin(), subCmd.end(), subCmd.begin(), ::tolower);

        if (subCmd == "list")
        {
            if (isIngress)
                router->printIngressACL();
            else
                router->printEgressACL();
        }
        else if (subCmd == "add")
        {
            if (args.size() < 7)
            {
                std::cout << "Penggunaan: acl <router> ingress|egress add <action> <src_ip> <dst_ip> [proto] [src_port] [dst_port]" << std::endl;
                return;
            }

            std::string actionStr = args[4];
            std::transform(actionStr.begin(), actionStr.end(), actionStr.begin(), ::tolower);
            ACLAction action = (actionStr == "permit") ? ACLAction::PERMIT : ACLAction::DENY;

            ACLRule rule;
            rule.action = action;
            rule.sourceIpCidr = args[5];
            rule.destIpCidr = args[6];

            // Parse optional protocol
            if (args.size() > 7)
            {
                std::string proto = args[7];
                std::transform(proto.begin(), proto.end(), proto.begin(), ::tolower);
                if (proto == "tcp")
                    rule.protocol = ACLProtocol::TCP;
                else if (proto == "udp")
                    rule.protocol = ACLProtocol::UDP;
                else if (proto == "icmp")
                    rule.protocol = ACLProtocol::ICMP;
                else
                    rule.protocol = ACLProtocol::ANY;
            }

            // Parse optional ports
            if (args.size() > 8 && (rule.protocol == ACLProtocol::TCP || rule.protocol == ACLProtocol::UDP))
            {
                uint16_t srcPort = static_cast<uint16_t>(std::atoi(args[8].c_str()));
                rule.sourcePortRange = ACLPortRange(srcPort);
            }

            if (args.size() > 9 && (rule.protocol == ACLProtocol::TCP || rule.protocol == ACLProtocol::UDP))
            {
                uint16_t dstPort = static_cast<uint16_t>(std::atoi(args[9].c_str()));
                rule.destPortRange = ACLPortRange(dstPort);
            }

            int ruleId;
            if (isIngress)
                ruleId = router->addIngressACLRule(rule);
            else
                ruleId = router->addEgressACLRule(rule);

            std::cout << "ACL rule ditambahkan dengan ID: " << ruleId << std::endl;
        }
        else if (subCmd == "remove")
        {
            if (args.size() < 5)
            {
                std::cout << "Penggunaan: acl <router> ingress|egress remove <rule_id>" << std::endl;
                return;
            }

            int ruleId = std::atoi(args[4].c_str());
            bool success;
            if (isIngress)
                success = router->removeIngressACLRule(ruleId);
            else
                success = router->removeEgressACLRule(ruleId);

            if (success)
                std::cout << "ACL rule " << ruleId << " dihapus." << std::endl;
            else
                std::cout << "Error: ACL rule " << ruleId << " tidak ditemukan." << std::endl;
        }
        else if (subCmd == "clear")
        {
            if (isIngress)
                router->clearIngressACL();
            else
                router->clearEgressACL();

            std::cout << "Semua ACL rules di-clear." << std::endl;
        }
        else
        {
            std::cout << "Error: Sub-command ACL tidak valid. Gunakan: list, add, remove, clear." << std::endl;
        }
    }

    void CLI::cmdNAT(const std::vector<std::string> &args)
    {
        if (args.size() < 2)
        {
            std::cout << "Penggunaan:" << std::endl;
            std::cout << "  nat <router> [list]                            - Tampilkan NAT mappings" << std::endl;
            std::cout << "  nat <router> static <int_ip> <int_port> <ext_ip> <ext_port> <tcp|udp>" << std::endl;
            std::cout << "  nat <router> dynamic <int_ip> <int_port> <ext_ip> <tcp|udp>" << std::endl;
            std::cout << "  nat <router> inside <port>                     - Set port sebagai interface inside" << std::endl;
            std::cout << "  nat <router> outside <port>                    - Set port sebagai interface outside" << std::endl;
            std::cout << "  nat <router> remove <int_ip> <int_port> <tcp|udp>" << std::endl;
            std::cout << "  nat <router> clear                             - Clear semua NAT mappings" << std::endl;
            return;
        }

        std::string routerName = args[1];
        auto node = findNode(routerName);
        if (!node)
        {
            std::cout << "Error: Node '" << routerName << "' tidak ditemukan." << std::endl;
            return;
        }

        auto router = std::dynamic_pointer_cast<Router>(node);
        if (!router)
        {
            std::cout << "Error: Node '" << routerName << "' bukan router." << std::endl;
            return;
        }

        if (args.size() < 3)
        {
            // Show NAT
            router->printNAT();
            return;
        }

        std::string subCmd = args[2];
        std::transform(subCmd.begin(), subCmd.end(), subCmd.begin(), ::tolower);

        if (subCmd == "list")
        {
            router->printNAT();
        }
        else if (subCmd == "static")
        {
            if (args.size() < 8)
            {
                std::cout << "Penggunaan: nat <router> static <int_ip> <int_port> <ext_ip> <ext_port> <tcp|udp>" << std::endl;
                return;
            }

            std::string intIp = args[3];
            uint16_t intPort = static_cast<uint16_t>(std::atoi(args[4].c_str()));
            std::string extIp = args[5];
            uint16_t extPort = static_cast<uint16_t>(std::atoi(args[6].c_str()));
            std::string proto = args[7];
            std::transform(proto.begin(), proto.end(), proto.begin(), ::tolower);

            uint8_t protocol = (proto == "tcp") ? 6 : 17;  // 6 = TCP, 17 = UDP

            router->addStaticNAT(intIp, intPort, extIp, extPort, protocol);
            std::cout << "Static NAT mapping ditambahkan." << std::endl;
        }
        else if (subCmd == "dynamic")
        {
            if (args.size() < 7)
            {
                std::cout << "Penggunaan: nat <router> dynamic <int_ip> <int_port> <ext_ip> <tcp|udp>" << std::endl;
                return;
            }

            std::string intIp = args[3];
            uint16_t intPort = static_cast<uint16_t>(std::atoi(args[4].c_str()));
            std::string extIp = args[5];
            std::string proto = args[6];
            std::transform(proto.begin(), proto.end(), proto.begin(), ::tolower);

            uint8_t protocol = (proto == "tcp") ? 6 : 17;

            router->addDynamicNAT(intIp, intPort, extIp, protocol);
            std::cout << "Dynamic NAT mapping ditambahkan." << std::endl;
        }
        else if (subCmd == "inside")
        {
            if (args.size() < 4)
            {
                std::cout << "Penggunaan: nat <router> inside <port>" << std::endl;
                return;
            }

            uint32_t port = static_cast<uint32_t>(std::atoi(args[3].c_str()));
            router->setNATInside(port);
            std::cout << "Port " << port << " set sebagai NAT inside interface." << std::endl;
        }
        else if (subCmd == "outside")
        {
            if (args.size() < 4)
            {
                std::cout << "Penggunaan: nat <router> outside <port>" << std::endl;
                return;
            }

            uint32_t port = static_cast<uint32_t>(std::atoi(args[3].c_str()));
            router->setNATOutside(port);
            std::cout << "Port " << port << " set sebagai NAT outside interface." << std::endl;
        }
        else if (subCmd == "remove")
        {
            if (args.size() < 6)
            {
                std::cout << "Penggunaan: nat <router> remove <int_ip> <int_port> <tcp|udp>" << std::endl;
                return;
            }

            std::string intIp = args[3];
            uint16_t intPort = static_cast<uint16_t>(std::atoi(args[4].c_str()));
            std::string proto = args[5];
            std::transform(proto.begin(), proto.end(), proto.begin(), ::tolower);

            uint8_t protocol = (proto == "tcp") ? 6 : 17;

            if (router->removeNAT(intIp, intPort, protocol))
                std::cout << "NAT mapping dihapus." << std::endl;
            else
                std::cout << "Error: NAT mapping tidak ditemukan." << std::endl;
        }
        else if (subCmd == "clear")
        {
            router->clearNAT();
            std::cout << "Semua NAT mappings di-clear." << std::endl;
        }
        else
        {
            std::cout << "Error: Sub-command NAT tidak valid." << std::endl;
        }
    }

    void CLI::cmdRip(const std::vector<std::string> &args)
    {
        if (args.size() < 2)
        {
            std::cout << "Penggunaan:" << std::endl;
            std::cout << "  rip enable <router>       - Aktifkan RIP pada router" << std::endl;
            std::cout << "  rip disable <router>      - Nonaktifkan RIP pada router" << std::endl;
            std::cout << "  rip update [router]       - Trigger RIP update manual" << std::endl;
            std::cout << "  rip show [router]         - Tampilkan RIP routes" << std::endl;
            std::cout << "  rip interval <n>          - Set auto-update interval (CLI commands)" << std::endl;
            return;
        }

        std::string subCmd = args[1];
        std::transform(subCmd.begin(), subCmd.end(), subCmd.begin(), ::tolower);

        if (subCmd == "enable")
        {
            if (args.size() < 3)
            {
                std::cout << "Penggunaan: rip enable <router>" << std::endl;
                return;
            }

            auto node = findNode(args[2]);
            if (!node)
            {
                std::cout << "Error: Node '" << args[2] << "' tidak ditemukan." << std::endl;
                return;
            }

            auto router = std::dynamic_pointer_cast<Router>(node);
            if (!router)
            {
                std::cout << "Error: Node '" << args[2] << "' bukan router." << std::endl;
                return;
            }

            if (router->isRipEnabled())
            {
                std::cout << "[RIP] RIP sudah aktif pada router '" << args[2] << "'." << std::endl;
                return;
            }

            router->enableRip();
            // Trigger immediate update so router advertises its connected networks
            router->triggerRipUpdate();
        }
        else if (subCmd == "disable")
        {
            if (args.size() < 3)
            {
                std::cout << "Penggunaan: rip disable <router>" << std::endl;
                return;
            }

            auto node = findNode(args[2]);
            if (!node)
            {
                std::cout << "Error: Node '" << args[2] << "' tidak ditemukan." << std::endl;
                return;
            }

            auto router = std::dynamic_pointer_cast<Router>(node);
            if (!router)
            {
                std::cout << "Error: Node '" << args[2] << "' bukan router." << std::endl;
                return;
            }

            router->disableRip();
        }
        else if (subCmd == "update")
        {
            if (args.size() >= 3)
            {
                auto node = findNode(args[2]);
                if (!node)
                {
                    std::cout << "Error: Node '" << args[2] << "' tidak ditemukan." << std::endl;
                    return;
                }

                auto router = std::dynamic_pointer_cast<Router>(node);
                if (!router)
                {
                    std::cout << "Error: Node '" << args[2] << "' bukan router." << std::endl;
                    return;
                }

                if (!router->isRipEnabled())
                {
                    std::cout << "Error: RIP tidak aktif pada router '" << args[2] << "'." << std::endl;
                    return;
                }

                router->triggerRipUpdate();
                std::cout << "[RIP] RIP update triggered pada '" << args[2] << "'." << std::endl;
            }
            else
            {
                int count = 0;
                for (std::map<std::string, std::shared_ptr<Node>>::const_iterator it = nodes.begin();
                     it != nodes.end(); ++it)
                {
                    auto router = std::dynamic_pointer_cast<Router>(it->second);
                    if (router && router->isRipEnabled())
                    {
                        router->triggerRipUpdate();
                        count++;
                    }
                }
                std::cout << "[RIP] RIP update triggered pada " << count << " router(s)." << std::endl;
            }
        }
        else if (subCmd == "show")
        {
            if (args.size() >= 3)
            {
                auto node = findNode(args[2]);
                if (!node)
                {
                    std::cout << "Error: Node '" << args[2] << "' tidak ditemukan." << std::endl;
                    return;
                }

                auto router = std::dynamic_pointer_cast<Router>(node);
                if (!router)
                {
                    std::cout << "Error: Node '" << args[2] << "' bukan router." << std::endl;
                    return;
                }

                router->printRipRoutes();
            }
            else
            {
                for (std::map<std::string, std::shared_ptr<Node>>::const_iterator it = nodes.begin();
                     it != nodes.end(); ++it)
                {
                    auto router = std::dynamic_pointer_cast<Router>(it->second);
                    if (router)
                    {
                        router->printRipRoutes();
                    }
                }
            }
        }
        else if (subCmd == "interval")
        {
            if (args.size() < 3)
            {
                std::cout << "Penggunaan: rip interval <n>" << std::endl;
                return;
            }

            try
            {
                int val = std::stoi(args[2]);
                if (val < 1)
                {
                    std::cout << "Error: Interval minimal 1." << std::endl;
                    return;
                }
                ripAutoInterval = val;
                std::cout << "[RIP] Auto-update interval di-set ke " << val << " CLI commands." << std::endl;
            }
            catch (...)
            {
                std::cout << "Error: Interval tidak valid." << std::endl;
            }
        }
        else
        {
            std::cout << "Error: Sub-command RIP tidak dikenal. Gunakan: enable, disable, update, show, interval." << std::endl;
        }
    }

    void CLI::triggerAutoRipUpdate()
    {
        static int cmdCounter = 0;
        cmdCounter++;

        if (cmdCounter >= ripAutoInterval)
        {
            cmdCounter = 0;
            for (std::map<std::string, std::shared_ptr<Node>>::const_iterator it = nodes.begin();
                 it != nodes.end(); ++it)
            {
                auto router = std::dynamic_pointer_cast<Router>(it->second);
                if (router && router->isRipEnabled())
                {
                    router->triggerRipUpdate();
                }
            }
        }
        else
        {
            for (std::map<std::string, std::shared_ptr<Node>>::const_iterator it = nodes.begin();
                 it != nodes.end(); ++it)
            {
                auto router = std::dynamic_pointer_cast<Router>(it->second);
                if (router && router->isRipEnabled())
                {
                    router->ageRipRoutes();
                }
            }
        }
    }

    void CLI::cmdSetIp(const std::vector<std::string> &args)
    {
        if (args.size() < 3)
        {
            std::cout << "Penggunaan: setip <host_name|router:port[.vlan]> <ip_address/cidr>" << std::endl;
            return;
        }

        if (!iputil::isValidCidr(args[2]))
        {
            std::cout << "Error: IP address/CIDR tidak valid." << std::endl;
            return;
        }

        std::string routerName;
        uint32_t port = 0;
        int vlanId = iputil::kUntaggedVlan;
        if (parseRouterInterfaceSpec(args[1], routerName, port, vlanId))
        {
            auto node = findNode(routerName);
            if (!node)
            {
                std::cout << "Error: Router '" << routerName << "' tidak ditemukan." << std::endl;
                return;
            }

            auto router = std::dynamic_pointer_cast<Router>(node);
            if (!router)
            {
                std::cout << "Error: Node '" << routerName << "' bukan router." << std::endl;
                return;
            }

            if (!router->getInterface(port))
            {
                std::cout << "Error: Port " << port << " tidak ditemukan pada router '" << routerName << "'." << std::endl;
                return;
            }

            router->configureInterface(port, vlanId, args[2]);
            std::cout << "IP router '" << routerName << "' interface " << port;
            if (vlanId != iputil::kUntaggedVlan)
            {
                std::cout << "." << vlanId;
            }
            std::cout << " di-set ke " << args[2] << std::endl;
            return;
        }

        auto node = findNode(args[1]);
        if (!node)
        {
            std::cout << "Error: Node '" << args[1] << "' tidak ditemukan." << std::endl;
            return;
        }

        auto host = std::dynamic_pointer_cast<Host>(node);
        if (!host)
        {
            std::cout << "Error: Gunakan format router:port[.vlan] untuk interface router." << std::endl;
            return;
        }

        host->setIpAddress(args[2]);
        std::cout << "IP host '" << args[1] << "' di-set ke " << args[2] << std::endl;
    }

    void CLI::cmdSetGateway(const std::vector<std::string> &args)
    {
        if (args.size() < 3)
        {
            std::cout << "Penggunaan: setgw <host_name> <gateway_ip>" << std::endl;
            return;
        }

        auto node = findNode(args[1]);
        if (!node)
        {
            std::cout << "Error: Node '" << args[1] << "' tidak ditemukan." << std::endl;
            return;
        }

        auto host = std::dynamic_pointer_cast<Host>(node);
        if (!host)
        {
            std::cout << "Error: Node '" << args[1] << "' bukan host." << std::endl;
            return;
        }

        host->setDefaultGateway(args[2]);
        std::cout << "Gateway host '" << args[1] << "' di-set ke " << args[2] << std::endl;
    }

    void CLI::cmdVlan(const std::vector<std::string> &args)
    {
        if (args.size() < 4)
        {
            std::cout << "Penggunaan:" << std::endl;
            std::cout << "  vlan access <switch_name> <port> <vlan_id>" << std::endl;
            std::cout << "  vlan trunk <switch_name> <port> [native_vlan]" << std::endl;
            return;
        }

        std::string mode = args[1];
        std::transform(mode.begin(), mode.end(), mode.begin(), ::tolower);
        auto node = findNode(args[2]);
        if (!node)
        {
            std::cout << "Error: Node '" << args[2] << "' tidak ditemukan." << std::endl;
            return;
        }

        auto sw = std::dynamic_pointer_cast<Switch>(node);
        if (!sw)
        {
            std::cout << "Error: Node '" << args[2] << "' bukan switch." << std::endl;
            return;
        }

        int port = std::stoi(args[3]);
        if (!sw->getInterface(static_cast<uint32_t>(port)))
        {
            std::cout << "Error: Port " << port << " tidak ditemukan pada switch '" << args[2] << "'." << std::endl;
            return;
        }

        if (mode == "access")
        {
            if (args.size() < 5)
            {
                std::cout << "Penggunaan: vlan access <switch_name> <port> <vlan_id>" << std::endl;
                return;
            }
            int vlanId = std::stoi(args[4]);
            sw->setAccessVlan(port, vlanId);
            std::cout << "Switch '" << args[2] << "' port " << port << " -> access VLAN " << vlanId << std::endl;
            return;
        }

        if (mode == "trunk")
        {
            int nativeVlan = 1;
            if (args.size() >= 5)
            {
                nativeVlan = std::stoi(args[4]);
            }
            sw->setTrunkVlan(port, nativeVlan);
            std::cout << "Switch '" << args[2] << "' port " << port << " -> trunk (native VLAN " << nativeVlan << ")" << std::endl;
            return;
        }

        std::cout << "Error: Mode VLAN tidak valid. Gunakan access atau trunk." << std::endl;
    }

    std::shared_ptr<Node> CLI::findNode(const std::string &name)
    {
        auto it = nodes.find(name);
        if (it != nodes.end())
        {
            return it->second;
        }
        return nullptr;
    }

    void CLI::clearTopology()
    {
        nodes.clear();
        connections.clear();
    }

    void CLI::cmdHelp()
    {
        std::cout << "=== MAGI SYSTEM SIMULATOR - COMMAND LIST ===" << std::endl;
        std::cout << std::endl;
        std::cout << "Pengujian Jaringan:" << std::endl;
        std::cout << "  <host> ping <ip>                                 - Mengirim ICMP Echo Request" << std::endl;
        std::cout << "  <host> traceroute <ip>                           - Melacak rute hop-by-hop" << std::endl;
        std::cout << "  <host> tcp_connect <ip> <port>                   - Melakukan TCP Handshake" << std::endl;
        std::cout << "  <host> udp_send <ip> <src_port> <dst_port>       - Membuat dan uji UDP segment" << std::endl;
        std::cout << "  <host> http_get <url>                            - Meminta halaman web statis" << std::endl;
        std::cout << "  <host> http_server start [file]                  - Menjalankan web server" << std::endl;
        std::cout << "  <host> http_server stop                          - Mematikan web server" << std::endl;
        std::cout << "  <host> dhcp_discover [ms] [n]                    - Meminta alokasi IP otomatis dengan retry" << std::endl;
        std::cout << "  <host> dns_server start|stop                     - Menjalankan atau menghentikan DNS server" << std::endl;
        std::cout << std::endl;
        std::cout << "Inspeksi Entitas:" << std::endl;
        std::cout << "  <router> route                                   - Menampilkan Routing Table internal router." << std::endl;
        std::cout << "  <router> route add <cidr> <next_hop> <port[.vlan]> - Menambahkan static route" << std::endl;
        std::cout << "  <switch> mac                                     - Menampilkan MAC Address Table internal switch." << std::endl;
        std::cout << "  <host|router> arp                                - Menampilkan ARP Cache." << std::endl;
        std::cout << "  setip <host_name> <ip/cidr>                      - Set IP address host" << std::endl;
        std::cout << "  setip <router:port[.vlan]> <ip/cidr>             - Set IP interface router/sub-interface" << std::endl;
        std::cout << "  setgw <host_name> <gateway_ip>                   - Set default gateway host" << std::endl;
        std::cout << "  <host> ping <target_ip>                          - Kirim ping antar-host" << std::endl;
        std::cout << "  <host> traceroute <target_ip>                    - Lacak hop router menuju target" << std::endl;
        std::cout << "  vlan access <switch> <port> <vlan>               - Set port switch sebagai access" << std::endl;
        std::cout << "  vlan trunk <switch> <port> <native_vlan>         - Set port switch sebagai trunk" << std::endl;
        std::cout << std::endl;
        std::cout << "Firewall & Security (ACL):" << std::endl;
        std::cout << "  acl <router> ingress|egress [list]               - Tampilkan ACL rules" << std::endl;
        std::cout << "  acl <router> ingress|egress add <action> <src_ip> <dst_ip> [proto] [src_port] [dst_port]" << std::endl;
        std::cout << "    Contoh: acl R1 ingress add deny 192.168.1.0/24 10.0.0.0/8 tcp 80 80" << std::endl;
        std::cout << "  acl <router> ingress|egress remove <rule_id>    - Hapus ACL rule" << std::endl;
        std::cout << "  acl <router> ingress|egress clear                - Clear semua ACL rules" << std::endl;
        std::cout << std::endl;
        std::cout << "Network Address Translation (NAT):" << std::endl;
        std::cout << "  nat <router> [list]                              - Tampilkan NAT mappings" << std::endl;
        std::cout << "  nat <router> static <int_ip> <int_port> <ext_ip> <ext_port> <tcp|udp>" << std::endl;
        std::cout << "    Contoh: nat R1 static 192.168.1.100 80 203.0.113.1 8080 tcp" << std::endl;
        std::cout << "  nat <router> dynamic <int_ip> <int_port> <ext_ip> <tcp|udp>" << std::endl;
        std::cout << "    Contoh: nat R1 dynamic 192.168.1.100 443 203.0.113.1 tcp" << std::endl;
        std::cout << "  nat <router> inside <port>                       - Set port sebagai inside interface" << std::endl;
        std::cout << "  nat <router> outside <port>                      - Set port sebagai outside interface" << std::endl;
        std::cout << "  nat <router> remove <int_ip> <int_port> <tcp|udp> - Hapus NAT mapping" << std::endl;
        std::cout << "  nat <router> clear                               - Clear semua NAT mappings" << std::endl;
        std::cout << std::endl;
        std::cout << "Dynamic Routing (RIPv2):" << std::endl;
        std::cout << "  rip enable <router>                              - Aktifkan RIPv2 pada router" << std::endl;
        std::cout << "  rip disable <router>                             - Nonaktifkan RIPv2 pada router" << std::endl;
        std::cout << "  rip update [router]                              - Trigger RIP update manual" << std::endl;
        std::cout << "  rip show [router]                                - Tampilkan RIP routes" << std::endl;
        std::cout << "  rip interval <n>                                 - Set auto-update interval (default: 3 CLI commands)" << std::endl;
        std::cout << "  Catatan: RIP update otomatis setiap " << ripAutoInterval << " perintah CLI" << std::endl;
        std::cout << std::endl;
        std::cout << "Manajemen Topologi:" << std::endl;
        std::cout << "  create <name> <host|switch|router> [jumlah_port] - Membuat node baru" << std::endl;
        std::cout << "  link <device1> <device2> [delay_ms]              - Menghubungkan dua device" << std::endl;
        std::cout << "  unlink <device1> <device2>                       - Memutuskan koneksi" << std::endl;
        std::cout << "  topology                                         - Menampilkan topologi" << std::endl;
        std::cout << "  show <node_name>                                 - Menampilkan info node" << std::endl;
        std::cout << std::endl;
        std::cout << "File Operations:" << std::endl;
        std::cout << "  save [filename]                                  - Menyimpan topologi ke JSON" << std::endl;
        std::cout << "  load [filename]                                  - Memuat topologi dari JSON" << std::endl;
        std::cout << std::endl;
        std::cout << "General:" << std::endl;
        std::cout << "  help                                             - Menampilkan bantuan" << std::endl;
        std::cout << "  exit / quit                                      - Keluar dari simulator" << std::endl;
        std::cout << std::endl;
        std::cout << "Format endpoint: NodeName atau NodeName:Port" << std::endl;
        std::cout << "  Contoh: H1, H1:1, SW1:24" << std::endl;
    }

    void CLI::run()
    {
        std::cout << "╔══════════════════════════════════════════════════════════════╗" << std::endl;
        std::cout << "║        MAGI SYSTEM: OSI Layer Network Simulator              ║" << std::endl;
        std::cout << "╚══════════════════════════════════════════════════════════════╝" << std::endl;
        std::cout << std::endl;
        std::cout << "Ketik 'help' untuk melihat daftar perintah." << std::endl;
        std::cout << std::endl;

        std::string input;

        while (running)
        {
            std::cout << "Magi> ";
            std::getline(std::cin, input);

            // Parse dan eksekusi perintah
            std::vector<std::string> args = parseCommand(input);
            executeCommand(args);

            if (running)
            {
                triggerAutoRipUpdate();
            }

            if (running)
            {
                std::cout << std::endl;
            }
        }
    }

} // namespace magi
