#include "io_uring/io_uring_context.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstring>
#include <latch>
#include <thread>

#include "unit.h"

using namespace tempura;
using namespace std::chrono_literals;

auto main() -> int {
  "IoUring creation"_test = [] {
    auto result = IoUring::create(32);
    expectTrue(result.has_value());
  };

  "IoUring nop operation"_test = [] {
    auto ring = IoUring::create(32).value();

    auto* sqe = ring.getSqe();
    expectTrue(sqe != nullptr);

    ring.prepareNop(sqe);
    ring.setUserData(sqe, 42);

    auto submit_result = ring.submit();
    expectTrue(submit_result.has_value());
    expectEq(1, *submit_result);

    auto wait_result = ring.waitCqe();
    expectTrue(wait_result.has_value());

    auto* cqe = *wait_result;
    expectEq(0, cqe->res);
    expectEq(42ul, io_uring_cqe_get_data64(cqe));

    ring.seenCqe(cqe);
  };

  "IoUringContext async nop"_test = [] {
    auto ring = IoUring::create(32).value();
    IoUringContext ctx{std::move(ring)};

    std::latch done{1};
    int32_t captured_result = -999;

    std::thread worker([&ctx] { ctx.run(); });

    ctx.asyncNop([&](int32_t result) {
      captured_result = result;
      done.count_down();
    });

    done.wait();
    ctx.stop();
    worker.join();

    expectEq(0, captured_result);
  };

  "IoUringContext file read/write"_test = [] {
    // Create a temporary file
    char path[] = "build/io_uring_test_XXXXXX";
    int fd = mkstemp(path);
    expectTrue(fd >= 0);

    const char* test_data = "Hello, io_uring!";
    size_t data_len = std::strlen(test_data);

    auto ring = IoUring::create(32).value();
    IoUringContext ctx{std::move(ring)};

    std::latch write_done{1};
    std::latch read_done{1};
    int32_t write_result = -1;
    int32_t read_result = -1;
    char read_buffer[64] = {};

    std::thread worker([&ctx] { ctx.run(); });

    // Async write
    ctx.asyncWrite(fd, test_data, data_len, 0, [&](int32_t result) {
      write_result = result;
      write_done.count_down();
    });

    write_done.wait();
    expectEq(static_cast<int32_t>(data_len), write_result);

    // Async read
    ctx.asyncRead(fd, read_buffer, sizeof(read_buffer), 0, [&](int32_t result) {
      read_result = result;
      read_done.count_down();
    });

    read_done.wait();
    expectEq(static_cast<int32_t>(data_len), read_result);
    expectEq(std::string_view{test_data}, std::string_view{read_buffer, data_len});

    ctx.stop();
    worker.join();

    close(fd);
    unlink(path);
  };

  "IoUringContext multiple operations"_test = [] {
    auto ring = IoUring::create(32).value();
    IoUringContext ctx{std::move(ring)};

    constexpr int kNumOps = 10;
    std::latch done{kNumOps};
    std::atomic<int> completed{0};

    std::thread worker([&ctx] { ctx.run(); });

    for (int i = 0; i < kNumOps; ++i) {
      ctx.asyncNop([&](int32_t) {
        ++completed;
        done.count_down();
      });
    }

    done.wait();
    ctx.stop();
    worker.join();

    expectEq(kNumOps, completed.load());
  };

  return TestRegistry::result();
}
