#pragma once

#include <chrono>
#include <string>
#include <string_view>
#include <vector>

namespace minidbg::test {

// Absolute path of a compiled test program, e.g. program("hello").
std::string program(std::string_view name);
// Absolute path of a test program's source file, e.g. source("basic.c").
std::string source(std::string_view name);

// 1-based number of the first line in `source_file` containing `marker`. Tests find lines via
// comment markers such as /* BREAK_HERE */ so editing a test program cannot silently break
// hard-coded line numbers. Fails the test (throws) if the marker is missing.
int line_of(std::string_view source_file, std::string_view marker);

struct RunResult {
    std::string output;  // stdout and stderr, interleaved
    int exit_status = -1;
    bool timed_out = false;
};

// Runs the minidbg binary with `args`, feeding `commands` (one per line) on stdin.
RunResult run_minidbg(const std::vector<std::string>& args,
                      const std::vector<std::string>& commands,
                      std::chrono::seconds timeout = std::chrono::seconds(30));

// Runs `path args...` and captures its output (used to get expected output of a program).
RunResult run_program(const std::string& path, const std::vector<std::string>& args,
                      std::chrono::seconds timeout = std::chrono::seconds(30));

// Number of non-overlapping occurrences of `needle` in `haystack`.
int count_occurrences(std::string_view haystack, std::string_view needle);

}  // namespace minidbg::test
