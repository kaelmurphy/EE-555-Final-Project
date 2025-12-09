#include "rans.hpp"

#include <array>
#include <cstdint>
#include <stdexcept>
#include <vector>
#include <numeric>

namespace {
    // rANS parameters
    constexpr uint32_t RANS_L    = 1u << 23;   // renormalization lower bound
    constexpr uint32_t TOTFREQ   = 1u << 12;   // total frequency (4096)
    constexpr int      ALPH_SIZE = 4;

    // Little-endian helpers
    void writeU32LE(std::vector<uint8_t>& out, uint32_t v) {
        out.push_back(static_cast<uint8_t>( v        & 0xFFu));
        out.push_back(static_cast<uint8_t>((v >> 8)  & 0xFFu));
        out.push_back(static_cast<uint8_t>((v >> 16) & 0xFFu));
        out.push_back(static_cast<uint8_t>((v >> 24) & 0xFFu));
    }

    void writeU16LE(std::vector<uint8_t>& out, uint16_t v) {
        out.push_back(static_cast<uint8_t>( v        & 0xFFu));
        out.push_back(static_cast<uint8_t>((v >> 8)  & 0xFFu));
    }

    uint32_t readU32LE(const std::vector<uint8_t>& in, size_t& offset) {
        if (offset + 4 > in.size()) {
            throw std::runtime_error("ransDecode: truncated U32");
        }
        uint32_t v = 0;
        v |= static_cast<uint32_t>(in[offset + 0]) << 0;
        v |= static_cast<uint32_t>(in[offset + 1]) << 8;
        v |= static_cast<uint32_t>(in[offset + 2]) << 16;
        v |= static_cast<uint32_t>(in[offset + 3]) << 24;
        offset += 4;
        return v;
    }

    uint16_t readU16LE(const std::vector<uint8_t>& in, size_t& offset) {
        if (offset + 2 > in.size()) {
            throw std::runtime_error("ransDecode: truncated U16");
        }
        uint16_t v = 0;
        v |= static_cast<uint16_t>(in[offset + 0]) << 0;
        v |= static_cast<uint16_t>(in[offset + 1]) << 8;
        offset += 2;
        return v;
    }
} // namespace

// ==============================
// rANS ENCODER
// ==============================

std::vector<uint8_t> ransEncode(const std::vector<int>& symbols) {
    const uint32_t N = static_cast<uint32_t>(symbols.size());
    if (N == 0) return {};

    // 1) Build histogram
    std::array<uint32_t, ALPH_SIZE> counts{0,0,0,0};
    for (int s : symbols) {
        if (s < 0 || s >= ALPH_SIZE) {
            throw std::runtime_error("ransEncode: symbol out of range (0..3)");
        }
        counts[static_cast<size_t>(s)]++;
    }

    uint32_t sumCounts =
        std::accumulate(counts.begin(), counts.end(), 0u);
    if (sumCounts == 0) {
        throw std::runtime_error("ransEncode: empty histogram");
    }

    // 2) Normalize to TOTFREQ = 4096, ensure each freq >= 1
    std::array<uint32_t, ALPH_SIZE> freqRaw{};
    for (int k = 0; k < ALPH_SIZE; ++k) {
        if (counts[k] == 0) {
            freqRaw[k] = 1;
        } else {
            uint64_t scaled =
                static_cast<uint64_t>(counts[k]) * TOTFREQ / sumCounts;
            if (scaled == 0) scaled = 1;
            freqRaw[k] = static_cast<uint32_t>(scaled);
        }
    }

    uint32_t sumFreq = 0;
    for (int k = 0; k < ALPH_SIZE; ++k) sumFreq += freqRaw[k];

    if (sumFreq < TOTFREQ) {
        freqRaw[0] += (TOTFREQ - sumFreq);
    } else if (sumFreq > TOTFREQ) {
        uint32_t diff = sumFreq - TOTFREQ;
        while (diff > 0) {
            int maxIdx = 0;
            for (int k = 1; k < ALPH_SIZE; ++k) {
                if (freqRaw[k] > freqRaw[maxIdx]) {
                    maxIdx = k;
                }
            }
            if (freqRaw[maxIdx] > 1) {
                freqRaw[maxIdx]--;
                diff--;
            } else {
                break;
            }
        }
    }

    std::array<uint16_t, ALPH_SIZE> freq{};
    for (int k = 0; k < ALPH_SIZE; ++k) {
        if (freqRaw[k] == 0) freqRaw[k] = 1;
        freq[k] = static_cast<uint16_t>(freqRaw[k]);
    }

    // 3) Build cumulative table
    std::array<uint16_t, ALPH_SIZE> cum{};
    cum[0] = 0;
    for (int k = 1; k < ALPH_SIZE; ++k) {
        cum[k] = static_cast<uint16_t>(cum[k-1] + freq[k-1]);
    }
    // optional: assert that cum[3] + freq[3] == TOTFREQ

    // 4) Write header: N + freq[0..3]
    std::vector<uint8_t> out;
    out.reserve(16 + symbols.size());
    writeU32LE(out, N);
    for (int k = 0; k < ALPH_SIZE; ++k) {
        writeU16LE(out, freq[k]);
    }

    // 5) rANS core
    uint32_t x = RANS_L;

    // process symbols in reverse
    for (int i = static_cast<int>(N) - 1; i >= 0; --i) {
        int s = symbols[static_cast<size_t>(i)];
        uint32_t f = freq[s];
        uint32_t c = cum[s];

        // Renormalization
        while (x >= (RANS_L >> 8) * f) {
            out.push_back(static_cast<uint8_t>(x & 0xFFu));
            x >>= 8;
        }

        uint32_t q = x / f;
        uint32_t r = x % f;
        x = q * TOTFREQ + r + c;
    }

    // Flush final state (4 bytes, LSB-first)
    for (int i = 0; i < 4; ++i) {
        out.push_back(static_cast<uint8_t>(x & 0xFFu));
        x >>= 8;
    }

    return out;
}

