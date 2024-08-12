#include <sys/wait.h>
#include <unistd.h>

#include <cstdint>
#include <iostream>
#include <print>

// Linux (unix?) has an odd method of creating new processes, the system calls
// fork and exec.
// Fork:
//   Creates a new process that is a copy of the current process. The new
//   process has a new PID, but is otherwise identical to the
//   original process. Memory is marked as copy on write, so the memory is not
//   copied until one of the processes writes to it.
// Exec:
//   Replaces the current process with a new process. This literally overwrites
//   the code segment of the current process with the code of the new process.
auto main() -> int {
  const int64_t data = 100;

  pid_t pid = fork();

  if (pid < 0) {
    std::cerr << "Failed to fork\n";
    return 1;
  }
  if (pid == 0) {
    std::println("Child process: {}", getpid());
    std::println("Child data: {}", data);  // 100
  } else {
    // Do not run until the child process has completed
    wait(nullptr);
    std::println("Parent process: {}", getpid());
    std::println("Parent data: {}", data);  // 100
  }

  return 0;
}
