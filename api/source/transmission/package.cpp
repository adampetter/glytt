#include "api/transmission/package.h"

#include <cstring>

unsigned short Package::MaxPayloadLength(unsigned short packetLength)
{
    if (packetLength <= HeaderSize)
        return 0;

    return (unsigned short)(packetLength - HeaderSize);
}

bool Package::Encode(const PackageHeader &header,
                     const Byte *payload,
                     unsigned short payloadLength,
                     Byte *packet,
                     unsigned short packetLength,
                     unsigned short *written)
{
    if (packet == nullptr || written == nullptr)
        return false;

    if (payloadLength > MaxPayloadLength(packetLength))
        return false;

    if (payloadLength > 0 && payload == nullptr)
        return false;

    unsigned short payloadCrc16 = ComputeCrc16(payload, payloadLength);

    packet[0] = Magic;
    packet[1] = header.version;
    packet[2] = (Byte)header.type;
    packet[3] = header.flags;
    packet[4] = (Byte)(header.sequence & 0xFF);
    packet[5] = (Byte)((header.sequence >> 8) & 0xFF);
    packet[6] = (Byte)(header.meta & 0xFF);
    packet[7] = (Byte)((header.meta >> 8) & 0xFF);
    packet[8] = (Byte)(payloadLength & 0xFF);
    packet[9] = (Byte)((payloadLength >> 8) & 0xFF);
    packet[10] = (Byte)(payloadCrc16 & 0xFF);
    packet[11] = (Byte)((payloadCrc16 >> 8) & 0xFF);

    if (payloadLength > 0)
        memcpy(packet + HeaderSize, payload, payloadLength);

    *written = (unsigned short)(HeaderSize + payloadLength);
    return true;
}

bool Package::Decode(const Byte *packet,
                     unsigned short packetLength,
                     PackageHeader *header,
                     Byte *payload,
                     unsigned short payloadBufferLength,
                     unsigned short *payloadWritten)
{
    if (packet == nullptr || header == nullptr || payloadWritten == nullptr)
        return false;

    if (packetLength < HeaderSize)
        return false;

    if (packet[0] != Magic)
        return false;

    header->version = packet[1];
    header->type = (PackageType)packet[2];
    header->flags = packet[3];
    header->sequence = (unsigned short)((unsigned short)packet[4] | ((unsigned short)packet[5] << 8));
    header->meta = (unsigned short)((unsigned short)packet[6] | ((unsigned short)packet[7] << 8));
    header->payloadLength = (unsigned short)((unsigned short)packet[8] | ((unsigned short)packet[9] << 8));
    header->payloadCrc16 = (unsigned short)((unsigned short)packet[10] | ((unsigned short)packet[11] << 8));

    if (header->payloadLength > MaxPayloadLength(packetLength))
        return false;

    if (header->payloadLength > payloadBufferLength)
        return false;

    if (header->payloadLength > 0)
    {
        if (payload == nullptr)
            return false;

        memcpy(payload, packet + HeaderSize, header->payloadLength);
    }

    unsigned short computedCrc16 = ComputeCrc16(payload, header->payloadLength);
    if (computedCrc16 != header->payloadCrc16)
        return false;

    *payloadWritten = header->payloadLength;
    return true;
}

unsigned short Package::ComputeCrc16(const Byte *data, unsigned short length)
{
    if (data == nullptr || length == 0)
        return 0;

    unsigned short crc = 0xFFFF;

    for (unsigned short i = 0; i < length; i++)
    {
        crc ^= (unsigned short)data[i];
        for (Byte bit = 0; bit < 8; bit++)
        {
            bool lsb = (crc & 1) != 0;
            crc >>= 1;
            if (lsb)
                crc ^= 0xA001;
        }
    }

    return crc;
}