// ==============================
// rANS DECODER
// ==============================

std::vector<int> ransDecode(const std::vector<uint8_t>& stream) {
    if (stream.size() < 12) {
        throw std::runtime_error("ransDecode: stream too short");
    }

    size_t offset = 0;
    uint32_t N = readU32LE(stream, offset);

    std::array<uint16_t, ALPH_SIZE> freq{};
    for (int k = 0; k < ALPH_SIZE; ++k) {
        freq[k] = readU16LE(stream, offset);
        if (freq[k] == 0) {
            throw std::runtime_error("ransDecode: zero freq in header");
        }
    }

    const size_t dataStart = offset;

    std::array<uint16_t, ALPH_SIZE> cum{};
    cum[0] = 0;
    for (int k = 1; k < ALPH_SIZE; ++k) {
        cum[k] = static_cast<uint16_t>(cum[k-1] + freq[k-1]);
    }
    uint32_t totalFreq = cum[ALPH_SIZE - 1] + freq[ALPH_SIZE - 1];
    if (totalFreq != TOTFREQ) {
        throw std::runtime_error("ransDecode: totalFreq != TOTFREQ");
    }

    // Reconstruct final state x from the last 4 bytes (LSB-first)
    size_t idx = stream.size();
    if (idx < 4) {
        throw std::runtime_error("ransDecode: not enough bytes for final state");
    }

    uint32_t x = 0;
    for (int i = 0; i < 4; ++i) {
        x |= static_cast<uint32_t>(stream[--idx]) << (8 * i);
    }

    std::vector<int> out(N);

    // Decode in reverse (i = N-1..0) to match encoder
    for (int i = static_cast<int>(N) - 1; i >= 0; --i) {
        // Split x into high+low parts w.r.t TOTFREQ
        uint32_t x_mod = x % TOTFREQ;   // == r + c
        uint32_t x_div = x / TOTFREQ;   // == q

        // Find symbol s such that cum[s] <= x_mod < cum[s] + freq[s]
        int s = 0;
        for (int k = 0; k < ALPH_SIZE; ++k) {
            uint32_t c = cum[k];
            uint32_t f = freq[k];
            if (x_mod >= c && x_mod < c + f) {
                s = k;
                break;
            }
        }
        out[static_cast<size_t>(i)] = s;

        uint32_t c = cum[s];
        uint32_t f = freq[s];

        uint32_t x_rem = x_mod - c;   // == r
        x = f * x_div + x_rem;        // recover previous x

        // Renormalization (inverse of encoder)
        while (x < RANS_L && idx > dataStart) {
            x = (x << 8) | stream[--idx];
        }
    }

    return out;
}