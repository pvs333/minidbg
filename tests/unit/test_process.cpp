#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>

#include <gtest/gtest.h>

#include "error.hpp"
#include "fd.hpp"
#include "process.hpp"
#include "test_support.hpp"

using namespace minidbg;

namespace {

// Launches with the inferior's stdout sent to /dev/null to keep test output clean.
std::unique_ptr<Process> launch_quiet(const std::string& path, std::vector<std::string> args = {}) {
    FileDescriptor devnull(::open("/dev/null", O_WRONLY | O_CLOEXEC));
    LaunchOptions options;
    options.stdout_fd = devnull.get();
    return Process::launch(path, args, options);
}

}  // namespace

TEST(Process, LaunchStopsAtExecThenRunsToExit) {
    auto process = launch_quiet(test::program("hello"));
    EXPECT_EQ(process->state(), ProcessState::Stopped);
    EXPECT_GT(process->pid(), 0);

    process->resume();
    StopReason reason = process->wait_on_signal();
    EXPECT_EQ(reason.state, ProcessState::Exited);
    EXPECT_EQ(reason.info, 0);
    EXPECT_FALSE(process->is_alive());
}

TEST(Process, ReportsNonZeroExitStatus) {
    auto process = launch_quiet(test::program("exit_code"), {"7"});
    process->resume();
    StopReason reason = process->wait_on_signal();
    EXPECT_EQ(reason.state, ProcessState::Exited);
    EXPECT_EQ(reason.info, 7);
}

TEST(Process, LaunchingMissingProgramThrows) {
    EXPECT_THROW(launch_quiet("/nonexistent/program"), Error);
}

TEST(Process, SegfaultStopsThenTerminatesWhenSignalDelivered) {
    auto process = launch_quiet(test::program("segfault"));
    process->resume();
    StopReason stop = process->wait_on_signal();
    ASSERT_EQ(stop.state, ProcessState::Stopped);
    EXPECT_EQ(stop.info, SIGSEGV);

    process->resume(SIGSEGV);
    StopReason end = process->wait_on_signal();
    EXPECT_EQ(end.state, ProcessState::Terminated);
    EXPECT_EQ(end.info, SIGSEGV);
}

TEST(Process, DestructorKillsLaunchedProcess) {
    pid_t pid;
    {
        auto process = launch_quiet(test::program("hello"));
        pid = process->pid();
    }
    // The child has been killed *and reaped*, so it no longer exists at all.
    EXPECT_EQ(::kill(pid, 0), -1);
    EXPECT_EQ(errno, ESRCH);
}

TEST(Process, DecodeWaitStatus) {
    StopReason exited = decode_wait_status(3 << 8);  // what WEXITSTATUS decodes as 3
    EXPECT_EQ(exited.state, ProcessState::Exited);
    EXPECT_EQ(exited.info, 3);

    StopReason killed = decode_wait_status(SIGKILL);
    EXPECT_EQ(killed.state, ProcessState::Terminated);
    EXPECT_EQ(killed.info, SIGKILL);

    StopReason stopped = decode_wait_status((SIGTRAP << 8) | 0x7f);
    EXPECT_EQ(stopped.state, ProcessState::Stopped);
    EXPECT_EQ(stopped.info, SIGTRAP);
}

TEST(Process, SignalNames) {
    EXPECT_EQ(signal_name(SIGSEGV), "SIGSEGV");
    EXPECT_EQ(signal_description(SIGSEGV), "Segmentation fault");
}
