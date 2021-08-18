load("@org_apache_rocketmq//bazel:combine.bzl", "cc_combine")

cc_combine(
    name = "rocketmq",
    link_static = False,
    output = "librocketmq.so",
    deps = [
        "//src/main/cpp/rocketmq:rocketmq_library",
    ],
)