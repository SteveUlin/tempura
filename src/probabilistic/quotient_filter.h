#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace tempura {

// ============================================================================
// Quotient Filter
// ============================================================================
//
// A quotient filter is a compact, probabilistic set membership structure that
// supports insertion, lookup, and deletion. Unlike Bloom filters, quotient
// filters support deletion without additional counting overhead.
//
// STRUCTURE
// ---------
// The filter hashes each item to f = q + r bits. The hash is split into:
//   - q-bit quotient: used as the bucket index (canonical slot)
//   - r-bit remainder: stored in the table
//
// COLLISION HANDLING
// ------------------
// Multiple items can hash to the same quotient. The quotient filter uses
// linear probing with three metadata bits per slot:
//
//   is_occupied    - Set if SOME item's canonical slot is this bucket
//   is_continuation - Set if this slot continues a run (not first in run)
//   is_shifted     - Set if this slot's element is displaced from canonical
//
// FALSE POSITIVE RATE: With r remainder bits: FPR approx 2^(-r)
//
// TEMPLATE PARAMETERS
// -------------------
//   QuotientBits  - Number of bits for quotient (default 16 = 64K slots)
//   RemainderBits - Number of bits for remainder (default 8 = 1/256 FPR)
//
template <std::size_t QuotientBits = 16, std::size_t RemainderBits = 8>
class QuotientFilter {
  static_assert(QuotientBits > 0 && QuotientBits <= 32,
                "QuotientBits must be in [1, 32]");
  static_assert(RemainderBits > 0 && RemainderBits <= 32,
                "RemainderBits must be in [1, 32]");
  static_assert(QuotientBits + RemainderBits <= 64,
                "Total fingerprint bits must fit in 64 bits");

public:
  static constexpr std::size_t kNumSlots = 1ULL << QuotientBits;
  static constexpr std::uint64_t kRemainderMask = (1ULL << RemainderBits) - 1;

  using Remainder = std::conditional_t<
      RemainderBits <= 8, std::uint8_t,
      std::conditional_t<RemainderBits <= 16, std::uint16_t, std::uint32_t>>;

private:
  struct Slot {
    Remainder remainder{0};
    bool is_occupied{false};     // Some element hashes to this canonical slot
    bool is_continuation{false}; // This slot continues a run
    bool is_shifted{false};      // This slot's element is displaced from canonical
    bool has_element{false};     // This slot contains an element
  };

  std::vector<Slot> slots_;
  std::size_t size_{0};

  auto incr(std::size_t i) const -> std::size_t {
    return (i + 1) & (kNumSlots - 1);
  }

  auto decr(std::size_t i) const -> std::size_t {
    return (i - 1 + kNumSlots) & (kNumSlots - 1);
  }

public:
  QuotientFilter() : slots_(kNumSlots), size_{0} {}

  auto insert(std::uint64_t item) -> bool {
    if (size_ >= kNumSlots - 1) {
      return false;
    }

    auto [fq, fr] = fingerprint(item);

    bool was_occupied = slots_[fq].is_occupied;
    slots_[fq].is_occupied = true;

    if (!was_occupied && !slots_[fq].has_element) {
      // Slot is empty and nothing else hashes here
      slots_[fq].remainder = fr;
      slots_[fq].has_element = true;
      slots_[fq].is_shifted = false;
      slots_[fq].is_continuation = false;
      ++size_;
      return true;
    }

    // Find the run for this quotient
    std::size_t run_start = findRunStart(fq);

    std::size_t insert_pos;
    bool is_continuation;

    if (!was_occupied) {
      // New run starting at run_start
      insert_pos = run_start;
      is_continuation = false;
    } else {
      // Add to end of existing run
      std::size_t s = run_start;
      while (slots_[incr(s)].has_element && slots_[incr(s)].is_continuation) {
        s = incr(s);
      }
      insert_pos = incr(s);
      is_continuation = true;
    }

    // Shift and insert
    insertAt(insert_pos, fr, is_continuation);
    ++size_;
    return true;
  }

  auto contains(std::uint64_t item) const -> bool {
    auto [fq, fr] = fingerprint(item);

    if (!slots_[fq].is_occupied) {
      return false;
    }

    std::size_t s = findRunStart(fq);

    // Check first element
    if (slots_[s].remainder == fr) {
      return true;
    }

    // Check continuations
    while (slots_[incr(s)].has_element && slots_[incr(s)].is_continuation) {
      s = incr(s);
      if (slots_[s].remainder == fr) {
        return true;
      }
    }

    return false;
  }

