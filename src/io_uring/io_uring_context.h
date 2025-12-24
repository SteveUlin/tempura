// IoUringContext - Thread-safe io_uring execution context with callbacks
//
// This is the recommended way to use io_uring for most applications. It wraps
// the low-level IoUring class and provides:
//
//   - Thread-safe submission from any thread
//   - Automatic completion dispatch via callbacks
//   - Proper lifecycle management (graceful shutdown)
//
// Architecture:
//   ┌──────────────────────────────────────────────────────────────────┐
//   │                         IoUringContext                           │
//   ├──────────────────────────────────────────────────────────────────┤
//   │  Worker threads ──► asyncRead/Write ──► Submission Queue (SQ)   │
//   │                                              │                   │
//   │                                              ▼ submit()          │
//   │                                         ┌─────────┐              │
//   │                                         │ Kernel  │              │
//   │                                         └────┬────┘              │
//   │                                              ▼                   │
//   │  IO thread ◄── run() ◄── waitCqe() ◄── Completion Queue (CQ)    │
//   │       │                                                          │
//   │       ▼                                                          │
//   │  callback(result) ──► your code handles the result              │
//   └──────────────────────────────────────────────────────────────────┘
//
// The key insight: one dedicated thread calls run() to process completions,
// while any number of threads can submit operations concurrently.

#pragma once

#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>
#include <unordered_map>

#include "io_uring.h"

namespace tempura {

// Callback signature for completion notifications.
// The result parameter is bytes transferred (positive) or -errno (negative).
using IoCompletionCallback = std::move_only_function<void(int32_t result)>;

// IoUringContext - manages io_uring operations across multiple threads
//
// Quick start:
//   // 1. Create the ring and context
//   auto ring = IoUring::create(256).value();
//   IoUringContext ctx{std::move(ring)};
//
//   // 2. Start a thread to process completions
//   std::thread io_thread([&ctx] { ctx.run(); });
//
//   // 3. Submit async operations from any thread
//   ctx.asyncRead(fd, buffer, size, offset, [](int32_t result) {
//     if (result < 0) {
//       // Error: result is -errno (e.g., -EAGAIN, -EIO)
//       std::cerr << "Read failed: " << std::strerror(-result) << "\n";
//     } else {
//       // Success: result is bytes read
//       std::cout << "Read " << result << " bytes\n";
//     }
//   });
//
//   // 4. Graceful shutdown (waits for pending ops to complete)
//   ctx.stop();
//   io_thread.join();
//
// Thread-safety:
//   - asyncRead/Write/etc: safe to call from any thread
//   - run(): must be called from exactly one thread (the "IO thread")
//   - stop(): safe to call from any thread
//
// Buffer lifetime:
//   Your buffers must remain valid until the callback is invoked. The kernel
//   reads/writes directly to your memory asynchronously.
//
class IoUringContext {
 public:
  explicit IoUringContext(IoUring ring) : ring_{std::move(ring)} {}

  // Non-copyable, non-movable (synchronization point)
  IoUringContext(const IoUringContext&) = delete;
  IoUringContext(IoUringContext&&) = delete;
  auto operator=(const IoUringContext&) -> IoUringContext& = delete;
  auto operator=(IoUringContext&&) -> IoUringContext& = delete;

  ~IoUringContext() { stop(); }

  // ─────────────────────────────────────────────────────────────────────────
  // Event loop (call from dedicated IO thread)
  // ─────────────────────────────────────────────────────────────────────────

  // Process completions until stop() is called and pending ops complete.
  // This is a blocking call that should run on a dedicated thread.
  // It will block on waitCqe() when idle, waking to process completions.
  void run() {
    while (true) {
      {
        std::unique_lock lock{mutex_};
        if (stopped_ && pending_count_ == 0) break;
      }

      // Wait for completions
      auto result = ring_.waitCqe();
      if (!result) {
        if (result.error() == std::errc::interrupted) continue;
        // Check if we should exit
        std::unique_lock lock{mutex_};
        if (stopped_ && pending_count_ == 0) break;
        continue;
      }

      auto* cqe = *result;
      IoCompletionCallback callback;

      {
        std::scoped_lock lock{mutex_};
        uint64_t id = io_uring_cqe_get_data64(cqe);
        auto it = callbacks_.find(id);
        if (it != callbacks_.end()) {
          callback = std::move(it->second);
          callbacks_.erase(it);
          --pending_count_;
        }
      }

      int32_t res = cqe->res;
      ring_.seenCqe(cqe);

      // Invoke callback outside the lock
      if (callback) {
        callback(res);
      }
    }
  }

  // Signal shutdown. No new submissions accepted after this call.
  // The run() loop will exit after all pending operations complete.
  void stop() {
    {
      std::scoped_lock lock{mutex_};
      stopped_ = true;
    }
    wakeUp();  // Submit a NOP to unblock waitCqe()
  }

