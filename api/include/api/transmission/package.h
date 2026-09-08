#pragma once

#include "api/common/types.h"

enum class PackageType : Byte
{
    Unknown = 0,
    Voice = 1,
    Alarm = 2,
    Gps = 3,
    Status = 4,
    Ack = 5,
    Control = 6
};

struct PackageHeader
{
    Byte version = 1;
    PackageType type = PackageType::Unknown;
    Byte flags = 0;
    unsigned short sequence = 0;
    unsigned short meta = 0;
    unsigned short payloadLength = 0;
    unsigned short payloadCrc16 = 0;
};

class Package
{
public:
    static constexpr Byte Magic = 0xB3;
    static constexpr unsigned short HeaderSize = 12;

    static unsigned short MaxPayloadLength(unsigned short packetLength);

    static bool Encode(const PackageHeader &header,
                       const Byte *payload,
                       unsigned short payloadLength,
                       Byte *packet,
                       unsigned short packetLength,
                       unsigned short *written);

    static bool Decode(const Byte *packet,
                       unsigned short packetLength,
                       PackageHeader *header,
                       Byte *payload,
                       unsigned short payloadBufferLength,
                       unsigned short *payloadWritten);

private:
    static unsigned short ComputeCrc16(const Byte *data, unsigned short length);
};