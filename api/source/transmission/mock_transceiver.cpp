#include "api/transmission/mock_transceiver.h"

#include "api/common/time.h"

#include <cstdio>
#include <cstring>

MockTransceiver::MockTransceiver(const MockTransceiverConfig &config)
{
    this->config = config;

    if (this->config.maxQueue == 0)
        this->config.maxQueue = 64;
}

MockTransceiver::~MockTransceiver()
{
}

int MockTransceiver::Receive(Byte *buffer, unsigned short length, unsigned int timeout)
{
    (void)timeout;

    if (buffer == nullptr || length == 0)
        return -1;

    if (this->queue.empty())
        return -1;

    unsigned long long nowMs = (unsigned long long)millis();
    Frame &front = this->queue.front();

    if (front.readyAtMs > nowMs)
        return -1;

    if (front.length > length)
        return -1;

    memcpy(buffer, front.data, front.length);
    int received = front.length;
    this->queue.pop_front();

    return received;
}

bool MockTransceiver::Send(Byte *buffer, unsigned short length)
{
    if (buffer == nullptr || length == 0 || length > 240)
        return false;

    this->sentPackets++;
    if (this->config.dropEveryNPacket > 0)
    {
        if ((this->sentPackets % this->config.dropEveryNPacket) == 0)
            return true;
    }

    if (this->queue.size() >= this->config.maxQueue)
        this->queue.pop_front();

    Frame frame;
    frame.length = length;
    frame.readyAtMs = (unsigned long long)millis() + this->config.latencyMs;
    memcpy(frame.data, buffer, length);

    this->queue.push_back(frame);
    return true;
}

void MockTransceiver::Debug()
{
    printf("[MOCK-RADIO] queue=%u sent=%u latencyMs=%u dropEvery=%u\n",
           (unsigned int)this->queue.size(),
           this->sentPackets,
           this->config.latencyMs,
           this->config.dropEveryNPacket);
}
