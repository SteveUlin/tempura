// io_uring Demo - Threaded async read/write operations
//
// Demonstrates using io_uring with the tempura task library:
// - Multiple threads submitting I/O operations
// - Thread pool integration
// - Async file read/write with senders

#include <fcntl.h>
#include <unistd.h>

#include <array>
#include <cstring>
#include <format>
#include <iostream>
#include <latch>
#include <thread>

#include "io_uring/io_uring_context.h"
#include "io_uring/io_uring_sender.h"
#include "synchronization/threadpool.h"
#include "task/task.h"

using namespace tempura;

// Create a test file with some data
auto createTestFile(const char* path, size_t size) -> int {
  int fd = open(path, O_RDWR | O_CREAT | O_TRUNC, 0644);
  if (fd < 0) {
    std::cerr << "Failed to create file: " << path << "\n";
    return -1;
  }

  // Fill with sequential bytes
  std::vector<char> data(size);
  for (size_t i = 0; i < size; ++i) {
    data[i] = static_cast<char>('A' + (i % 26));
  }
  write(fd, data.data(), data.size());
  return fd;
}

int main() {
  std::cout << "=== io_uring Demo ===\n\n";

  // Create the io_uring context
  auto ring = IoUring::create(256);
  if (!ring) {
    std::cerr << "Failed to create io_uring: " << ring.error().message() << "\n";
    return 1;
  }
  IoUringContext ctx{std::move(*ring)};

  // Start the io_uring completion processing thread
  std::thread io_thread([&ctx] {
    std::cout << "[IO Thread] Started processing completions\n";
    ctx.run();
    std::cout << "[IO Thread] Stopped\n";
  });

  // Create thread pool for submitting operations
  ThreadPool pool{4};
  std::cout << "[Main] Created thread pool with 4 workers\n\n";

  // =========================================================================
  // Demo 1: Multiple threads writing to the same file
  // =========================================================================
  std::cout << "--- Demo 1: Parallel writes from multiple threads ---\n";
  {
    const char* path = "build/io_uring_demo_writes.txt";
    int fd = open(path, O_RDWR | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
      std::cerr << "Failed to create file\n";
      return 1;
    }

    constexpr int kNumWriters = 8;
    constexpr size_t kChunkSize = 64;
    std::latch done{kNumWriters};

    // Each thread writes its ID repeated in a chunk
    for (int i = 0; i < kNumWriters; ++i) {
      pool.enqueue([&ctx, fd, i, &done] {
        std::array<char, kChunkSize> buffer;
        std::fill(buffer.begin(), buffer.end(), static_cast<char>('0' + i));

        uint64_t offset = i * kChunkSize;
        ctx.asyncWrite(fd, buffer.data(), buffer.size(), offset,
                       [i, &done](int32_t result) {
                         std::cout << std::format("  [Writer {}] Wrote {} bytes\n", i, result);
                         done.count_down();
                       });
      });
    }

    done.wait();
    fsync(fd);

    // Read back and verify
    std::array<char, kNumWriters * kChunkSize> read_buffer{};
    lseek(fd, 0, SEEK_SET);
    read(fd, read_buffer.data(), read_buffer.size());

    std::cout << "  Result preview: ";
    for (int i = 0; i < kNumWriters; ++i) {
      std::cout << read_buffer[i * kChunkSize];
    }
    std::cout << "...\n";

    close(fd);
    unlink(path);
  }

  // =========================================================================
  // Demo 2: Concurrent reads from multiple offsets
  // =========================================================================
  std::cout << "\n--- Demo 2: Parallel reads from multiple offsets ---\n";
  {
    const char* path = "build/io_uring_demo_reads.txt";
    int fd = createTestFile(path, 1024);
    if (fd < 0) return 1;

    constexpr int kNumReaders = 4;
    constexpr size_t kChunkSize = 64;
    std::latch done{kNumReaders};

    std::array<std::array<char, kChunkSize>, kNumReaders> buffers{};

    for (int i = 0; i < kNumReaders; ++i) {
      pool.enqueue([&ctx, fd, i, &buffers, &done] {
        uint64_t offset = i * 128;  // Staggered offsets
        ctx.asyncRead(fd, buffers[i].data(), buffers[i].size(), offset,
                      [i, &buffers, &done](int32_t result) {
                        std::cout << std::format(
                            "  [Reader {}] Read {} bytes: '{}...'\n",
                            i, result, std::string_view{buffers[i].data(), 8});
                        done.count_down();
                      });
      });
    }

    done.wait();
    close(fd);
    unlink(path);
  }

  // =========================================================================
  // Demo 3: Using senders with syncWait
  // =========================================================================
  std::cout << "\n--- Demo 3: Using senders with syncWait ---\n";
  {
    const char* path = "build/io_uring_demo_sender.txt";
    int fd = createTestFile(path, 256);
    if (fd < 0) return 1;

    std::array<char, 32> buffer{};

    // Use sender API with then() for transformation
    auto work = asyncRead(ctx, fd, buffer.data(), buffer.size(), 0) |
                then([&buffer](int32_t bytes_read) {
                  std::string_view data{buffer.data(), static_cast<size_t>(bytes_read)};
                  std::cout << std::format("  Read {} bytes: '{}'\n", bytes_read, data);
                  return bytes_read;
                });

    auto result = syncWait(std::move(work));
    if (result) {
      auto [bytes] = *result;
      std::cout << std::format("  syncWait returned: {} bytes\n", bytes);
    }

    close(fd);
    unlink(path);
  }

  // =========================================================================
  // Demo 4: Producer-consumer pattern with io_uring
  // =========================================================================
  std::cout << "\n--- Demo 4: Producer-consumer with ring buffer ---\n";
  {
    const char* path = "build/io_uring_demo_ringbuf.txt";
    int fd = open(path, O_RDWR | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) return 1;

    constexpr int kNumMessages = 16;
    constexpr size_t kMsgSize = 32;
    std::atomic<int> produced{0};
    std::atomic<int> consumed{0};
    std::latch all_done{kNumMessages * 2};  // Producers + consumers

    // Producer threads
    for (int i = 0; i < kNumMessages; ++i) {
      pool.enqueue([&ctx, fd, i, &produced, &all_done] {
        std::array<char, kMsgSize> msg{};
        auto text = std::format("Message-{:04d}", i);
        std::copy(text.begin(), text.end(), msg.begin());

        ctx.asyncWrite(fd, msg.data(), msg.size(), i * kMsgSize,
                       [i, &produced, &all_done](int32_t result) {
                         if (result > 0) ++produced;
                         all_done.count_down();
                       });
      });
    }

    // Consumer threads (delayed to let some writes complete)
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    for (int i = 0; i < kNumMessages; ++i) {
      pool.enqueue([&ctx, fd, i, &consumed, &all_done] {
        static thread_local std::array<char, kMsgSize> buf{};

        ctx.asyncRead(fd, buf.data(), buf.size(), i * kMsgSize,
                      [i, &consumed, &all_done](int32_t result) {
                        if (result > 0) ++consumed;
                        all_done.count_down();
                      });
      });
    }

    all_done.wait();
    std::cout << std::format("  Produced: {}, Consumed: {}\n",
                             produced.load(), consumed.load());

    close(fd);
    unlink(path);
  }

  // =========================================================================
  // Cleanup
  // =========================================================================
  std::cout << "\n=== Shutting down ===\n";
  pool.stop();
  ctx.stop();
  io_thread.join();

  std::cout << "Done!\n";
  return 0;
}
