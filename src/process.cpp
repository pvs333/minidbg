#include "process.hpp"

#include <fcntl.h>
#include <signal.h>
#include <sys/personality.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cstring>
#include <string>

#include "error.hpp"
#include "fd.hpp"

namespace minidbg {

namespace {

Pipe make_cloexec_pipe() {
    int fds[2];
    if (::pipe2(fds, O_CLOEXEC) < 0) {
        throw_errno("pipe2");
    }
    return Pipe{FileDescriptor(fds[0]), FileDescriptor(fds[1])};
}

// Reports `what: strerror(errno)` to the parent through the launch pipe and exits. Runs in the
// forked child, so it sticks to async-signal-safe calls (write, _exit).
[[noreturn]] void report_child_error_and_exit(int error_fd, const char* what) {
    const char* reason = std::strerror(errno);
    // Best-effort writes: if they fail there is nobody left to tell.
    [[maybe_unused]] auto r1 = ::write(error_fd, what, std::strlen(what));
    [[maybe_unused]] auto r2 = ::write(error_fd, ": ", 2);
    [[maybe_unused]] auto r3 = ::write(error_fd, reason, std::strlen(reason));
    ::_exit(127);
}

// Runs in the forked child: set up tracing and become the target program.
[[noreturn]] void exec_child(const std::string& path, const std::vector<std::string>& args,
                             const LaunchOptions& options, int error_fd) {
    if (options.disable_aslr) {
        // Like gdb: makes addresses reproducible between runs. Docker's default seccomp
        // profile rejects this, so failure is deliberately tolerated.
        ::personality(ADDR_NO_RANDOMIZE);
    }
    // The debugger ignores SIGINT (so Ctrl-C stops only the inferior). Ignored signal
    // dispositions survive execve, so restore the default for the child.
    ::signal(SIGINT, SIG_DFL);

    if (options.stdout_fd >= 0 && ::dup2(options.stdout_fd, STDOUT_FILENO) < 0) {
        report_child_error_and_exit(error_fd, "dup2");
    }
    if (::ptrace(PTRACE_TRACEME, 0, nullptr, nullptr) < 0) {
        report_child_error_and_exit(error_fd, "ptrace(PTRACE_TRACEME)");
    }

    std::vector<char*> argv;
    argv.push_back(const_cast<char*>(path.c_str()));
    for (const auto& arg : args) {
        argv.push_back(const_cast<char*>(arg.c_str()));
    }
    argv.push_back(nullptr);
    ::execv(path.c_str(), argv.data());
    report_child_error_and_exit(error_fd, "execv");
}

}  // namespace

StopReason decode_wait_status(int status) {
    StopReason reason;
    if (WIFEXITED(status)) {
        reason.state = ProcessState::Exited;
        reason.info = WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
        reason.state = ProcessState::Terminated;
        reason.info = WTERMSIG(status);
    } else if (WIFSTOPPED(status)) {
        reason.state = ProcessState::Stopped;
        reason.info = WSTOPSIG(status);
    } else {
        throw_error("unexpected wait status " + std::to_string(status));
    }
    return reason;
}

std::string signal_name(int signal) {
    if (const char* abbrev = ::sigabbrev_np(signal)) {
        return std::string("SIG") + abbrev;
    }
    return "SIG" + std::to_string(signal);
}

std::string signal_description(int signal) {
    if (const char* description = ::sigdescr_np(signal)) {
        return description;
    }
    return "Unknown signal " + std::to_string(signal);
}

std::unique_ptr<Process> Process::launch(const std::string& path,
                                         const std::vector<std::string>& args,
                                         const LaunchOptions& options) {
    // The child reports exec failures through this pipe. Because both ends are O_CLOEXEC,
    // a *successful* execve closes the child's write end and our read() returns 0.
    Pipe channel = make_cloexec_pipe();

    pid_t pid = ::fork();
    if (pid < 0) {
        throw_errno("fork");
    }
    if (pid == 0) {
        channel.read_end.reset();
        exec_child(path, args, options, channel.write_end.get());
    }

    channel.write_end.reset();
    std::string child_error;
    char buffer[256];
    ssize_t n;
    while ((n = ::read(channel.read_end.get(), buffer, sizeof(buffer))) != 0) {
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            throw_errno("read from launch pipe");
        }
        child_error.append(buffer, static_cast<std::size_t>(n));
    }

