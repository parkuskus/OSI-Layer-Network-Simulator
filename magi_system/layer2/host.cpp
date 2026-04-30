#include "host.hpp"
#include "core/interface.hpp"
#include "layer2/ethernet.hpp"
#include <iostream>
#include <sstream>

namespace {

const short kEtherTypeIpv4 = static_cast<short>(0x0800);
const short kEtherTypeArp = static_cast<short>(0x0806);
const std::string kBroadcastMac = "ff:ff:ff:ff:ff:ff";

std::vector<std::string> splitBy(const std::string& input, char delimiter) {
    std::vector<std::string> parts;
    std::stringstream ss(input);
    std::string item;
    while (std::getline(ss, item, delimiter)) {
        parts.push_back(item);
    }
    return parts;
}

std::string bytesToString(const std::vector<uint8_t>& bytes) {
    return std::string(bytes.begin(), bytes.end());
}

std::vector<uint8_t> stringToBytes(const std::string& s) {
    return std::vector<uint8_t>(s.begin(), s.end());
}

} // namespace

namespace magi {

Host::Host(const std::string& name, const std::string& ipAddress, const std::string& defaultGateway)
    : Node(name), ipAddress(ipAddress), defaultGateway(defaultGateway) {
    // Host biasanya memiliki 1 interface
    addInterface();
}

void Host::handleReceive(Interface* incomingInterface, const std::vector<uint8_t>& rawBytes) {
    EthernetFrame frame;
    try {
        frame.fromBytes(rawBytes);
    } catch (...) {
        return;
    }

    if (!incomingInterface) {
        return;
    }

    const std::string myMac = incomingInterface->getMacAddress();
    if (frame.dstMac != kBroadcastMac && frame.dstMac != myMac) {
        return;
    }

    if (frame.etherType == kEtherTypeArp) {
        ARPMessage arp;
        try {
            arp.fromBytes(frame.payload);
        } catch (...) {
            return;
        }

        // Learn sender mapping on every ARP packet.
        arpCache.table[arp.senderIp] = arp.senderMac;

        if (arp.opcode == 1 && arp.targetIp == ipAddress) {
            ARPMessage reply;
            reply.opcode = 2;
            reply.senderMac = myMac;
            reply.senderIp = ipAddress;
            reply.targetMac = arp.senderMac;
            reply.targetIp = arp.senderIp;

            EthernetFrame out;
            out.dstMac = arp.senderMac;
            out.srcMac = myMac;
            out.etherType = kEtherTypeArp;
            out.vlanId = frame.vlanId;
            out.payload = reply.toBytes();
            incomingInterface->send(out.toBytes());
            return;
        }

        if (arp.opcode == 2) {
            auto queueIt = arpCache.queue.find(arp.senderIp);
            if (queueIt != arpCache.queue.end()) {
                for (const auto& pendingPayload : queueIt->second) {
                    EthernetFrame out;
                    out.dstMac = arp.senderMac;
                    out.srcMac = myMac;
                    out.etherType = kEtherTypeIpv4;
                    out.vlanId = frame.vlanId;
                    out.payload = pendingPayload;
                    incomingInterface->send(out.toBytes());
                }
                arpCache.queue.erase(queueIt);
            }
        }
        return;
    }

    if (frame.etherType == kEtherTypeIpv4) {
        const std::string payload = bytesToString(frame.payload);
        const auto parts = splitBy(payload, '|');
        if (parts.size() != 3) {
            return;
        }

        const std::string& msgType = parts[0];
        const std::string& srcIp = parts[1];
        const std::string& dstIp = parts[2];

        if (dstIp != ipAddress) {
            return;
        }

        if (msgType == "PING") {
            std::cout << "[" << name << "] menerima PING dari " << srcIp << std::endl;
            sendLayer3Packet(srcIp, stringToBytes("PONG|" + ipAddress + "|" + srcIp));
            return;
        }

        if (msgType == "PONG") {
            std::cout << "[" << name << "] menerima PONG dari " << srcIp << std::endl;
        }
    }
}

std::string Host::toJson() const {
    std::stringstream ss;
    ss << "    {\n";
    ss << "      \"name\": \"" << name << "\",\n";
    ss << "      \"ip_address\": \"" << ipAddress << "\",\n";
    ss << "      \"default_gateway\": \"" << defaultGateway << "\"\n";
    ss << "    }";
    return ss.str();
}

void Host::printInfo() const {
    std::cout << "[Host: " << name << "]" << std::endl;
    std::cout << "  IP Address: " << (ipAddress.empty() ? "(not set)" : ipAddress) << std::endl;
    std::cout << "  Default Gateway: " << (defaultGateway.empty() ? "(not set)" : defaultGateway) << std::endl;
    std::cout << "  Interfaces:" << std::endl;
    for (const auto& pair : interfaces) {
        std::cout << "    Port " << pair.first << ": " << pair.second->getMacAddress();
        if (pair.second->isConnected()) {
            std::cout << " (Connected)";
        }
        std::cout << std::endl;
    }
}

void Host::printArpCache() const {
    std::cout << "[ARP Cache: " << name << "]" << std::endl;
    if (arpCache.table.empty()) {
        std::cout << "  (empty)" << std::endl;
        return;
    }

    for (const auto& entry : arpCache.table) {
        std::cout << "  " << entry.first << " -> " << entry.second << std::endl;
    }
}

void Host::sendLayer3Packet(std::string targetIp, std::vector<uint8_t> l3Bytes) {
    auto iface = getInterface(1);
    if (!iface) {
        std::cout << "[" << name << "] Tidak ada interface untuk mengirim paket." << std::endl;
        return;
    }

    const std::string srcMac = iface->getMacAddress();
    auto cacheIt = arpCache.table.find(targetIp);
    if (cacheIt != arpCache.table.end()) {
        EthernetFrame frame;
        frame.dstMac = cacheIt->second;
        frame.srcMac = srcMac;
        frame.etherType = kEtherTypeIpv4;
        frame.vlanId = -1;
        frame.payload = l3Bytes;
        iface->send(frame.toBytes());
        return;
    }

    auto& pending = arpCache.queue[targetIp];
    pending.push_back(l3Bytes);

    // Only send one ARP request while there are pending packets for this IP.
    if (pending.size() > 1) {
        return;
    }

    ARPMessage request;
    request.opcode = 1;
    request.senderMac = srcMac;
    request.senderIp = ipAddress;
    request.targetMac = "00:00:00:00:00:00";
    request.targetIp = targetIp;

    EthernetFrame arpFrame;
    arpFrame.dstMac = kBroadcastMac;
    arpFrame.srcMac = srcMac;
    arpFrame.etherType = kEtherTypeArp;
    arpFrame.vlanId = -1;
    arpFrame.payload = request.toBytes();
    iface->send(arpFrame.toBytes());
}

}