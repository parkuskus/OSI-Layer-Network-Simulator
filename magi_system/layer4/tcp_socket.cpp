#include "layer4/tcp_socket.hpp"
#include <ctime>
#include <cstdlib>
#include <iostream>

namespace magi
{

    std::string stateToString(TCPState state)
    {
        switch (state)
        {
        case TCPState::CLOSED:
            return "CLOSED";
        case TCPState::LISTEN:
            return "LISTEN";
        case TCPState::SYN_SENT:
            return "SYN_SENT";
        case TCPState::SYN_RCVD:
            return "SYN_RCVD";
        case TCPState::ESTABLISHED:
            return "ESTABLISHED";
        case TCPState::FIN_WAIT_1:
            return "FIN_WAIT_1";
        case TCPState::FIN_WAIT_2:
            return "FIN_WAIT_2";
        case TCPState::CLOSING:
            return "CLOSING";
        case TCPState::TIME_WAIT:
            return "TIME_WAIT";
        case TCPState::CLOSE_WAIT:
            return "CLOSE_WAIT";
        case TCPState::LAST_ACK:
            return "LAST_ACK";
        default:
            return "UNKNOWN";
        }
    }

    TCPState stringToState(const std::string &stateStr)
    {
        if (stateStr == "CLOSED")
            return TCPState::CLOSED;
        if (stateStr == "LISTEN")
            return TCPState::LISTEN;
        if (stateStr == "SYN_SENT")
            return TCPState::SYN_SENT;
        if (stateStr == "SYN_RCVD")
            return TCPState::SYN_RCVD;
        if (stateStr == "ESTABLISHED")
            return TCPState::ESTABLISHED;
        if (stateStr == "FIN_WAIT_1")
            return TCPState::FIN_WAIT_1;
        if (stateStr == "FIN_WAIT_2")
            return TCPState::FIN_WAIT_2;
        if (stateStr == "CLOSING")
            return TCPState::CLOSING;
        if (stateStr == "TIME_WAIT")
            return TCPState::TIME_WAIT;
        if (stateStr == "CLOSE_WAIT")
            return TCPState::CLOSE_WAIT;
        if (stateStr == "LAST_ACK")
            return TCPState::LAST_ACK;
        return TCPState::CLOSED;
    }

    TCPSocket::TCPSocket(const std::string &localIp, uint16_t localPort,
                         const std::string &remoteIp, uint16_t remotePort)
        : localIP(localIp), localPort(localPort), remoteIP(remoteIp),
          remotePort(remotePort), state(TCPState::CLOSED),
          seqNum(generateInitialSeqNum()), ackNum(0),
          remoteSeqNum(0), remoteAckNum(0),
          nextExpectedSeq(0), windowSize(65535), remoteWindowSize(65535)
    {
    }

    void TCPSocket::setState(TCPState newState)
    {
        TCPState oldState = state;
        state = newState;
        std::cout << "[TCP] State transition: " << stateToString(oldState)
                  << " -> " << stateToString(newState) << std::endl;
    }

    std::string TCPSocket::getStateString() const
    {
        return stateToString(state);
    }

    uint32_t TCPSocket::generateInitialSeqNum() const
    {
        srand(static_cast<unsigned>(time(nullptr)));
        return static_cast<uint32_t>(rand());
    }

    std::shared_ptr<TCPSegment> TCPSocket::initiateConnection()
    {
        if (state != TCPState::CLOSED)
        {
            return nullptr;
        }

        auto segment = createSegment(TCP_FLAG_SYN);
        setState(TCPState::SYN_SENT);

        std::cout << "[TCP] Initiating connection: " << localIP << ":" << localPort
                  << " -> " << remoteIP << ":" << remotePort << std::endl;

        return segment;
    }

    std::shared_ptr<TCPSegment> TCPSocket::respondToSyn(const TCPSegment &incomingSegment)
    {
        if (state != TCPState::LISTEN && state != TCPState::CLOSED)
        {
            return nullptr;
        }

        remoteSeqNum = incomingSegment.seqNum;
        nextExpectedSeq = incomingSegment.seqNum + 1;

        auto segment = createSegment(TCP_FLAG_SYN | TCP_FLAG_ACK);
        segment->ackNum = nextExpectedSeq;
        segment->updateChecksum(localIP, remoteIP);

        setState(TCPState::SYN_RCVD);

        std::cout << "[TCP] Responding with SYN-ACK to incoming connection" << std::endl;

        return segment;
    }

