#include "test_support.hpp"

#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <fstream>
#include <stdexcept>

namespace minidbg::test {

std::string program(std::string_view name) {
    return std::string(MINIDBG_PROGRAMS_DIR) + "/" + std::string(name);
}

std::string source(std::string_view name) {
    return std::string(MINIDBG_SOURCES_DIR) + "/" + std::string(name);
}

int line_of(std::string_view source_file, std::string_view marker) {
    std::ifstream in{std::string(source_file)};
    if (!in) {
        throw std::runtime_error("cannot open " + std::string(source_file));
    }
    std::string line;
    for (int number = 1; std::getline(in, line); ++number) {
        if (line.find(marker) != std::string::npos) {
            return number;
        }
    }
    throw std::runtime_error("marker '" + std::string(marker) + "' not found in " +
                             std::string(source_file));
}

int count_occurrences(std::string_view haystack, std::string_view needle) {
    int count = 0;
    for (std::size_t pos = haystack.find(needle); pos != std::string_view::npos;
         pos = haystack.find(needle, pos + needle.size())) {
        ++count;
    }
    return count;
}

namespace {

RunResult run_with_input(const std::string& path, const std::vector<std::string>& args,
                         const std::string& input, std::chrono::seconds timeout) {
    int in_pipe[2];
    int out_pipe[2];
    if (::pipe2(in_pipe, O_CLOEXEC) < 0 || ::pipe2(out_pipe, O_CLOEXEC) < 0) {
        throw std::runtime_error(std::string("pipe2: ") + std::strerror(errno));
    }
    pid_t pid = ::fork();
    if (pid < 0) {
        throw std::runtime_error(std::string("fork: ") + std::strerror(errno));
    }
    if (pid == 0) {
        ::dup2(in_pipe[0], STDIN_FILENO);
        ::dup2(out_pipe[1], STDOUT_FILENO);
        ::dup2(out_pipe[1], STDERR_FILENO);
        // Run in our own process group so a timeout can kill minidbg *and* its inferior.
        ::setpgid(0, 0);
        std::vector<char*> argv;
        argv.push_back(const_cast<char*>(path.c_str()));
        for (const auto& arg : args) {
            argv.push_back(const_cast<char*>(arg.c_str()));
        }
        argv.push_back(nullptr);
        ::execv(path.c_str(), argv.data());
        ::_exit(127);
    }
    ::close(in_pipe[0]);
    ::close(out_pipe[1]);
    ::setpgid(pid, pid);

    // The scripts are tiny (far below the 64 KiB pipe buffer), so this cannot block.
    std::size_t written = 0;
    while (written < input.size()) {
        ssize_t n = ::write(in_pipe[1], input.data() + written, input.size() - written);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            break;  // EPIPE: the debugger exited early; we still collect its output
        }
        written += static_cast<std::size_t>(n);
    }
    ::close(in_pipe[1]);

    RunResult result;
    auto deadline = std::chrono::steady_clock::now() + timeout;
    char buffer[4096];
    for (;;) {
        auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
            deadline - std::chrono::steady_clock::now());
        if (remaining.count() <= 0) {
            result.timed_out = true;
            ::kill(-pid, SIGKILL);
            break;
        }
        pollfd pfd{out_pipe[0], POLLIN, 0};
        int ready = ::poll(&pfd, 1, static_cast<int>(remaining.count()));
        if (ready < 0 && errno == EINTR) {
            continue;
        }
        if (ready <= 0) {
            continue;  // timeout is re-checked at the top of the loop
        }
        ssize_t n = ::read(out_pipe[0], buffer, sizeof(buffer));
        if (n <= 0) {
            break;  // EOF: every writer (minidbg and its inferior) is gone
        }
        result.output.append(buffer, static_cast<std::size_t>(n));
    }
    ::close(out_pipe[0]);

    int status = 0;
    while (::waitpid(pid, &status, 0) < 0 && errno == EINTR) {
    }
    result.exit_status = WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status);
    return result;
}

}  // namespace

RunResult run_minidbg(const std::vector<std::string>& args,
                      const std::vector<std::string>& commands, std::chrono::seconds timeout) {
    std::string input;
    for (const auto& command : commands) {
        input += command;
        input += '\n';
    }
    return run_with_input(MINIDBG_BINARY, args, input, timeout);
}

RunResult run_program(const std::string& path, const std::vector<std::string>& args,
                      std::chrono::seconds timeout) {
    return run_with_input(path, args, "", timeout);
}

}  // namespace minidbg::test
