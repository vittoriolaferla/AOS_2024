#ifndef BYTEREADER_H
#define BYTEREADER_H
#include <iostream>
#include <fstream>
#include <vector>
#include <cstdint> // Include for specific integer types

using byte = std::uint8_t;

// Helper class to read bits from a byte vector
class BitReader
{
private:
    std::uint32_t nextByte = 0;
    std::uint32_t nextBit = 0;
    const std::vector<byte>* data = nullptr;  // Use a pointer to allow for late initialization

public:
    // Default constructor
    BitReader() = default;

    // Allow setting the data vector after construction
    void setData(const std::vector<byte>& d) {
        data = &d;
    }

    // Read one bit (0 or 1) or return -1 if all bits have already been read
    int readBit() {
        if (!data || nextByte >= data->size()) {
            return -1;
        }
        int bit = ((*data)[nextByte] >> (7 - nextBit)) & 1;
        nextBit++;
        if (nextBit == 8) {
            nextBit = 0;
            nextByte++;
        }
        return bit;
    }

    // Read a variable number of bits
    // The first read bit is the most significant bit
    // Return -1 if at any point all bits have already been read
    int readBits(std::uint32_t length) {
        int bits = 0;
        for (std::uint32_t i = 0; i < length; ++i) {
            int bit = readBit();
            if (bit == -1) {
                return -1;
            }
            bits = (bits << 1) | bit;
        }
        return bits;
    }

    // If there are any bits remaining, advance to the 0th bit of the next byte
    void align() {
        if (!data || nextByte >= data->size()) {
            return;
        }
        if (nextBit != 0) {
            nextBit = 0;
            nextByte++;
        }
    }

    // If there are any bits remaining, advance to the 0th bit of the next byte
    void reset() {
        nextByte = 0;
     nextBit = 0;
     data = nullptr; 
    }
};

#endif
