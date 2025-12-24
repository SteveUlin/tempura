#include "io_uring/io_uring_sender.h"

#include <fcntl.h>
#include <unistd.h>

#include <cstring>
#include <thread>

#include "task/task.h"
#include "unit.h"

using namespace tempura;

auto main() -> int {
  "IoUringScheduler concept"_test = [] {
    auto ring = IoUring::create(32).value();
    IoUringContext ctx{std::move(ring)};

    IoUringScheduler scheduler{ctx};
    static_assert(Scheduler<IoUringScheduler>);

    auto sender = scheduler.schedule();
    static_assert(Sender<decltype(sender)>);
  };

  "asyncRead sender"_test = [] {
    // Create test file
    char path[] = "build/io_uring_sender_read_XXXXXX";
    int fd = mkstemp(path);
    expectTrue(fd >= 0);

    const char* test_data = "Sender test data";
    write(fd, test_data, std::strlen(test_data));
    lseek(fd, 0, SEEK_SET);

    auto ring = IoUring::create(32).value();
    IoUringContext ctx{std::move(ring)};

    std::thread worker([&ctx] { ctx.run(); });

    char buffer[64] = {};
    auto result = syncWait(asyncRead(ctx, fd, buffer, sizeof(buffer), 0));

    expectTrue(result.has_value());
    auto [bytes_read] = *result;
    expectEq(static_cast<int32_t>(std::strlen(test_data)), bytes_read);
    expectEq(std::string_view{test_data}, std::string_view{buffer, static_cast<size_t>(bytes_read)});

    ctx.stop();
    worker.join();

    close(fd);
    unlink(path);
  };

  "asyncWrite sender"_test = [] {
    char path[] = "build/io_uring_sender_write_XXXXXX";
    int fd = mkstemp(path);
    expectTrue(fd >= 0);

    auto ring = IoUring::create(32).value();
    IoUringContext ctx{std::move(ring)};

    std::thread worker([&ctx] { ctx.run(); });

    const char* test_data = "Written via sender";
    auto result = syncWait(asyncWrite(ctx, fd, test_data, std::strlen(test_data), 0));

    expectTrue(result.has_value());
    auto [bytes_written] = *result;
    expectEq(static_cast<int32_t>(std::strlen(test_data)), bytes_written);

    // Verify by reading back
    lseek(fd, 0, SEEK_SET);
    char buffer[64] = {};
    read(fd, buffer, sizeof(buffer));
    expectEq(std::string_view{test_data}, std::string_view{buffer, std::strlen(test_data)});

    ctx.stop();
    worker.join();

    close(fd);
    unlink(path);
  };

  "chained read-then operations"_test = [] {
    char path[] = "build/io_uring_sender_chain_XXXXXX";
    int fd = mkstemp(path);
    expectTrue(fd >= 0);

    const char* test_data = "12345";
    write(fd, test_data, std::strlen(test_data));
    lseek(fd, 0, SEEK_SET);

    auto ring = IoUring::create(32).value();
    IoUringContext ctx{std::move(ring)};

    std::thread worker([&ctx] { ctx.run(); });

    char buffer[64] = {};

    // Read then transform to int
    auto work = asyncRead(ctx, fd, buffer, sizeof(buffer), 0) |
                then([&buffer](int32_t bytes_read) {
                  buffer[bytes_read] = '\0';
                  return std::atoi(buffer);
                });

    auto result = syncWait(std::move(work));
    expectTrue(result.has_value());
    auto [value] = *result;
    expectEq(12345, value);

    ctx.stop();
    worker.join();

    close(fd);
    unlink(path);
  };

  return TestRegistry::result();
}
