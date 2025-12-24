// io_uring - Linux async I/O using liburing
//
// io_uring is a Linux kernel interface (5.1+) for high-performance async I/O.
// Unlike traditional async I/O (epoll, aio), io_uring uses shared ring buffers
// between userspace and kernel, eliminating most syscall overhead.
//
// Key concepts:
//   - Submission Queue (SQ): Userspace writes I/O requests (SQEs) here
//   - Completion Queue (CQ): Kernel writes results (CQEs) here
//   - Zero-copy: Buffers stay in place, kernel operates on user memory
//   - Batching: Multiple operations submitted/completed per syscall
//
// Typical workflow:
//   1. Get an SQE from the submission ring (getSqe)
//   2. Prepare the operation (prepareRead, prepareWrite, etc.)
//   3. Submit to kernel (submit)
//   4. Wait for completion (waitCqe) or poll (peekCqe)
//   5. Mark completion as seen (seenCqe) to free the CQE slot
//
// This class provides a low-level RAII wrapper. For thread-safe usage with
// callbacks, see IoUringContext. For sender/receiver integration, see
// io_uring_sender.h.

#pragma once

#include <liburing.h>

#include <cstring>
#include <expected>
#include <system_error>

namespace tempura {

// Error codes for io_uring operations
enum class IoUringError {
  kSetupFailed,
  kQueueFull,
  kSubmissionFailed,
  kNoCompletions,
};

inline auto ioUringErrorCategory() -> const std::error_category& {
  static struct Category : std::error_category {
    auto name() const noexcept -> const char* override { return "io_uring"; }
    auto message(int ev) const -> std::string override {
      switch (static_cast<IoUringError>(ev)) {
        case IoUringError::kSetupFailed:
          return "io_uring_setup failed";
        case IoUringError::kQueueFull:
          return "submission queue full";
        case IoUringError::kSubmissionFailed:
          return "io_uring submission failed";
        case IoUringError::kNoCompletions:
          return "no completions available";
      }
      return "unknown io_uring error";
    }
  } category;
  return category;
}

inline auto makeError(IoUringError e) -> std::error_code {
  return {static_cast<int>(e), ioUringErrorCategory()};
}

// Completion result from an I/O operation
//
// When an I/O operation completes, the kernel writes a Completion Queue Entry
// (CQE) to the completion ring. This struct provides a convenient C++ wrapper.
//
// The result field follows POSIX conventions:
//   - Positive: Success (bytes transferred for read/write, 0 for fsync/nop)
//   - Negative: Error code as -errno (e.g., -EAGAIN, -EIO)
struct CompletionEntry {
  int32_t result;      // Bytes transferred (positive) or -errno (negative)
  uint32_t flags;      // CQE flags (e.g., IORING_CQE_F_MORE for multishot)
  uint64_t user_data;  // Opaque ID you set when submitting (for correlation)

  [[nodiscard]] auto isError() const -> bool { return result < 0; }

  [[nodiscard]] auto errorCode() const -> std::error_code {
    if (result >= 0) return {};
    return std::make_error_code(static_cast<std::errc>(-result));
  }
};

// IoUring - RAII wrapper around liburing's io_uring
//
// Provides low-level access to Linux's io_uring async I/O interface.
// This is a thin wrapper that mirrors the liburing API with C++ conveniences.
//
// Thread-safety: NOT thread-safe. The underlying ring structures require
// external synchronization if accessed from multiple threads. For thread-safe
// usage with automatic callback dispatch, use IoUringContext instead.
//
// Example:
//   auto ring = IoUring::create(256).value();
//
//   // Submit a read
//   auto* sqe = ring.getSqe();
//   ring.prepareRead(sqe, fd, buffer, 4096, 0);
//   ring.setUserData(sqe, my_request_id);
//   ring.submit();
//
//   // Wait for completion
//   auto* cqe = ring.waitCqe().value();
//   int bytes_read = cqe->res;  // -errno on error
//   ring.seenCqe(cqe);          // MUST call to free the CQE slot
//
class IoUring {
 public:
  // Create an io_uring instance with the specified queue depth.
  // Queue depth determines max in-flight operations (typically 256-4096).
  // Common flags: IORING_SETUP_SQPOLL (kernel-side submission polling)
  static auto create(unsigned queue_depth, unsigned flags = 0)
      -> std::expected<IoUring, std::error_code> {
    IoUring ring;
    int ret = io_uring_queue_init(queue_depth, &ring.ring_, flags);
    if (ret < 0) {
      return std::unexpected{
          std::make_error_code(static_cast<std::errc>(-ret))};
    }
    ring.initialized_ = true;
    return ring;
  }

  IoUring() = default;

  // Non-copyable
  IoUring(const IoUring&) = delete;
  auto operator=(const IoUring&) -> IoUring& = delete;

  // Movable
  IoUring(IoUring&& other) noexcept : ring_{other.ring_}, initialized_{other.initialized_} {
    other.initialized_ = false;
  }

  auto operator=(IoUring&& other) noexcept -> IoUring& {
    if (this != &other) {
      cleanup();
      ring_ = other.ring_;
      initialized_ = other.initialized_;
      other.initialized_ = false;
    }
    return *this;
  }

  ~IoUring() { cleanup(); }

  // ─────────────────────────────────────────────────────────────────────────
  // Submission: Getting and preparing SQEs
  // ─────────────────────────────────────────────────────────────────────────

  // Get a Submission Queue Entry for a new operation.
  // Returns nullptr if the submission queue is full (queue_depth exceeded).
  // The SQE is valid until submit() is called.
  [[nodiscard]] auto getSqe() -> io_uring_sqe* {
    return io_uring_get_sqe(&ring_);
  }

