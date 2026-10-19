#pragma once
// xoshiro256** — small, fast, statistically strong PRNG (Blackman & Vigna).

#include <cstdint>

namespace holdem {

class Rng {
 public:
  explicit Rng(std::uint64_t seed) {
    for (auto& word : s_) word = splitmix64(seed);
  }

  std::uint64_t next() {
    const std::uint64_t result = rotl(s_[1] * 5, 7) * 9;
    const std::uint64_t t = s_[1] << 17;
    s_[2] ^= s_[0];
    s_[3] ^= s_[1];
    s_[1] ^= s_[2];
    s_[0] ^= s_[3];
    s_[2] ^= t;
    s_[3] = rotl(s_[3], 45);
    return result;
  }

  // Uniform integer in [0, n) via Lemire's multiply-shift (bias < n / 2^32,
  // i.e. below 1e-8 for a 52-card deck — far under Monte Carlo noise).
  std::uint32_t below(std::uint32_t n) {
    const std::uint64_t x = next() >> 32;
    return static_cast<std::uint32_t>((x * n) >> 32);
  }

  static std::uint64_t splitmix64(std::uint64_t& state) {
    std::uint64_t z = (state += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
  }

 private:
  static std::uint64_t rotl(std::uint64_t x, int k) { return (x << k) | (x >> (64 - k)); }
  std::uint64_t s_[4];
};

}  // namespace holdem
