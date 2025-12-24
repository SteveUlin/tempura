// io_uring sender integration with tempura task system
//
// This module bridges io_uring with the sender/receiver model, allowing you
// to compose async I/O with other async operations using standard combinators.
//
// Why use senders instead of callbacks?
//   - Composable: chain operations with | then(...), | let_value(...)
//   - Structured: automatic lifetime and cancellation management
//   - Type-safe: errors are part of the type signature
//   - Integrates with the rest of the tempura task system
//
// Quick comparison:
//
//   // Callback style (IoUringContext):
//   ctx.asyncRead(fd, buf, size, 0, [&](int32_t result) {
//     if (result < 0) handleError(result);
//     else processData(buf, result);
//   });
//
//   // Sender style (this module):
//   auto work = asyncRead(ctx, fd, buf, size, 0)
//             | then([](int32_t bytes) { return processData(bytes); })
//             | let_error([](auto err) { return handleError(err); });
//   syncWait(std::move(work));
//
// Provided senders:
//   - asyncRead(ctx, fd, buf, size, offset) → int32_t bytes read
//   - asyncWrite(ctx, fd, buf, size, offset) → int32_t bytes written
//   - asyncFsync(ctx, fd) → void
//   - IoUringScheduler{ctx}.schedule() → void (for on(scheduler, work))
//
// All senders complete with SetValueTag on success, SetErrorTag(std::error_code)
// on failure, or SetStoppedTag if cancelled.

#pragma once

#include <utility>

#include "io_uring_context.h"
#include "task/concepts.h"
#include "task/completion_signatures.h"

namespace tempura {

// ============================================================================
// IoUringScheduler - schedules work on an IoUringContext
// ============================================================================
//
// A Scheduler that uses io_uring for continuation. When you use on(scheduler,
// work), the work will be started from the io_uring completion thread.
//
// This is useful for ensuring I/O operations execute on the io_uring thread:
//
//   IoUringScheduler scheduler{ctx};
//   auto work = on(scheduler, asyncRead(ctx, fd, buf, size, 0));
//
// The schedule() operation submits a NOP to io_uring, which completes
// immediately and triggers the continuation on the IO thread.

template <typename R>
class IoUringScheduleOperationState {
 public:
  IoUringScheduleOperationState(IoUringContext& ctx, R r)
      : ctx_{&ctx}, receiver_{std::move(r)} {}

  IoUringScheduleOperationState(const IoUringScheduleOperationState&) = delete;
  auto operator=(const IoUringScheduleOperationState&)
      -> IoUringScheduleOperationState& = delete;
  IoUringScheduleOperationState(IoUringScheduleOperationState&&) = delete;
  auto operator=(IoUringScheduleOperationState&&)
      -> IoUringScheduleOperationState& = delete;

  void start() noexcept {
    ctx_->asyncNop([this](int32_t) noexcept { receiver_.setValue(); });
  }

 private:
  IoUringContext* ctx_;
  R receiver_;
};

class IoUringScheduleSender {
 public:
  template <typename Env = EmptyEnv>
  using CompletionSignatures =
      tempura::CompletionSignatures<SetValueTag(), SetStoppedTag()>;

  explicit IoUringScheduleSender(IoUringContext& ctx) : ctx_{&ctx} {}

  template <ReceiverOf<> R>
  auto connect(R&& receiver) && {
    return IoUringScheduleOperationState<std::remove_cvref_t<R>>{
        *ctx_, std::forward<R>(receiver)};
  }

 private:
  IoUringContext* ctx_;
};

class IoUringScheduler {
 public:
  explicit IoUringScheduler(IoUringContext& ctx) : ctx_{&ctx} {}

  auto schedule() const { return IoUringScheduleSender{*ctx_}; }

  friend constexpr auto operator==(IoUringScheduler lhs, IoUringScheduler rhs)
      -> bool {
    return lhs.ctx_ == rhs.ctx_;
  }

