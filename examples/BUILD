load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library", "cc_test")

cc_binary(
  name = "hello_world",
  srcs = ["hello_world.cpp"],
)

cc_binary(
  name = "unit_main",
  srcs = ["unit_main.cpp"],
  deps = [
    "//src:unit",
  ],
)
