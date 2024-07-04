load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library", "cc_test")

cc_library(
  name = "matchers",
  hdrs = ["matchers.h"],
  deps = [
    ":symbolic",
  ],
)

cc_test(
  name = "matchers_test",
  srcs = ["matchers_test.cpp"],
  deps = [
    ":matchers",
    ":operators",
    ":symbolic",
    "//src:type_list",
    "//src:unit",
  ],
)

cc_library(
  name = "meta",
  hdrs = ["meta.h"],
  deps = [
    ":symbolic",
  ],
)

cc_test(
  name = "meta_test",
  srcs = ["meta_test.cpp"],
  deps = [
    ":matchers",
    ":meta",
    ":operators",
    ":symbolic",
    "//src:unit",
  ],
)

cc_library(
  name = "operators",
  hdrs = ["operators.h"],
  deps = [
    ":symbolic",
  ],
)

cc_library(
  name = "simplify",
  hdrs = ["simplify.h"],
  deps = [
    ":operators",
    ":symbolic",
    ":matchers",
    ":meta",
    "//src:type_list",
  ],
)

cc_test(
  name = "simplify_test",
  srcs = ["simplify_test.cpp"],
  deps = [
    ":simplify",
    ":operators",
    ":symbolic",
    ":to_string",
    "//src:unit",
  ],
)

cc_library(
  name = "symbolic",
  hdrs = ["symbolic.h"],
  deps = [
    "//src:type_list",
  ],
)

cc_test(
  name = "symbolic_test",
  srcs = ["symbolic_test.cpp"],
  deps = [
    ":operators",
    ":symbolic",
    "//src:unit",
  ],
)

cc_library(
  name = "to_string",
  hdrs = ["to_string.h"],
  deps = [
    ":symbolic",
  ],
)

cc_test(
  name = "to_string_test",
  srcs = ["to_string_test.cpp"],
  deps = [
    ":operators",
    ":symbolic",
    ":to_string",
    "//src:unit",
  ],
)

