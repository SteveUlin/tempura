# Hash Functions Library

A comprehensive collection of hash function implementations with educational commentary. The code comments are written to teach hash function theory, not just document the implementation.

## Reading Order

Start here and work through in order. Each file builds on concepts from the previous ones.

### 1. `hash.h` — Foundations
**Read this first.** Introduces the core concepts that all hash functions share:
- What makes a hash function "good" (uniformity, avalanche, speed)
- Cryptographic vs non-cryptographic hashes
- Key mixing primitives: XOR, rotation, multiplication
- The finalization step and why it matters
- Hash combining (the boost pattern)

### 2. `fnv.h` — The Simplest Practical Hash
FNV-1a is the "hello world" of hash functions:
- Just two operations per byte: XOR then multiply
- Explains why specific magic constants were chosen
- Shows how multiplication spreads bit influence
- Good baseline for understanding what more complex hashes improve upon

### 3. `murmur3.h` — The Classic Workhorse
MurmurHash3 dominated for a decade. Learn:
- The multiply-rotate "murmur" mixing pattern
- Block-based processing (4 or 8 bytes at a time)
- Tail handling for inputs not aligned to block size
- The fmix32/fmix64 finalizers and how they were discovered via simulated annealing

### 4. `wyhash.h` — Modern Speed King
The fastest portable hash function. Introduces:
- The "mum" primitive: 64×64→128 multiply, then XOR-fold
- Why 128-bit multiplication provides excellent avalanche in one operation
- Parallel accumulators for instruction-level parallelism
- How wyhash became the default in Go, Zig, Nim, and V

### 5. `fibonacci.h` — The Right Way to Use Hashes
Not a hash function, but essential knowledge:
- Why `hash % table_size` wastes high bits
- Fibonacci hashing: multiply by 2^64/φ, use high bits
- The mathematical magic of the golden ratio (Knuth's recommendation)
- Fast range reduction for non-power-of-2 sizes

### 6. `crc32c.h` — Hardware Acceleration
Your first hardware-accelerated hash:
- CRC mathematics: polynomial division over GF(2)
- The SSE4.2 `_mm_crc32_u64` instruction
- Processing 8 bytes per cycle (~30 GB/s)
- Why CRC makes a decent hash but has limitations

### 7. `aes_hash.h` — Peak Performance
Using encryption instructions for hashing:
- AES round operations: SubBytes, ShiftRows, MixColumns
- Why a single AESENC instruction provides near-perfect avalanche
- Processing 16 bytes at ~50 GB/s
- The tradeoff: requires AES-NI hardware

### 8. `rolling_hash.h` — A Different Paradigm
Fundamentally different from all the above:
- O(1) window updates vs O(n) rehashing
- Polynomial hashing: treating data as polynomial coefficients
- The Rabin-Karp string search algorithm
- Content-defined chunking (how rsync finds changed blocks)

### 9. `perfect_hash.h` — Zero Collisions
For when you know all keys at compile time:
- Perfect hashing: every key maps to a unique slot
- The two-level hashing trick (CHD algorithm)
- Why process largest buckets first
- Constexpr construction: zero runtime cost

## Quick Reference

| File | Type | Speed | Best For |
|------|------|-------|----------|
| `fnv.h` | Software | ~1 GB/s | Simplicity, constexpr, learning |
| `murmur3.h` | Software | ~5 GB/s | Compatibility, well-understood |
| `wyhash.h` | Software | ~20 GB/s | General purpose, best portable |
| `crc32c.h` | Hardware | ~30 GB/s | Checksums, x86 with SSE4.2 |
| `aes_hash.h` | Hardware | ~50 GB/s | Maximum speed, x86 with AES-NI |
| `rolling_hash.h` | Software | O(1) update | Substring search, diff, chunking |
| `perfect_hash.h` | Compile-time | O(1) lookup | Static key sets, keywords |
| `fibonacci.h` | Utility | — | Mapping hash → table index |

## Key Concepts by File

```
hash.h           → avalanche, uniformity, mixing primitives
fnv.h            → simplicity, byte-at-a-time processing
murmur3.h        → block processing, finalization
wyhash.h         → wide multiplication, parallel accumulators
fibonacci.h      → golden ratio, range reduction
crc32c.h         → hardware instructions, polynomial math
aes_hash.h       → encryption as mixing, SIMD
rolling_hash.h   → incremental updates, polynomial representation
perfect_hash.h   → collision-free mapping, compile-time construction
```

## Building and Testing

```bash
cmake -B build
cmake --build build --target hash_test perfect_hash_test hardware_hash_test
ctest --test-dir build -L hash -V
```

## Dependencies

- C++26 (for constexpr features)
- SSE4.2 (for CRC32C)
- AES-NI (for AES hash)
- Both are available on all x86 CPUs since ~2010
