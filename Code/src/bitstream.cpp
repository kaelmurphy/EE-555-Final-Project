#include "bitstream.hpp"
#include <stdexcept>

void BitWriter::writeBit(bool bit) {
    currentByte_ |= (bit ? 1u : 0u) << bitPos_;
    ++bitPos_;
    if (bitPos_ == 8) {
        buffer_.push_back(currentByte_);
        currentByte_ = 0;
        bitPos_ = 0;
    }
}

void BitWriter::writeBits(uint32_t value, int nBits) {
    for (int i = 0; i < nBits; ++i) {
        writeBit((value >> i) & 1u);
    }
}

std::vector<uint8_t> BitWriter::flush() {
    if (bitPos_ != 0) {
        buffer_.push_back(currentByte_);
        currentByte_ = 0;
        bitPos_ = 0;
    }
    return buffer_;
}

BitReader::BitReader(const std::vector<uint8_t>& data)
    : data_(data) {}

bool BitReader::readBit() {
    if (byteIndex_ >= data_.size()) {
        throw std::runtime_error("BitReader: out of data");
    }
    bool bit = (data_[byteIndex_] >> bitPos_) & 1u;
    ++bitPos_;
    if (bitPos_ == 8) {
        bitPos_ = 0;
        ++byteIndex_;
    }
    return bit;
}

uint32_t BitReader::readBits(int nBits) {
    uint32_t v = 0;
    for (int i = 0; i < nBits; ++i) {
        if (readBit()) v |= (1u << i);
    }
    return v;
}