    if (!child_error.empty()) {
        int status;
        ::waitpid(pid, &status, 0);
        throw_error("cannot launch '" + path + "': " + child_error);
    }

    std::unique_ptr<Process> process(new Process(pid, /*attached=*/false));
    StopReason reason = process->wait_on_signal();
    if (reason.state != ProcessState::Stopped || reason.info != SIGTRAP) {
        throw_error("'" + path + "' did not stop at exec as expected");
    }
    // If minidbg dies unexpectedly, the kernel kills the tracee instead of leaving it
    // stopped forever.
    if (::ptrace(PTRACE_SETOPTIONS, pid, nullptr, PTRACE_O_EXITKILL) < 0) {
        throw_errno("ptrace(PTRACE_SETOPTIONS)");
    }
    return process;
}

std::unique_ptr<Process> Process::attach(pid_t pid) {
    if (pid <= 0) {
        throw_error("invalid pid " + std::to_string(pid));
    }
    if (::ptrace(PTRACE_ATTACH, pid, nullptr, nullptr) < 0) {
        throw_errno("ptrace(PTRACE_ATTACH, " + std::to_string(pid) + ")");
    }
    std::unique_ptr<Process> process(new Process(pid, /*attached=*/true));
    // PTRACE_ATTACH sends SIGSTOP; wait for that stop. The process may also report another
    // signal first, which is fine: any stop means we are in control.
    StopReason reason = process->wait_on_signal();
    if (!reason.is_alive()) {
        throw_error("process " + std::to_string(pid) + " exited during attach");
    }
    return process;
}

Process::~Process() {
    if (!is_alive()) {
        return;
    }
    // Destructors must not throw; the process is going away either way.
    try {
        if (attached_) {
            detach();
        } else {
            kill();
        }
    } catch (const Error&) {
    }
}

void Process::resume(int signal) {
    if (::ptrace(PTRACE_CONT, pid_, nullptr, signal) < 0) {
        throw_errno("ptrace(PTRACE_CONT)");
    }
    state_ = ProcessState::Running;
}

void Process::single_step(int signal) {
    if (::ptrace(PTRACE_SINGLESTEP, pid_, nullptr, signal) < 0) {
        throw_errno("ptrace(PTRACE_SINGLESTEP)");
    }
    state_ = ProcessState::Running;
}

StopReason Process::wait_on_signal() {
    int status = 0;
    while (::waitpid(pid_, &status, 0) < 0) {
        if (errno != EINTR) {
            throw_errno("waitpid");
        }
    }
    StopReason reason = decode_wait_status(status);
    state_ = reason.state;
    if (reason.state == ProcessState::Stopped && reason.info == SIGTRAP) {
        reason.trap = classify_trap();
    }
    return reason;
}

TrapType Process::classify_trap() {
    siginfo_t info;
    if (::ptrace(PTRACE_GETSIGINFO, pid_, nullptr, &info) < 0) {
        throw_errno("ptrace(PTRACE_GETSIGINFO)");
    }
    switch (info.si_code) {
        // On x86 an int3 is delivered with si_code SI_KERNEL; TRAP_BRKPT is accepted too.
        case SI_KERNEL:
        case TRAP_BRKPT:
            return TrapType::SoftwareBreak;
        case TRAP_TRACE:
            return TrapType::SingleStep;
        case TRAP_HWBKPT:
            return TrapType::HardwareBreak;
        // The implicit SIGTRAP after execve (without PTRACE_O_TRACEEXEC) is sent as if by
        // kill(), i.e. SI_USER.
        case SI_USER:
            return TrapType::Exec;
        default:
            return TrapType::Unknown;
    }
}

void Process::kill() {
    if (!is_alive()) {
        return;
    }
    if (::kill(pid_, SIGKILL) < 0) {
        throw_errno("kill");
    }
    // Reap every remaining state change until the process is really gone.
    for (;;) {
        int status = 0;
        if (::waitpid(pid_, &status, 0) < 0) {
            if (errno == EINTR) {
                continue;
            }
            break;  // ECHILD: already reaped
        }
        if (WIFEXITED(status) || WIFSIGNALED(status)) {
            break;
        }
    }
    state_ = ProcessState::Terminated;
}

void Process::detach() {
    if (!is_alive()) {
        return;
    }
    if (::ptrace(PTRACE_DETACH, pid_, nullptr, nullptr) < 0) {
        throw_errno("ptrace(PTRACE_DETACH)");
    }
    state_ = ProcessState::Detached;
}

}  // namespace minidbg
