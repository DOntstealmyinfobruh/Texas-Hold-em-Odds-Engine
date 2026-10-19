// Evaluates every one of the 133,784,560 seven-card hands and checks the
// category counts against the known combinatorial totals. This is the
// strongest single check on the evaluator: any wrong branch shows up here.

#include <chrono>
#include <cstdio>

#include "holdem/evaluator.hpp"

using namespace holdem;

int main() {
  int idx[52];
  for (int i = 0, k = 0; i < 64; ++i)
    if ((kFullDeck >> i) & 1) idx[k++] = i;
  Hand bit[52];
  for (int i = 0; i < 52; ++i) bit[i] = Hand{1} << idx[i];

  long long counts[9] = {};
  long long total = 0;
  const auto start = std::chrono::steady_clock::now();

  for (int a = 0; a < 52; ++a) {
    const Hand ha = bit[a];
    for (int b = a + 1; b < 52; ++b) {
      const Hand hb = ha | bit[b];
      for (int c = b + 1; c < 52; ++c) {
        const Hand hc = hb | bit[c];
        for (int d = c + 1; d < 52; ++d) {
          const Hand hd = hc | bit[d];
          for (int e = d + 1; e < 52; ++e) {
            const Hand he = hd | bit[e];
            for (int f = e + 1; f < 52; ++f) {
              const Hand hf = he | bit[f];
              for (int g = f + 1; g < 52; ++g) {
                ++counts[category_of(evaluate(hf | bit[g]))];
                ++total;
              }
            }
          }
        }
      }
    }
  }

  const double secs =
      std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
  const long long expected[9] = {23294460, 58627800, 31433400, 6461620, 6180020,
                                 4047644,  3473184,  224848,   41584};
  int failures = 0;
  std::printf("%-16s %12s %12s\n", "category", "count", "expected");
  for (int i = 8; i >= 0; --i) {
    const bool ok = counts[i] == expected[i];
    failures += !ok;
    std::printf("%-16s %12lld %12lld %s\n", category_name(Category(i)), counts[i], expected[i],
                ok ? "" : "  <-- MISMATCH");
  }
  std::printf("total            %12lld %12lld\n", total, 133784560LL);
  std::printf("%.2fs  (%.1fM evaluations/s, single thread)\n", secs, total / secs / 1e6);
  if (total != 133784560LL) ++failures;
  return failures ? 1 : 0;
}