  // ─────────────────────────────────────────────────────────────────────────
  // Async operations (call from any thread)
  // ─────────────────────────────────────────────────────────────────────────

  // Read up to nbytes from fd at offset into buf.
  // Returns false if stopped or queue full. Callback receives bytes read.
  auto asyncRead(int fd, void* buf, unsigned nbytes, uint64_t offset,
                 IoCompletionCallback callback) -> bool {
    return submitOperation(
        [&](io_uring_sqe* sqe) {
          io_uring_prep_read(sqe, fd, buf, nbytes, offset);
        },
        std::move(callback));
  }

  // Write nbytes from buf to fd at offset.
  // Returns false if stopped or queue full. Callback receives bytes written.
  auto asyncWrite(int fd, const void* buf, unsigned nbytes, uint64_t offset,
                  IoCompletionCallback callback) -> bool {
    return submitOperation(
        [&](io_uring_sqe* sqe) {
          io_uring_prep_write(sqe, fd, buf, nbytes, offset);
        },
        std::move(callback));
  }

  // Scatter read: read into multiple buffers at once.
  // The iovec array and buffers must remain valid until callback.
  auto asyncReadv(int fd, const iovec* iovecs, unsigned nr_vecs,
                  uint64_t offset, IoCompletionCallback callback) -> bool {
    return submitOperation(
        [&](io_uring_sqe* sqe) {
          io_uring_prep_readv(sqe, fd, iovecs, nr_vecs, offset);
        },
        std::move(callback));
  }

  // Gather write: write from multiple buffers at once.
  // The iovec array and buffers must remain valid until callback.
  auto asyncWritev(int fd, const iovec* iovecs, unsigned nr_vecs,
                   uint64_t offset, IoCompletionCallback callback) -> bool {
    return submitOperation(
        [&](io_uring_sqe* sqe) {
          io_uring_prep_writev(sqe, fd, iovecs, nr_vecs, offset);
        },
        std::move(callback));
  }

  // Flush file data to disk. Callback receives 0 on success.
  auto asyncFsync(int fd, IoCompletionCallback callback) -> bool {
    return submitOperation(
        [&](io_uring_sqe* sqe) { io_uring_prep_fsync(sqe, fd, 0); },
        std::move(callback));
  }

  // No-op that completes immediately. Useful for testing or wakeup.
  auto asyncNop(IoCompletionCallback callback) -> bool {
    return submitOperation(
        [&](io_uring_sqe* sqe) { io_uring_prep_nop(sqe); },
        std::move(callback));
  }

  // ─────────────────────────────────────────────────────────────────────────
  // Status queries
  // ─────────────────────────────────────────────────────────────────────────

  // Number of operations submitted but not yet completed.
  [[nodiscard]] auto pendingCount() const -> size_t {
    std::scoped_lock lock{mutex_};
    return pending_count_;
  }

  // True after stop() is called.
  [[nodiscard]] auto isStopped() const -> bool {
    std::scoped_lock lock{mutex_};
    return stopped_;
  }

  // Access underlying ring for advanced operations (linked SQEs, multishot).
  [[nodiscard]] auto ring() -> IoUring& { return ring_; }
  [[nodiscard]] auto ring() const -> const IoUring& { return ring_; }

 private:
  // Internal: submit an operation with mutex held
  template <typename PrepFn>
  auto submitOperation(PrepFn&& prep, IoCompletionCallback callback) -> bool {
    std::scoped_lock lock{mutex_};

    if (stopped_) return false;

    auto* sqe = ring_.getSqe();
    if (!sqe) return false;  // SQ full, caller should retry or backoff

    prep(sqe);

    // Associate callback with a unique ID for completion matching
    uint64_t id = next_id_++;
    io_uring_sqe_set_data64(sqe, id);
    callbacks_[id] = std::move(callback);
    ++pending_count_;

    ring_.submit();
    return true;
  }

  // Wake run() by submitting a NOP (unblocks waitCqe)
  void wakeUp() {
    std::scoped_lock lock{mutex_};
    auto* sqe = ring_.getSqe();
    if (sqe) {
      io_uring_prep_nop(sqe);
      io_uring_sqe_set_data64(sqe, 0);  // ID 0 reserved for internal wakeup
      ring_.submit();
    }
  }

  IoUring ring_;
  mutable std::mutex mutex_;                                // Protects all below
  std::unordered_map<uint64_t, IoCompletionCallback> callbacks_;  // ID → callback
  uint64_t next_id_{1};      // Next callback ID (0 is wakeup sentinel)
  size_t pending_count_{0};  // Ops submitted but not completed
  bool stopped_{false};      // Shutdown requested
};

}  // namespace tempura
