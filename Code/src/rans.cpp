#include "rans.hpp"

std::vector<uint8_t> ransEncode(const std::vector<int>& symbols) {
    // Placeholder: just copy symbols as bytes.
    std::vector<uint8_t> out;
    out.reserve(symbols.size());
    for (int s : symbols) {
        out.push_back(static_cast<uint8_t>(s));
    }
    return out;
}

std::vector<int> ransDecode(const std::vector<uint8_t>& stream) {
    std::vector<int> syms;
    syms.reserve(stream.size());
    for (auto b : stream) {
        syms.push_back(static_cast<int>(b));
    }
    return syms;
}