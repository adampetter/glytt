#pragma once

#include "api/transmission/transceiver.h"

#include <deque>

struct MockTransceiverConfig
{
    unsigned int latencyMs = 15;
    unsigned int maxQueue = 64;
    unsigned int dropEveryNPacket = 0;
};

class MockTransceiver : public Transceiver
{
private:
    struct Frame
    {
        Byte data[240] = {0};
        unsigned short length = 0;
        unsigned long long readyAtMs = 0;
    };

    MockTransceiverConfig config;
    std::deque<Frame> queue;
    unsigned int sentPackets = 0;

public:
    MockTransceiver(const MockTransceiverConfig &config);
    ~MockTransceiver();

    int Receive(Byte *buffer, unsigned short length, unsigned int timeout);
    bool Send(Byte *buffer, unsigned short length = 1);
    void Debug();
};