  // Prepare a read: reads up to nbytes from fd at offset into buf.
  // The buffer must remain valid until the completion is received.
  // Offset is absolute file position (use -1 for current position on pipes).
  void prepareRead(io_uring_sqe* sqe, int fd, void* buf, unsigned nbytes,
                   uint64_t offset) {
    io_uring_prep_read(sqe, fd, buf, nbytes, offset);
  }

  // Prepare a write: writes nbytes from buf to fd at offset.
  // The buffer must remain valid until the completion is received.
  void prepareWrite(io_uring_sqe* sqe, int fd, const void* buf,
                    unsigned nbytes, uint64_t offset) {
    io_uring_prep_write(sqe, fd, buf, nbytes, offset);
  }

  // Scatter read: reads into multiple buffers (iovec array).
  // Useful for reading structured data into separate fields.
  void prepareReadv(io_uring_sqe* sqe, int fd, const iovec* iovecs,
                    unsigned nr_vecs, uint64_t offset) {
    io_uring_prep_readv(sqe, fd, iovecs, nr_vecs, offset);
  }

  // Gather write: writes from multiple buffers (iovec array).
  // Useful for writing headers + payload without copying.
  void prepareWritev(io_uring_sqe* sqe, int fd, const iovec* iovecs,
                     unsigned nr_vecs, uint64_t offset) {
    io_uring_prep_writev(sqe, fd, iovecs, nr_vecs, offset);
  }

  // Flush file data to disk. Flags: IORING_FSYNC_DATASYNC for data-only sync.
  void prepareFsync(io_uring_sqe* sqe, int fd, unsigned flags = 0) {
    io_uring_prep_fsync(sqe, fd, flags);
  }

  // No-op: completes immediately. Useful for testing or waking up waiters.
  void prepareNop(io_uring_sqe* sqe) { io_uring_prep_nop(sqe); }

  // Attach an opaque ID to this SQE. This value is returned in the CQE's
  // user_data field, allowing you to match completions to requests.
  void setUserData(io_uring_sqe* sqe, uint64_t data) {
    io_uring_sqe_set_data64(sqe, data);
  }

  // ─────────────────────────────────────────────────────────────────────────
  // Submission: Sending SQEs to the kernel
  // ─────────────────────────────────────────────────────────────────────────

  // Submit all pending SQEs to the kernel for processing.
  // Returns the number of SQEs submitted. This is the main syscall.
  auto submit() -> std::expected<int, std::error_code> {
    int ret = io_uring_submit(&ring_);
    if (ret < 0) {
      return std::unexpected{
          std::make_error_code(static_cast<std::errc>(-ret))};
    }
    return ret;
  }

  // Submit and block until at least wait_nr completions are available.
  // Combines submit() + waitCqe() for efficiency.
  auto submitAndWait(unsigned wait_nr = 1)
      -> std::expected<int, std::error_code> {
    int ret = io_uring_submit_and_wait(&ring_, wait_nr);
    if (ret < 0) {
      return std::unexpected{
          std::make_error_code(static_cast<std::errc>(-ret))};
    }
    return ret;
  }

  // ─────────────────────────────────────────────────────────────────────────
  // Completion: Retrieving CQEs from the kernel
  // ─────────────────────────────────────────────────────────────────────────

  // Non-blocking check for completions. Returns nullptr if none available.
  [[nodiscard]] auto peekCqe() -> io_uring_cqe* {
    io_uring_cqe* cqe = nullptr;
    int ret = io_uring_peek_cqe(&ring_, &cqe);
    if (ret < 0) return nullptr;
    return cqe;
  }

  // Block until a completion is available. Returns the CQE.
  // IMPORTANT: You must call seenCqe() after processing to free the slot.
  [[nodiscard]] auto waitCqe() -> std::expected<io_uring_cqe*, std::error_code> {
    io_uring_cqe* cqe = nullptr;
    int ret = io_uring_wait_cqe(&ring_, &cqe);
    if (ret < 0) {
      return std::unexpected{
          std::make_error_code(static_cast<std::errc>(-ret))};
    }
    return cqe;
  }

  // Mark a CQE as processed. MUST be called after handling each completion,
  // otherwise the completion queue will fill up and block new completions.
  void seenCqe(io_uring_cqe* cqe) { io_uring_cqe_seen(&ring_, cqe); }

  // Convenience: peek + extract + seen in one call. Returns our C++ wrapper.
  [[nodiscard]] auto popCompletion()
      -> std::expected<CompletionEntry, std::error_code> {
    auto* cqe = peekCqe();
    if (!cqe) {
      return std::unexpected{makeError(IoUringError::kNoCompletions)};
    }

    CompletionEntry entry{
        .result = cqe->res,
        .flags = cqe->flags,
        .user_data = io_uring_cqe_get_data64(cqe),
    };

    seenCqe(cqe);
    return entry;
  }

  // Check if there are completions available without consuming them.
  [[nodiscard]] auto hasCompletions() -> bool { return peekCqe() != nullptr; }

  // Access the raw liburing struct for advanced usage (multishot, linked ops).
  [[nodiscard]] auto raw() -> io_uring* { return &ring_; }
  [[nodiscard]] auto raw() const -> const io_uring* { return &ring_; }

 private:
  void cleanup() {
    if (initialized_) {
      io_uring_queue_exit(&ring_);
      initialized_ = false;
    }
  }

  io_uring ring_{};
  bool initialized_{false};
};

}  // namespace tempura