 private:
  IoUringContext* ctx_;
};

static_assert(Scheduler<IoUringScheduler>);

// ============================================================================
// Async Read Sender
// ============================================================================
//
// Sender that reads from a file descriptor asynchronously.
//
// Completion signals:
//   - SetValueTag(int32_t): bytes read (may be less than requested)
//   - SetErrorTag(std::error_code): I/O error (e.g., ENOENT, EIO)
//   - SetStoppedTag(): operation was cancelled
//
// Example:
//   char buffer[4096];
//   auto result = syncWait(asyncRead(ctx, fd, buffer, sizeof(buffer), 0));
//   if (result) {
//     auto [bytes] = *result;
//     process(buffer, bytes);
//   }

template <typename R>
class AsyncReadOperationState {
 public:
  AsyncReadOperationState(IoUringContext& ctx, int fd, void* buf,
                          unsigned nbytes, uint64_t offset, R r)
      : ctx_{&ctx},
        fd_{fd},
        buf_{buf},
        nbytes_{nbytes},
        offset_{offset},
        receiver_{std::move(r)} {}

  AsyncReadOperationState(const AsyncReadOperationState&) = delete;
  auto operator=(const AsyncReadOperationState&)
      -> AsyncReadOperationState& = delete;
  AsyncReadOperationState(AsyncReadOperationState&&) = delete;
  auto operator=(AsyncReadOperationState&&)
      -> AsyncReadOperationState& = delete;

  void start() noexcept {
    ctx_->asyncRead(fd_, buf_, nbytes_, offset_,
                    [this](int32_t result) noexcept {
                      if (result < 0) {
                        receiver_.setError(
                            std::make_error_code(static_cast<std::errc>(-result)));
                      } else {
                        receiver_.setValue(std::move(result));
                      }
                    });
  }

 private:
  IoUringContext* ctx_;
  int fd_;
  void* buf_;
  unsigned nbytes_;
  uint64_t offset_;
  R receiver_;
};

class AsyncReadSender {
 public:
  template <typename Env = EmptyEnv>
  using CompletionSignatures = tempura::CompletionSignatures<
      SetValueTag(int32_t), SetErrorTag(std::error_code), SetStoppedTag()>;

  AsyncReadSender(IoUringContext& ctx, int fd, void* buf, unsigned nbytes,
                  uint64_t offset)
      : ctx_{&ctx}, fd_{fd}, buf_{buf}, nbytes_{nbytes}, offset_{offset} {}

  template <typename R>
  auto connect(R&& receiver) && {
    return AsyncReadOperationState<std::remove_cvref_t<R>>{
        *ctx_, fd_, buf_, nbytes_, offset_, std::forward<R>(receiver)};
  }

 private:
  IoUringContext* ctx_;
  int fd_;
  void* buf_;
  unsigned nbytes_;
  uint64_t offset_;
};

// Create a sender that reads from fd. Buffer must remain valid until complete.
inline auto asyncRead(IoUringContext& ctx, int fd, void* buf, unsigned nbytes,
                      uint64_t offset = 0) {
  return AsyncReadSender{ctx, fd, buf, nbytes, offset};
}

// ============================================================================
// Async Write Sender
// ============================================================================
//
// Sender that writes to a file descriptor asynchronously.
//
// Completion signals:
//   - SetValueTag(int32_t): bytes written (may be less than requested)
//   - SetErrorTag(std::error_code): I/O error (e.g., ENOSPC, EIO)
//   - SetStoppedTag(): operation was cancelled

template <typename R>
class AsyncWriteOperationState {
 public:
  AsyncWriteOperationState(IoUringContext& ctx, int fd, const void* buf,
                           unsigned nbytes, uint64_t offset, R r)
      : ctx_{&ctx},
        fd_{fd},
        buf_{buf},
        nbytes_{nbytes},
        offset_{offset},
        receiver_{std::move(r)} {}

  AsyncWriteOperationState(const AsyncWriteOperationState&) = delete;
  auto operator=(const AsyncWriteOperationState&)
      -> AsyncWriteOperationState& = delete;
  AsyncWriteOperationState(AsyncWriteOperationState&&) = delete;
  auto operator=(AsyncWriteOperationState&&)
      -> AsyncWriteOperationState& = delete;

