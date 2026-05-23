#ifndef MAGI_LAYER4_TCP_SOCKET_HPP
#define MAGI_LAYER4_TCP_SOCKET_HPP

#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <queue>
#include <memory>
#include <functional>
#include "layer4/tcp.hpp"

namespace magi
{

    enum class TCPState
    {
        CLOSED,
        LISTEN,
        SYN_SENT,
        SYN_RCVD,
        ESTABLISHED,
        FIN_WAIT_1,
        FIN_WAIT_2,
        CLOSING,
        TIME_WAIT,
        CLOSE_WAIT,
        LAST_ACK
    };

    std::string stateToString(TCPState state);
    TCPState stringToState(const std::string &stateStr);

    class TCPSocket
    {
    public:
        std::string localIP;
        uint16_t localPort;
        std::string remoteIP;
        uint16_t remotePort;

        TCPState state;

        uint32_t seqNum;
        uint32_t ackNum;
        uint32_t remoteSeqNum;
        uint32_t remoteAckNum;

        std::vector<uint8_t> receiveBuffer;
        std::map<uint32_t, std::vector<uint8_t>> outOfOrderSegments;
        uint32_t nextExpectedSeq;

        std::vector<uint8_t> sendBuffer;

        uint16_t windowSize;
        uint16_t remoteWindowSize;

        TCPSocket(const std::string &localIp, uint16_t localPort,
                  const std::string &remoteIp = "", uint16_t remotePort = 0);

        void setState(TCPState newState);
        std::string getStateString() const;
        void setRemoteEndpoint(const std::string &remoteIp, uint16_t remotePort);

        std::shared_ptr<TCPSegment> handleIncomingSegment(const TCPSegment &segment);
        std::shared_ptr<TCPSegment> createSegment(uint8_t flags,
                                                  const std::vector<uint8_t> &payload = {});

        std::shared_ptr<TCPSegment> initiateConnection();
        std::shared_ptr<TCPSegment> respondToSyn(const TCPSegment &incomingSegment);
        std::shared_ptr<TCPSegment> acknowledgeConnection(const TCPSegment &incomingSegment);

        std::shared_ptr<TCPSegment> sendData(const std::vector<uint8_t> &data);
        bool canSendData() const;
        bool addToReceiveBuffer(const TCPSegment &segment);
        std::vector<uint8_t> getReceivedData();

        std::shared_ptr<TCPSegment> initiateClose();
        std::shared_ptr<TCPSegment> respondToFin(const TCPSegment &incomingSegment);

        bool isValidSeqNum(uint32_t seqNum) const;
        bool isInWindow(uint32_t seqNum) const;

        size_t getReceiveBufferSize() const { return receiveBuffer.size(); }
        size_t getSendBufferSize() const { return sendBuffer.size(); }
        bool isConnected() const { return state == TCPState::ESTABLISHED; }
        bool isClosed() const { return state == TCPState::CLOSED; }

    private:
        void processSegment(const TCPSegment &segment);
        void handleSYN(const TCPSegment &segment);
        void handleSYNACK(const TCPSegment &segment);
        void handleACK(const TCPSegment &segment);
        void handleDATA(const TCPSegment &segment);
        void handleFIN(const TCPSegment &segment);
        uint32_t generateInitialSeqNum() const;
    };

}

#endif // MAGI_LAYER4_TCP_SOCKET_HPP
