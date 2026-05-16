#include "layer4/tcp.hpp"
#include <cstring>
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace magi {

std::vector<uint8_t> ipStringToBytes(const std::string& ip) {
    std::vector<uint8_t> bytes;
    std::istringstream ss(ip);
    std::string octet;
    while (std::getline(ss, octet, '.')) {
        bytes.push_back(static_cast<uint8_t>(std::stoi(octet)));
    }
    return bytes;
}

uint16_t calculateChecksumHelper(const std::vector<uint8_t>& data) {
    uint32_t sum = 0;

    for (size_t i = 0; i < data.size(); i += 2) {
        uint16_t word;
        if (i + 1 < data.size()) {
            word = (static_cast<uint16_t>(data[i]) << 8) | data[i + 1];
        } else {
            word = (static_cast<uint16_t>(data[i]) << 8);
        }
        sum += word;
    }
    
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }

    return ~static_cast<uint16_t>(sum);
}

std::vector<uint8_t> TCPPseudoHeader::toBytes() const {
    std::vector<uint8_t> bytes;

    auto srcBytes = ipStringToBytes(srcIp);
    bytes.insert(bytes.end(), srcBytes.begin(), srcBytes.end());

    auto dstBytes = ipStringToBytes(dstIp);
    bytes.insert(bytes.end(), dstBytes.begin(), dstBytes.end());

    bytes.push_back(reserved);

    bytes.push_back(protocol);

    bytes.push_back((tcpLength >> 8) & 0xFF);
    bytes.push_back(tcpLength & 0xFF);

    return bytes;
}

TCPSegment::TCPSegment()
    : sourcePort(0), destinationPort(0), seqNum(0), ackNum(0),
      dataOffset(5), flags(0), windowSize(65535), checksum(0), urgentPointer(0) {
}

std::vector<uint8_t> TCPSegment::toBytes() const {
    std::vector<uint8_t> bytes;

    bytes.push_back((sourcePort >> 8) & 0xFF);
    bytes.push_back(sourcePort & 0xFF);

    bytes.push_back((destinationPort >> 8) & 0xFF);
    bytes.push_back(destinationPort & 0xFF);

    bytes.push_back((seqNum >> 24) & 0xFF);
    bytes.push_back((seqNum >> 16) & 0xFF);
    bytes.push_back((seqNum >> 8) & 0xFF);
    bytes.push_back(seqNum & 0xFF);
    
    bytes.push_back((ackNum >> 24) & 0xFF);
    bytes.push_back((ackNum >> 16) & 0xFF);
    bytes.push_back((ackNum >> 8) & 0xFF);
    bytes.push_back(ackNum & 0xFF);
    
    uint8_t offset_flags = (dataOffset << 4) | ((flags & 0x3F) >> 2);
    uint8_t flags_lower = ((flags & 0x03) << 6);
    bytes.push_back(offset_flags);
    bytes.push_back(flags_lower);
    
    bytes.push_back((windowSize >> 8) & 0xFF);
    bytes.push_back(windowSize & 0xFF);

    bytes.push_back(0x00);
    bytes.push_back(0x00);

    bytes.push_back((urgentPointer >> 8) & 0xFF);
    bytes.push_back(urgentPointer & 0xFF);

    bytes.insert(bytes.end(), payload.begin(), payload.end());

    return bytes;
}

void TCPSegment::fromBytes(const std::vector<uint8_t>& bytes) {
    if (bytes.size() < 20) return;

    sourcePort = (static_cast<uint16_t>(bytes[0]) << 8) | bytes[1];

    destinationPort = (static_cast<uint16_t>(bytes[2]) << 8) | bytes[3];

    seqNum = (static_cast<uint32_t>(bytes[4]) << 24) |
             (static_cast<uint32_t>(bytes[5]) << 16) |
             (static_cast<uint32_t>(bytes[6]) << 8) |
             bytes[7];
    
    ackNum = (static_cast<uint32_t>(bytes[8]) << 24) |
             (static_cast<uint32_t>(bytes[9]) << 16) |
             (static_cast<uint32_t>(bytes[10]) << 8) |
             bytes[11];
    
    dataOffset = (bytes[12] >> 4) & 0x0F;
    uint8_t flags_upper = bytes[12] & 0x0F;
    uint8_t flags_lower = bytes[13] >> 6;
    flags = (flags_upper << 2) | flags_lower;
    
    windowSize = (static_cast<uint16_t>(bytes[14]) << 8) | bytes[15];

    checksum = (static_cast<uint16_t>(bytes[16]) << 8) | bytes[17];

    urgentPointer = (static_cast<uint16_t>(bytes[18]) << 8) | bytes[19];

    size_t headerSize = dataOffset * 4;
    if (bytes.size() > headerSize) {
        payload.insert(payload.end(), bytes.begin() + headerSize, bytes.end());
    }
}

uint16_t TCPSegment::calculateChecksum(const std::string& srcIp, const std::string& dstIp) const {
    TCPPseudoHeader pseudoHeader;
    pseudoHeader.srcIp = srcIp;
    pseudoHeader.dstIp = dstIp;
    pseudoHeader.tcpLength = static_cast<uint16_t>(20 + payload.size());

    auto pseudoHeaderBytes = pseudoHeader.toBytes();
    std::vector<uint8_t> segmentBytes = toBytes();

    segmentBytes[16] = 0x00;
    segmentBytes[17] = 0x00;

    std::vector<uint8_t> combined;
    combined.insert(combined.end(), pseudoHeaderBytes.begin(), pseudoHeaderBytes.end());
    combined.insert(combined.end(), segmentBytes.begin(), segmentBytes.end());
    
    return calculateChecksumHelper(combined);
}

void TCPSegment::updateChecksum(const std::string& srcIp, const std::string& dstIp) {
    checksum = calculateChecksum(srcIp, dstIp);
}

bool TCPSegment::validateChecksum(const std::string& srcIp, const std::string& dstIp) const {
    return checksum == calculateChecksum(srcIp, dstIp);
}

}