  void start() noexcept {
    ctx_->asyncWrite(fd_, buf_, nbytes_, offset_,
                     [this](int32_t result) noexcept {
                       if (result < 0) {
                         receiver_.setError(
                             std::make_error_code(static_cast<std::errc>(-result)));
                       } else {
                         receiver_.setValue(std::move(result));
                       }
                     });
  }

 private:
  IoUringContext* ctx_;
  int fd_;
  const void* buf_;
  unsigned nbytes_;
  uint64_t offset_;
  R receiver_;
};

class AsyncWriteSender {
 public:
  template <typename Env = EmptyEnv>
  using CompletionSignatures = tempura::CompletionSignatures<
      SetValueTag(int32_t), SetErrorTag(std::error_code), SetStoppedTag()>;

  AsyncWriteSender(IoUringContext& ctx, int fd, const void* buf,
                   unsigned nbytes, uint64_t offset)
      : ctx_{&ctx}, fd_{fd}, buf_{buf}, nbytes_{nbytes}, offset_{offset} {}

  template <typename R>
  auto connect(R&& receiver) && {
    return AsyncWriteOperationState<std::remove_cvref_t<R>>{
        *ctx_, fd_, buf_, nbytes_, offset_, std::forward<R>(receiver)};
  }

 private:
  IoUringContext* ctx_;
  int fd_;
  const void* buf_;
  unsigned nbytes_;
  uint64_t offset_;
};

// Create a sender that writes to fd. Buffer must remain valid until complete.
inline auto asyncWrite(IoUringContext& ctx, int fd, const void* buf,
                       unsigned nbytes, uint64_t offset = 0) {
  return AsyncWriteSender{ctx, fd, buf, nbytes, offset};
}

// ============================================================================
// Async Fsync Sender
// ============================================================================
//
// Sender that flushes file data to disk asynchronously.
//
// Completion signals:
//   - SetValueTag(): flush successful
//   - SetErrorTag(std::error_code): I/O error
//   - SetStoppedTag(): operation was cancelled

template <typename R>
class AsyncFsyncOperationState {
 public:
  AsyncFsyncOperationState(IoUringContext& ctx, int fd, R r)
      : ctx_{&ctx}, fd_{fd}, receiver_{std::move(r)} {}

  AsyncFsyncOperationState(const AsyncFsyncOperationState&) = delete;
  auto operator=(const AsyncFsyncOperationState&)
      -> AsyncFsyncOperationState& = delete;
  AsyncFsyncOperationState(AsyncFsyncOperationState&&) = delete;
  auto operator=(AsyncFsyncOperationState&&)
      -> AsyncFsyncOperationState& = delete;

  void start() noexcept {
    ctx_->asyncFsync(fd_, [this](int32_t result) noexcept {
      if (result < 0) {
        receiver_.setError(
            std::make_error_code(static_cast<std::errc>(-result)));
      } else {
        receiver_.setValue();
      }
    });
  }

 private:
  IoUringContext* ctx_;
  int fd_;
  R receiver_;
};

class AsyncFsyncSender {
 public:
  template <typename Env = EmptyEnv>
  using CompletionSignatures = tempura::CompletionSignatures<
      SetValueTag(), SetErrorTag(std::error_code), SetStoppedTag()>;

  AsyncFsyncSender(IoUringContext& ctx, int fd) : ctx_{&ctx}, fd_{fd} {}

  template <typename R>
  auto connect(R&& receiver) && {
    return AsyncFsyncOperationState<std::remove_cvref_t<R>>{
        *ctx_, fd_, std::forward<R>(receiver)};
  }

 private:
  IoUringContext* ctx_;
  int fd_;
};

// Create a sender that flushes fd to disk.
inline auto asyncFsync(IoUringContext& ctx, int fd) {
  return AsyncFsyncSender{ctx, fd};
}

}  // namespace tempura
