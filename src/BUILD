load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library", "cc_test")

cc_library(
  name = "benchmark",
  hdrs = ["benchmark.h"],
  visibility = ["//visibility:public"],
)

cc_library(
  name = "compile_time_string",
  hdrs = ["compile_time_string.h"],
  visibility = ["//visibility:public"],
)

cc_test(
  name = "compile_time_string_test",
  srcs = ["compile_time_string_test.cpp"],
  deps = [
    ":compile_time_string",
    ":unit",
  ],
)

cc_library(
  name = "fifo_queue",
  hdrs = ["fifo_queue.h"],
  visibility = ["//visibility:public"],
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
  visibility = ["//visibility:public"],
)

cc_library(
  name = "type_list",
  hdrs = ["type_list.h"],
  deps = [
    ":type_name",
  ],
  visibility = ["//visibility:public"],
)

cc_test(
  name = "type_list_test",
  srcs = ["type_list_test.cpp"],
  deps = [
    ":type_list",
    ":unit",
  ],
)

cc_library(
  name = "type_name",
  hdrs = ["type_name.h"],
  visibility = ["//visibility:public"],
)

cc_test(
  name = "type_name_test",
  srcs = ["type_name_test.cpp"],
  deps = [
    ":type_name",
    ":unit",
  ],
)

cc_library(
  name = "unit",
  hdrs = ["unit.h"],
  visibility = ["//visibility:public"],
)

