#include <iostream>
#include <vector>
#include <array>
#include <cmath>
#include <random>
#include <algorithm>
#include <iomanip>

#include "cabac.hpp"
#include "rans.hpp"

// Generate N symbols in {0,1,2,3} with probabilities:
// P(0) = 0.7, P(1) = P(2) = P(3) = 0.1
std::vector<int> generateSource(int N) {
    std::mt19937 rng(12345); // fixed seed for reproducibility
    std::discrete_distribution<int> dist({70, 10, 10, 10});

    std::vector<int> symbols;
    symbols.reserve(N);
    for (int i = 0; i < N; ++i) {
        symbols.push_back(dist(rng));
    }
    return symbols;
}

// Compute entropy of symbols {0..K-1} given counts
double computeSymbolEntropy(const std::array<int,4>& counts, int N) {
    double H = 0.0;
    for (int k = 0; k < 4; ++k) {
        if (counts[k] == 0) continue;
        double p = static_cast<double>(counts[k]) / N;
        H += -p * std::log2(p);
    }
    return H;
}

// Compute entropy of a binary sequence of bits (0/1)
double computeBinEntropy(const std::vector<int>& bits) {
    if (bits.empty()) return 0.0;
    int c0 = 0, c1 = 0;
    for (int b : bits) {
        if (b == 0) ++c0;
        else        ++c1;
    }
    double N = static_cast<double>(c0 + c1);
    double H = 0.0;
    if (c0 > 0) {
        double p0 = c0 / N;
        H += -p0 * std::log2(p0);
    }
    if (c1 > 0) {
        double p1 = c1 / N;
        H += -p1 * std::log2(p1);
    }
    return H;
}

// Simple test helper to print if roundtrip passes
void testArithmeticCoder() {
    std::cout << "=== Arithmetic coder self-test ===\n";

    {
        std::vector<int> bits = {0,1,0,1,1,0,0,0,1,1,1,0};
        auto coded   = arithEncodeBits(bits);
        auto decoded = arithDecodeBits(coded);
        bool ok = (decoded == bits);
        std::cout << "  Small fixed pattern roundtrip: " << std::boolalpha << ok << "\n";
    }

    {
        // random bits
        std::mt19937 rng(999);
        std::uniform_int_distribution<int> bitDist(0,1);
        std::vector<int> bits;
        bits.reserve(1000);
        for (int i = 0; i < 1000; ++i) {
            bits.push_back(bitDist(rng));
        }

        auto coded   = arithEncodeBits(bits);
        auto decoded = arithDecodeBits(coded);
        bool ok = (decoded == bits);
        std::cout << "  Random 1000-bit roundtrip:     " << std::boolalpha << ok << "\n";
        std::cout << "  Encoded size: " << coded.size() << " bytes\n";
    }

    std::cout << "==================================\n\n";
}

int main() {
    // 0) Self-test the arithmetic coder once
    testArithmeticCoder();

    // 1) Generate source: symbols in {0,1,2,3}
    const int N = 1000;
    std::vector<int> symbols = generateSource(N);

    const int alphabetSize = 4;
    std::array<int, alphabetSize> counts{0,0,0,0};

    for (int s : symbols) {
        if (s < 0 || s >= alphabetSize) {
            std::cerr << "Symbol out of range in source!\n";
            return 1;
        }
        counts[static_cast<size_t>(s)]++;
    }

    std::cout << "Number of source symbols: " << N << "\n";

    // 2) Histogram of symbols
    std::cout << "Frequencies:\n";
    for (int k = 0; k < alphabetSize; ++k) {
        std::cout << "  symbol " << k << ": " << counts[k] << "\n";
    }

    // 3) Probabilities + entropy of source symbols
    double Hsym = computeSymbolEntropy(counts, N);

    std::cout << std::fixed << std::setprecision(6);
    std::cout << "Probabilities:\n";
    for (int k = 0; k < alphabetSize; ++k) {
        double p = static_cast<double>(counts[k]) / N;
        std::cout << "  P(" << k << ") = " << p << "\n";
    }
    std::cout << "Theoretical symbol entropy H_sym = "
              << Hsym << " bits/symbol\n\n";

    // ==========================
    // GOOD binarization
    // ==========================
    auto bitsGood  = binarizeSequence(symbols, BinarizationType::Good);
    auto bytesGood = packBitsToBytes(bitsGood);
    double Rgood   = static_cast<double>(bitsGood.size()) / N;

    double HbinGood = computeBinEntropy(bitsGood);
    double idealRateGood = HbinGood * Rgood;  // ideal CABAC rate for that binarization

    std::cout << "Good binarization:\n";
    std::cout << "  total bins            = " << bitsGood.size() << "\n";
    std::cout << "  total bytes (raw bins)= " << bytesGood.size() << "\n";
    std::cout << "  bins/symbol           = " << Rgood << "\n";
    std::cout << "  bin entropy H_bins    = " << HbinGood << " bits/bin\n";
    std::cout << "  ideal CABAC rate      = " << idealRateGood << " bits/symbol\n";

    auto arithGood = arithEncodeBits(bitsGood);
    auto bitsGoodDecoded = arithDecodeBits(arithGood);
    bool okGood = (bitsGoodDecoded == bitsGood);

    const int headerBytes = 12;
    int payloadGoodBytes = std::max(0, static_cast<int>(arithGood.size()) - headerBytes);
    double Rgood_pure = 8.0 * payloadGoodBytes / N;

    std::cout << "  arithmetic-coded size (with header) = "
              << arithGood.size() << " bytes\n";
    std::cout << "  pure coder rate (good, no header)   = "
              << Rgood_pure << " bits/symbol\n";
    std::cout << "  roundtrip OK (good)                 = "
              << std::boolalpha << okGood << "\n\n";

    // ==========================
    // BAD binarization
    // ==========================
    auto bitsBad  = binarizeSequence(symbols, BinarizationType::Bad);
    auto bytesBad = packBitsToBytes(bitsBad);
    double Rbad   = static_cast<double>(bitsBad.size()) / N;

    double HbinBad = computeBinEntropy(bitsBad);
    double idealRateBad = HbinBad * Rbad;

    std::cout << "Bad binarization:\n";
    std::cout << "  total bins            = " << bitsBad.size() << "\n";
    std::cout << "  total bytes (raw bins)= " << bytesBad.size() << "\n";
    std::cout << "  bins/symbol           = " << Rbad << "\n";
    std::cout << "  bin entropy H_bins    = " << HbinBad << " bits/bin\n";
    std::cout << "  ideal CABAC rate      = " << idealRateBad << " bits/symbol\n";

    auto arithBad = arithEncodeBits(bitsBad);
    auto bitsBadDecoded = arithDecodeBits(arithBad);
    bool okBad = (bitsBadDecoded == bitsBad);

    int payloadBadBytes = std::max(0, static_cast<int>(arithBad.size()) - headerBytes);
    double Rbad_pure = 8.0 * payloadBadBytes / N;

    std::cout << "  arithmetic-coded size (with header) = "
              << arithBad.size() << " bytes\n";
    std::cout << "  pure coder rate (bad, no header)    = "
              << Rbad_pure << " bits/symbol\n";
    std::cout << "  roundtrip OK (bad)                  = "
              << std::boolalpha << okBad << "\n";

    return 0;
}