  auto remove(std::uint64_t item) -> bool {
    auto [fq, fr] = fingerprint(item);

    if (!slots_[fq].is_occupied) {
      return false;
    }

    std::size_t run_start = findRunStart(fq);
    std::size_t s = run_start;

    // Find element and count run length
    std::size_t run_len = 1;
    std::optional<std::size_t> found_pos;

    if (slots_[s].remainder == fr) {
      found_pos = s;
    }

    while (slots_[incr(s)].has_element && slots_[incr(s)].is_continuation) {
      s = incr(s);
      ++run_len;
      if (!found_pos && slots_[s].remainder == fr) {
        found_pos = s;
      }
    }

    if (!found_pos) {
      return false;
    }

    if (run_len == 1) {
      slots_[fq].is_occupied = false;
    }

    // Delete and compact
    deleteAndCompact(*found_pos);
    --size_;
    return true;
  }

  auto size() const -> std::size_t { return size_; }
  static constexpr auto capacity() -> std::size_t { return kNumSlots; }
  auto loadFactor() const -> double {
    return static_cast<double>(size_) / static_cast<double>(kNumSlots);
  }
  auto empty() const -> bool { return size_ == 0; }

  void clear() {
    for (auto& slot : slots_) {
      slot = Slot{};
    }
    size_ = 0;
  }

private:
  static auto hash(std::uint64_t x) -> std::uint64_t {
    x ^= x >> 33;
    x *= 0xff51afd7ed558ccdULL;
    x ^= x >> 33;
    x *= 0xc4ceb9fe1a85ec53ULL;
    x ^= x >> 33;
    return x;
  }

  auto fingerprint(std::uint64_t item) const -> std::pair<std::size_t, Remainder> {
    auto h = hash(item);
    auto fq = static_cast<std::size_t>(h >> RemainderBits) & (kNumSlots - 1);
    auto fr = static_cast<Remainder>(h & kRemainderMask);
    return {fq, fr};
  }

  // Find start of run for quotient fq
  auto findRunStart(std::size_t fq) const -> std::size_t {
    // Walk backward to find cluster start
    std::size_t b = fq;
    while (slots_[b].is_shifted) {
      b = decr(b);
    }

    // b is at cluster start, walk forward to find fq's run
    std::size_t s = b;
    while (b != fq) {
      // Skip current run
      while (slots_[incr(s)].has_element && slots_[incr(s)].is_continuation) {
        s = incr(s);
      }
      s = incr(s);

      // Find next occupied canonical slot
      do {
        b = incr(b);
      } while (!slots_[b].is_occupied);
    }

    return s;
  }

  // Insert at position, shifting elements right
  void insertAt(std::size_t pos, Remainder fr, bool is_continuation) {
    if (!slots_[pos].has_element) {
      slots_[pos].remainder = fr;
      slots_[pos].is_continuation = is_continuation;
      slots_[pos].is_shifted = is_continuation;
      slots_[pos].has_element = true;
      return;
    }

    Remainder carry_rem = fr;
    bool carry_cont = is_continuation;
    bool carry_shift = true;

    while (slots_[pos].has_element) {
      Remainder tmp_rem = slots_[pos].remainder;
      bool tmp_cont = slots_[pos].is_continuation;

      slots_[pos].remainder = carry_rem;
      slots_[pos].is_continuation = carry_cont;
      slots_[pos].is_shifted = carry_shift;

      carry_rem = tmp_rem;
      carry_cont = tmp_cont;
      carry_shift = true;

      pos = incr(pos);
    }

    slots_[pos].remainder = carry_rem;
    slots_[pos].is_continuation = carry_cont;
    slots_[pos].is_shifted = carry_shift;
    slots_[pos].has_element = true;
  }

  // Delete element at pos and compact
  void deleteAndCompact(std::size_t pos) {
    bool was_run_start = !slots_[pos].is_continuation;

    std::size_t curr = pos;
    std::size_t next = incr(pos);

    // Special handling: if we're deleting a run start and next is continuation,
    // next becomes the new run start
    bool promoted_next = false;
    if (was_run_start && slots_[next].has_element && slots_[next].is_continuation) {
      promoted_next = true;
    }

    // Shift elements left while they're part of the same cluster
    while (slots_[next].has_element && slots_[next].is_shifted) {
      slots_[curr].remainder = slots_[next].remainder;
      slots_[curr].has_element = true;

      // Handle continuation flag
      if (curr == pos && promoted_next) {
        slots_[curr].is_continuation = false;
      } else {
        slots_[curr].is_continuation = slots_[next].is_continuation;
      }

      // Handle shifted flag - element becomes unshifted if it's a run start
      // returning to its canonical position
      if (!slots_[curr].is_continuation && slots_[curr].is_occupied) {
        slots_[curr].is_shifted = false;
      } else {
        slots_[curr].is_shifted = true;
      }

      curr = next;
      next = incr(next);
    }

    // Clear the last slot
    slots_[curr].remainder = 0;
    slots_[curr].is_continuation = false;
    slots_[curr].is_shifted = false;
    slots_[curr].has_element = false;
  }
};

}  // namespace tempura