    std::shared_ptr<TCPSegment> TCPSocket::acknowledgeConnection(const TCPSegment &incomingSegment)
    {
        if (state != TCPState::SYN_SENT)
        {
            return nullptr;
        }

        remoteSeqNum = incomingSegment.seqNum;
        remoteAckNum = incomingSegment.ackNum;
        nextExpectedSeq = incomingSegment.seqNum + 1;

        auto segment = createSegment(TCP_FLAG_ACK);
        segment->ackNum = nextExpectedSeq;
        segment->updateChecksum(localIP, remoteIP);

        setState(TCPState::ESTABLISHED);

        std::cout << "[TCP] Connection established! State: " << stateToString(state) << std::endl;

        return segment;
    }

    std::shared_ptr<TCPSegment> TCPSocket::sendData(const std::vector<uint8_t> &data)
    {
        if (!canSendData())
        {
            std::cout << "[TCP] Cannot send data: connection not in ESTABLISHED state" << std::endl;
            return nullptr;
        }

        auto segment = createSegment(TCP_FLAG_PSH | TCP_FLAG_ACK, data);
        sendBuffer.insert(sendBuffer.end(), data.begin(), data.end());

        std::cout << "[TCP] Sending " << data.size() << " bytes of data" << std::endl;

        return segment;
    }

    bool TCPSocket::canSendData() const
    {
        return state == TCPState::ESTABLISHED;
    }

    bool TCPSocket::addToReceiveBuffer(const TCPSegment &segment)
    {
        if (segment.payload.empty())
        {
            return true;
        }

        uint32_t segmentStart = segment.seqNum;
        uint32_t segmentEnd = segment.seqNum + static_cast<uint32_t>(segment.payload.size());

        if (nextExpectedSeq <= segmentStart && segmentStart < nextExpectedSeq + 65535)
        {
            if (segmentStart == nextExpectedSeq)
            {
                receiveBuffer.insert(receiveBuffer.end(), segment.payload.begin(), segment.payload.end());
                nextExpectedSeq = segmentEnd;

                auto it = outOfOrderSegments.begin();
                while (it != outOfOrderSegments.end())
                {
                    if (it->first == nextExpectedSeq)
                    {
                        receiveBuffer.insert(receiveBuffer.end(), it->second.begin(), it->second.end());
                        nextExpectedSeq += static_cast<uint32_t>(it->second.size());
                        it = outOfOrderSegments.erase(it);
                    }
                    else
                    {
                        ++it;
                    }
                }

                std::cout << "[TCP] Added in-order segment to receive buffer (size: "
                          << receiveBuffer.size() << ")" << std::endl;
                return true;
            }
            else if (segmentStart > nextExpectedSeq)
            {
                outOfOrderSegments[segmentStart] = segment.payload;
                std::cout << "[TCP] Added out-of-order segment, buffering for later assembly" << std::endl;
                return true;
            }
        }

        return false;
    }

    std::vector<uint8_t> TCPSocket::getReceivedData()
    {
        std::vector<uint8_t> data = receiveBuffer;
        receiveBuffer.clear();
        return data;
    }

    std::shared_ptr<TCPSegment> TCPSocket::initiateClose()
    {
        if (state == TCPState::ESTABLISHED)
        {
            auto segment = createSegment(TCP_FLAG_FIN | TCP_FLAG_ACK);
            setState(TCPState::FIN_WAIT_1);
            std::cout << "[TCP] Initiating close: sending FIN" << std::endl;
            return segment;
        }
        else if (state == TCPState::CLOSE_WAIT)
        {
            // Application closing on the side that received FIN: send FIN and enter LAST_ACK
            auto segment = createSegment(TCP_FLAG_FIN | TCP_FLAG_ACK);
            setState(TCPState::LAST_ACK);
            std::cout << "[TCP] Initiating close from CLOSE_WAIT: sending FIN (LAST_ACK)" << std::endl;
            return segment;
        }

        return nullptr;
    }

    std::shared_ptr<TCPSegment> TCPSocket::respondToFin(const TCPSegment &incomingSegment)
    {
        if (state == TCPState::ESTABLISHED)
        {
            nextExpectedSeq = incomingSegment.seqNum + 1;
            setState(TCPState::CLOSE_WAIT);

            auto segment = createSegment(TCP_FLAG_ACK);
            segment->ackNum = nextExpectedSeq;
            segment->updateChecksum(localIP, remoteIP);

            std::cout << "[TCP] Received FIN, responding with ACK" << std::endl;

            return segment;
        }
        else if (state == TCPState::FIN_WAIT_1)
        {
            nextExpectedSeq = incomingSegment.seqNum + 1;
            setState(TCPState::CLOSING);

            auto segment = createSegment(TCP_FLAG_ACK);
            segment->ackNum = nextExpectedSeq;

            std::cout << "[TCP] Simultaneous FIN: responding with ACK" << std::endl;

            return segment;
        }
        else if (state == TCPState::FIN_WAIT_2)
        {
            // Peer closed while we're in FIN_WAIT_2: ack the FIN and enter TIME_WAIT
            nextExpectedSeq = incomingSegment.seqNum + 1;
            setState(TCPState::TIME_WAIT);

            auto segment = createSegment(TCP_FLAG_ACK);
            segment->ackNum = nextExpectedSeq;
            segment->updateChecksum(localIP, remoteIP);

            std::cout << "[TCP] Received FIN in FIN_WAIT_2: entering TIME_WAIT and sending ACK" << std::endl;

            return segment;
        }

        return nullptr;
    }

