load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library", "cc_test")

cc_library(
  name = "symbolic",
  hdrs = ["symbolic.h"],
  deps = [
    "//src:compile_time_string",
  ],
)

cc_library(
  name = "operators",
  hdrs = ["operators.h"],
  deps = [
    ":symbolic",
  ],
)

cc_test(
  name = "symbolic_test",
  srcs = ["symbolic_test.cpp"],
  deps = [
    ":symbolic",
    ":operators",
    "//src:unit",
  ],
)
