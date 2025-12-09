#pragma once
#include <vector>
#include <cstdint>

// rANS encoder/decoder for a 4-symbol alphabet {0,1,2,3}.
// Uses a static model estimated from the symbol histogram.

std::vector<uint8_t> ransEncode(const std::vector<int>& symbols);
std::vector<int>     ransDecode(const std::vector<uint8_t>& stream);