    std::shared_ptr<TCPSegment> TCPSocket::createSegment(uint8_t flags,
                                                         const std::vector<uint8_t> &payload)
    {
        auto segment = std::make_shared<TCPSegment>();
        segment->sourcePort = localPort;
        segment->destinationPort = remotePort;
        segment->seqNum = seqNum;
        segment->ackNum = ackNum;
        segment->flags = flags;
        segment->windowSize = windowSize;
        segment->payload = payload;

        if (flags & TCP_FLAG_SYN)
        {
            seqNum += 1;
        }
        if (flags & TCP_FLAG_FIN)
        {
            seqNum += 1;
        }
        if (!payload.empty())
        {
            seqNum += static_cast<uint32_t>(payload.size());
        }

        segment->updateChecksum(localIP, remoteIP);

        return segment;
    }

    std::shared_ptr<TCPSegment> TCPSocket::handleIncomingSegment(const TCPSegment &segment)
    {
        processSegment(segment);

        std::shared_ptr<TCPSegment> response = nullptr;

        if (segment.hasSYN() && !segment.hasACK())
        {
            handleSYN(segment);
            response = respondToSyn(segment);
        }
        else if (segment.hasSYN() && segment.hasACK())
        {
            handleSYNACK(segment);
            response = acknowledgeConnection(segment);
        }
        else if (segment.hasACK() && !segment.hasSYN() && !segment.hasFIN())
        {
            handleACK(segment);
        }
        else if (segment.hasFIN())
        {
            handleFIN(segment);
            response = respondToFin(segment);
        }
        else if (!segment.payload.empty())
        {
            handleDATA(segment);
        }

        return response;
    }

    void TCPSocket::processSegment(const TCPSegment &segment)
    {
        std::cout << "[TCP] Processing segment: flags=0x" << std::hex
                  << static_cast<int>(segment.flags) << std::dec
                  << " seq=" << segment.seqNum << " ack=" << segment.ackNum << std::endl;
    }

    void TCPSocket::handleSYN(const TCPSegment &)
    {
        std::cout << "[TCP] Received SYN" << std::endl;
    }

    void TCPSocket::handleSYNACK(const TCPSegment &)
    {
        std::cout << "[TCP] Received SYN-ACK" << std::endl;
    }

    void TCPSocket::handleACK(const TCPSegment &)
    {
        if (state == TCPState::SYN_RCVD)
        {
            setState(TCPState::ESTABLISHED);
            std::cout << "[TCP] Received ACK, connection established" << std::endl;
        }
        else if (state == TCPState::FIN_WAIT_1)
        {
            setState(TCPState::FIN_WAIT_2);
            std::cout << "[TCP] Received ACK for FIN, in FIN_WAIT_2" << std::endl;
        }
        else if (state == TCPState::LAST_ACK)
        {
            setState(TCPState::CLOSED);
            std::cout << "[TCP] Received ACK for FIN, connection closed" << std::endl;
        }
        else if (state == TCPState::CLOSING)
        {
            setState(TCPState::TIME_WAIT);
            std::cout << "[TCP] Simultaneous close completed" << std::endl;
        }
    }

    void TCPSocket::handleDATA(const TCPSegment &segment)
    {
        if (state == TCPState::ESTABLISHED ||
            state == TCPState::FIN_WAIT_1 ||
            state == TCPState::FIN_WAIT_2)
        {

            if (addToReceiveBuffer(segment))
            {
                std::cout << "[TCP] Accepted data segment" << std::endl;
            }
        }
        else
        {
            std::cout << "[TCP] Received data in non-data-accepting state: "
                      << stateToString(state) << std::endl;
        }
    }

    void TCPSocket::handleFIN(const TCPSegment &)
    {
        std::cout << "[TCP] Received FIN" << std::endl;
    }

    bool TCPSocket::isValidSeqNum(uint32_t seqNum) const
    {
        return nextExpectedSeq <= seqNum;
    }

    bool TCPSocket::isInWindow(uint32_t seqNum) const
    {
        uint32_t windowEnd = nextExpectedSeq + windowSize;
        return seqNum >= nextExpectedSeq && seqNum < windowEnd;
    }

}
