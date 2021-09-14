load("@rules_cc//cc:defs.bzl", "cc_library")
cc_library(
    name = "asio",
    hdrs = glob(["include/**/*.hpp", "include/**/*.ipp"]),
    visibility =  ["//visibility:public"],
    strip_include_prefix = "include",
    defines = [
        "ASIO_STANDALONE",
    ],
)