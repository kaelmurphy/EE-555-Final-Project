#pragma once
#include <vector>
#include <cstdint>

class BitWriter {
public:
    void writeBit(bool bit);
    void writeBits(uint32_t value, int nBits);
    std::vector<uint8_t> flush();
private:
    std::vector<uint8_t> buffer_;
    uint8_t currentByte_ = 0;
    int bitPos_ = 0; // 0..7
};

class BitReader {
public:
    explicit BitReader(const std::vector<uint8_t>& data);
    bool readBit();
    uint32_t readBits(int nBits);
private:
    const std::vector<uint8_t>& data_;
    size_t byteIndex_ = 0;
    int bitPos_ = 0;
};