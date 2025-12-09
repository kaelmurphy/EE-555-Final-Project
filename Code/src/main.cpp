#include <iostream>
#include <vector>
#include <array>
#include <cmath>
#include <random>
#include <algorithm>
#include <iomanip>

#include "cabac.hpp"
#include "cabac_tables.hpp"
#include "rans.hpp"

// Generate N symbols in {0,1,2,3} with probabilities 0.7,0.1,0.1,0.1
std::vector<int> generateSource(int N) {
    std::mt19937 rng(12345);
    std::discrete_distribution<int> dist({70, 10, 10, 10});
    std::vector<int> symbols;
    symbols.reserve(N);
    for (int i = 0; i < N; ++i) symbols.push_back(dist(rng));
    return symbols;
}

double computeSymbolEntropy(const std::array<int,4>& counts, int N) {
    double H = 0.0;
    for (int k = 0; k < 4; ++k) {
        if (counts[k] == 0) continue;
        double p = double(counts[k]) / N;
        H += -p * std::log2(p);
    }
    return H;
}

double computeBinEntropy(const std::vector<int>& bits) {
    if (bits.empty()) return 0.0;
    int c0 = 0, c1 = 0;
    for (int b : bits) (b ? c1 : c0)++;
    double N = c0 + c1;
    double H = 0.0;
    if (c0 > 0) H += -(c0/N) * std::log2(c0/N);
    if (c1 > 0) H += -(c1/N) * std::log2(c1/N);
    return H;
}

int cabacFindState(double pLPS) {
    double best = 1e9;
    int bestState = 0;
    for (int s = 0; s < 64; ++s) {
        double p = cabacRangeTabLPS[s][0] / 256.0;
        double d = std::abs(pLPS - p);
        if (d < best) { best = d; bestState = s; }
    }
    return bestState;
}

int main() {
    const int N = 1000;
    auto symbols = generateSource(N);

    std::array<int,4> counts{0,0,0,0};
    for (int s : symbols) counts[s]++;

    std::cout << "Number of source symbols: " << N << "\n";
    std::cout << "Frequencies:\n";
    for (int k = 0; k < 4; ++k)
        std::cout << "  symbol " << k << ": " << counts[k] << "\n";

    double Hsym = computeSymbolEntropy(counts, N);
    std::cout << std::fixed << std::setprecision(6);
    std::cout << "Theoretical symbol entropy H_sym = "
              << Hsym << " bits/symbol\n\n";

    // CABAC good
    auto bitsGood = binarizeSequence(symbols, BinarizationType::Good);
    auto bytesGood = packBitsToBytes(bitsGood);
    double binsPerGood = double(bitsGood.size()) / N;
    double HbinGood = computeBinEntropy(bitsGood);
    double idealCABACgood = HbinGood * binsPerGood;

    double p1 = std::count(bitsGood.begin(), bitsGood.end(), 1)
                / double(bitsGood.size());
    double p0 = 1.0 - p1;
    double pLPS = std::min(p0, p1);

    int cabacState = cabacFindState(pLPS);
    double cabacModelP = cabacRangeTabLPS[cabacState][0] / 256.0;

    // CABAC bad
    auto bitsBad = binarizeSequence(symbols, BinarizationType::Bad);
    double binsPerBad = double(bitsBad.size()) / N;
    double HbinBad = computeBinEntropy(bitsBad);
    double idealCABACbad = HbinBad * binsPerBad;

    // rANS
    auto ransStream  = ransEncode(symbols);
    auto ransDecoded = ransDecode(ransStream);
    bool okRans      = (ransDecoded == symbols);

    int ransBytes = int(ransStream.size());
    double ransRate = 8.0 * ransBytes / N;

    // Pretty summary
    std::cout << "\n===================================================\n";
    std::cout << "                 ENTROPY SUMMARY\n";
    std::cout << "===================================================\n";

    std::cout << "True source entropy (4-symbol):      "
              << Hsym << " bits/symbol\n\n";

    std::cout << "---------------- CABAC (Good) ---------------------\n";
    std::cout << "bins/symbol:                         " << binsPerGood << "\n";
    std::cout << "bin entropy:                         " << HbinGood << " bits/bin\n";
    std::cout << "ideal CABAC rate:                    " << idealCABACgood << " bits/symbol\n";
    std::cout << "Observed LPS probability:            " << pLPS << "\n";
    std::cout << "CABAC-model LPS probability:         "
              << cabacModelP << " (state " << cabacState << ")\n";
    std::cout << "Difference:                          "
              << std::abs(pLPS - cabacModelP) << "\n\n";

    std::cout << "---------------- CABAC (Bad) ----------------------\n";
    std::cout << "bins/symbol:                         " << binsPerBad << "\n";
    std::cout << "bin entropy:                         " << HbinBad << " bits/bin\n";
    std::cout << "ideal CABAC rate:                    " << idealCABACbad << " bits/symbol\n\n";

    std::cout << "---------------- rANS -----------------------------\n";
    std::cout << "rANS stream size:                    " << ransBytes << " bytes\n";
    std::cout << "rANS rate:                           " << ransRate << " bits/symbol\n";
    std::cout << "roundtrip OK:                        " << std::boolalpha << okRans << "\n\n";

    double diffRans = std::abs(ransRate - Hsym);
    double diffCab  = std::abs(idealCABACgood - Hsym);
    std::string winner = (diffRans < diffCab ? "rANS" : "CABAC (good)");

    std::cout << "===================================================\n";
    std::cout << "Winner (closest to entropy):          "
              << winner << "\n";
    std::cout << "===================================================\n";

    return 0;
}