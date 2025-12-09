#include "cabac.hpp"
#include "bitstream.hpp"

#include <array>
#include <stdexcept>
#include <cstdint>
#include <vector>

// ============================
// Binarization (Good / Bad)
// ============================

// Alphabet {0,1,2,3}.
// Assume 0 is most probable, 3 least probable.

// GOOD binarization: truncated unary – short code for frequent symbols.
//   0 -> 0
//   1 -> 10
//   2 -> 110
//   3 -> 1110
//
// BAD binarization: reversed mapping – long code for frequent symbol 0.
//   0 -> 1110
//   1 -> 110
//   2 -> 10
//   3 -> 0

static const std::array<std::vector<int>, 4> GOOD_TABLE = {
    std::vector<int>{0},              // 0
    std::vector<int>{1, 0},           // 1
    std::vector<int>{1, 1, 0},        // 2
    std::vector<int>{1, 1, 1, 0}      // 3
};

static const std::array<std::vector<int>, 4> BAD_TABLE = {
    std::vector<int>{1, 1, 1, 0},     // 0
    std::vector<int>{1, 1, 0},        // 1
    std::vector<int>{1, 0},           // 2
    std::vector<int>{0}               // 3
};

std::vector<int> binarizeSymbol(int symbol, BinarizationType type) {
    if (symbol < 0 || symbol > 3) {
        throw std::runtime_error("symbol out of range (0..3)");
    }

    if (type == BinarizationType::Good) {
        return GOOD_TABLE[static_cast<size_t>(symbol)];
    } else {
        return BAD_TABLE[static_cast<size_t>(symbol)];
    }
}

std::vector<int> binarizeSequence(const std::vector<int>& symbols,
                                  BinarizationType type)
{
    std::vector<int> bits;
    bits.reserve(symbols.size() * 4); // rough upper bound

    for (int s : symbols) {
        auto b = binarizeSymbol(s, type);
        bits.insert(bits.end(), b.begin(), b.end());
    }
    return bits;
}

std::vector<unsigned char> packBitsToBytes(const std::vector<int>& bits) {
    BitWriter bw;
    for (int b : bits) {
        bw.writeBit(b != 0);
    }
    return bw.flush();
}

// ============================
// Toy binary arithmetic coder
// ============================
//
// Simple binary range coder with a *static* probability model
// estimated from the bit sequence.
//
// Stream layout:
//   [0..3]  : uint32_t numBits
//   [4..7]  : uint32_t count0
//   [8..11] : uint32_t count1
//   [12..]  : range coded bytes
//

static void writeU32LE(std::vector<uint8_t>& out, uint32_t v) {
    out.push_back(static_cast<uint8_t>( v        & 0xFFu));
    out.push_back(static_cast<uint8_t>((v >> 8)  & 0xFFu));
    out.push_back(static_cast<uint8_t>((v >> 16) & 0xFFu));
    out.push_back(static_cast<uint8_t>((v >> 24) & 0xFFu));
}

static uint32_t readU32LE(const std::vector<uint8_t>& in, size_t offset) {
    if (offset + 4 > in.size()) {
        throw std::runtime_error("arithDecodeBits: header truncated");
    }
    uint32_t v = 0;
    v |= static_cast<uint32_t>(in[offset + 0]) << 0;
    v |= static_cast<uint32_t>(in[offset + 1]) << 8;
    v |= static_cast<uint32_t>(in[offset + 2]) << 16;
    v |= static_cast<uint32_t>(in[offset + 3]) << 24;
    return v;
}

std::vector<uint8_t> arithEncodeBits(const std::vector<int>& bits) {
    const uint32_t numBits = static_cast<uint32_t>(bits.size());

    // Global symbol counts for 0 and 1
    uint32_t count0 = 0;
    uint32_t count1 = 0;
    for (int b : bits) {
        if (b == 0) ++count0;
        else        ++count1;
    }

    // Avoid degenerate probs
    if (count0 == 0) ++count0;
    if (count1 == 0) ++count1;
    const uint32_t total = count0 + count1;

    std::vector<uint8_t> out;
    out.reserve(16 + bits.size() / 2);

    // Header
    writeU32LE(out, numBits);
    writeU32LE(out, count0);
    writeU32LE(out, count1);

    // Range coder state
    uint32_t low   = 0;
    uint32_t range = 0xFFFFFFFFu;

    auto renormalize = [&](uint32_t& lowRef, uint32_t& rangeRef) {
        while (rangeRef < (1u << 24)) {
            out.push_back(static_cast<uint8_t>(lowRef >> 24));
            lowRef   <<= 8;
            rangeRef <<= 8;
        }
    };

    // Encode bits
    for (int b : bits) {
        uint32_t r = range / total;
        uint32_t split = r * count0; // interval size for 0

        if (b == 0) {
            // [low, low+split)
            range = split;
        } else {
            // [low+split, low+range)
            low   += split;
            range -= split;
        }

        renormalize(low, range);
    }

    // Flush final bytes of 'low'
    for (int i = 0; i < 4; ++i) {
        out.push_back(static_cast<uint8_t>(low >> 24));
        low <<= 8;
    }

    return out;
}

std::vector<int> arithDecodeBits(const std::vector<uint8_t>& stream) {
    if (stream.size() < 12) {
        throw std::runtime_error("arithDecodeBits: stream too short");
    }

    size_t offset = 0;
    uint32_t numBits = readU32LE(stream, offset); offset += 4;
    uint32_t count0  = readU32LE(stream, offset); offset += 4;
    uint32_t count1  = readU32LE(stream, offset); offset += 4;

    if (count0 == 0 || count1 == 0) {
        throw std::runtime_error("arithDecodeBits: invalid counts");
    }

    const uint32_t total = count0 + count1;

    // Init state
    uint32_t low   = 0;
    uint32_t range = 0xFFFFFFFFu;
    uint32_t code  = 0;

    // Initialize code with first 4 bytes of data
    if (offset + 4 > stream.size()) {
        throw std::runtime_error("arithDecodeBits: no data bytes");
    }
    for (int i = 0; i < 4; ++i) {
        code = (code << 8) | stream[offset++];
    }

    auto renormalize = [&](uint32_t& lowRef, uint32_t& rangeRef, uint32_t& codeRef) {
        while (rangeRef < (1u << 24)) {
            rangeRef <<= 8;
            lowRef   <<= 8;
            uint8_t nextByte = 0;
            if (offset < stream.size()) {
                nextByte = stream[offset++];
            }
            codeRef = (codeRef << 8) | nextByte;
        }
    };

    std::vector<int> bits;
    bits.reserve(numBits);

    for (uint32_t i = 0; i < numBits; ++i) {
        uint32_t r = range / total;
        uint32_t split = r * count0;

        uint32_t rel = code - low;
        int bit;
        if (rel < split) {
            // 0
            bit   = 0;
            range = split;
        } else {
            // 1
            bit   = 1;
            low  += split;
            range -= split;
        }

        bits.push_back(bit);
        renormalize(low, range, code);
    }

    return bits;
}