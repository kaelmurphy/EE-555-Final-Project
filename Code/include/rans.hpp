#pragma once
#include <vector>
#include <cstdint>

std::vector<uint8_t> ransEncode(const std::vector<int>& symbols);
std::vector<int>     ransDecode(const std::vector<uint8_t>& stream);