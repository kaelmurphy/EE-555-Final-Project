#pragma once
#include <vector>
#include <cstdint>

enum class BinarizationType {
    Good,
    Bad
};

// Binarize a single symbol (0..3).
std::vector<int> binarizeSymbol(int symbol, BinarizationType type);

// Binarize a whole sequence into one bitstream.
std::vector<int> binarizeSequence(const std::vector<int>& symbols,
                                  BinarizationType type);

// Pack bits (0/1) into bytes.
std::vector<unsigned char> packBitsToBytes(const std::vector<int>& bits);

// Toy binary arithmetic coder on bits.
std::vector<uint8_t> arithEncodeBits(const std::vector<int>& bits);
std::vector<int>     arithDecodeBits(const std::vector<uint8_t>& stream);