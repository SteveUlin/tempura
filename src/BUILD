load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library", "cc_test")

cc_library(
  name = "benchmark",
  hdrs = ["benchmark.h"],
)

cc_library(
  name = "fifo_queue",
  hdrs = ["fifo_queue.h"],
)

cc_test(
  name = "fifo_queue_test",
  srcs = ["fifo_queue_test.cpp"],
  deps = [
    ":benchmark",
    ":fifo_queue",
    ":profiler",
    ":unit",
  ],
)

cc_binary(
  name = "hello_world",
  srcs = ["hello_world.cpp"],
)

cc_library(
  name = "profiler",
  hdrs = ["profiler.h"],
  srcs = ["profiler.cpp"],
)

cc_library(
  name = "unit",
  hdrs = ["unit.h"],
  visibility = ["//visibility:public"],
)

