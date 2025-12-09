#include "cabac.hpp"
#include "bitstream.hpp"

#include <array>
#include <stdexcept>
#include <vector>

// ============================
// Binarization (Good / Bad)
// ============================
//
// Alphabet {0,1,2,3} with 0 as most probable.
//
// GOOD binarization: truncated-unary style.
//   0 -> 0
//   1 -> 10
//   2 -> 110
//   3 -> 1110
//
// BAD binarization: reversed mapping (bad choice).
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