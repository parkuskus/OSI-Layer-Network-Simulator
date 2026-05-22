#include "layer4/udp.hpp"
#include <cstring>
#include <sstream>
#include <iomanip>

namespace magi {

std::vector<uint8_t> ipStringToBytes(const std::string& ip);

uint16_t calculateChecksumHelper(const std::vector<uint8_t>& data);

std::vector<uint8_t> UDPPseudoHeader::toBytes() const {
    std::vector<uint8_t> bytes;

    auto srcBytes = ipStringToBytes(srcIp);
    bytes.insert(bytes.end(), srcBytes.begin(), srcBytes.end());

    auto dstBytes = ipStringToBytes(dstIp);
    bytes.insert(bytes.end(), dstBytes.begin(), dstBytes.end());

    bytes.push_back(reserved);

    bytes.push_back(protocol);

    bytes.push_back((udpLength >> 8) & 0xFF);
    bytes.push_back(udpLength & 0xFF);

    return bytes;
}

UDPSegment::UDPSegment()
    : sourcePort(0), destinationPort(0), length(8), checksum(0) {
}

std::vector<uint8_t> UDPSegment::toBytes() const {
    std::vector<uint8_t> bytes;

    bytes.push_back((sourcePort >> 8) & 0xFF);
    bytes.push_back(sourcePort & 0xFF);

    bytes.push_back((destinationPort >> 8) & 0xFF);
    bytes.push_back(destinationPort & 0xFF);

    uint16_t totalLength = 8 + static_cast<uint16_t>(payload.size());
    bytes.push_back((totalLength >> 8) & 0xFF);
    bytes.push_back(totalLength & 0xFF);

    bytes.push_back((checksum >> 8) & 0xFF);
    bytes.push_back(checksum & 0xFF);

    bytes.insert(bytes.end(), payload.begin(), payload.end());

    return bytes;
}

void UDPSegment::fromBytes(const std::vector<uint8_t>& bytes) {
    if (bytes.size() < 8) return;

    sourcePort = (static_cast<uint16_t>(bytes[0]) << 8) | bytes[1];

    destinationPort = (static_cast<uint16_t>(bytes[2]) << 8) | bytes[3];

    length = (static_cast<uint16_t>(bytes[4]) << 8) | bytes[5];

    checksum = (static_cast<uint16_t>(bytes[6]) << 8) | bytes[7];

    if (bytes.size() > 8) {
        payload.insert(payload.end(), bytes.begin() + 8, bytes.end());
    }
}

uint16_t UDPSegment::calculateChecksum(const std::string& srcIp, const std::string& dstIp) const {
    UDPPseudoHeader pseudoHeader;
    pseudoHeader.srcIp = srcIp;
    pseudoHeader.dstIp = dstIp;
    pseudoHeader.udpLength = static_cast<uint16_t>(8 + payload.size());

    auto pseudoHeaderBytes = pseudoHeader.toBytes();
    std::vector<uint8_t> segmentBytes = toBytes();

    segmentBytes[6] = 0x00;
    segmentBytes[7] = 0x00;

    std::vector<uint8_t> combined;
    combined.insert(combined.end(), pseudoHeaderBytes.begin(), pseudoHeaderBytes.end());
    combined.insert(combined.end(), segmentBytes.begin(), segmentBytes.end());
    
    return calculateChecksumHelper(combined);
}

void UDPSegment::updateChecksum(const std::string& srcIp, const std::string& dstIp) {
    checksum = calculateChecksum(srcIp, dstIp);
}

bool UDPSegment::validateChecksum(const std::string& srcIp, const std::string& dstIp) const {
    return checksum == calculateChecksum(srcIp, dstIp);
}

}
