// End-to-end tests for milestone 1: launching, running to exit, signals.
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "test_support.hpp"

using namespace minidbg::test;
using ::testing::HasSubstr;

TEST(E2EProcess, RunToNormalExit) {
    auto result = run_minidbg({program("hello")}, {"run"});
    ASSERT_FALSE(result.timed_out) << result.output;
    EXPECT_THAT(result.output, HasSubstr("Starting program: "));
    EXPECT_THAT(result.output, HasSubstr("hello from the inferior"));
    EXPECT_THAT(result.output, HasSubstr("exited with status 0"));
    EXPECT_EQ(result.exit_status, 0);
}

TEST(E2EProcess, ReportsExitCodeAndPassesArguments) {
    auto result = run_minidbg({program("exit_code"), "5"}, {"run"});
    EXPECT_THAT(result.output, HasSubstr("exited with status 5"));

    // "run <args>" replaces the arguments given on the command line.
    result = run_minidbg({program("exit_code"), "5"}, {"run 9"});
    EXPECT_THAT(result.output, HasSubstr("exited with status 9"));
}

TEST(E2EProcess, SegfaultIsReportedThenDeliveredOnContinue) {
    auto result = run_minidbg({program("segfault")}, {"run", "continue"});
    EXPECT_THAT(result.output, HasSubstr("stopped by signal SIGSEGV (Segmentation fault)"));
    EXPECT_THAT(result.output, HasSubstr("terminated by signal SIGSEGV"));
}

TEST(E2EProcess, SignalIsPassedToProgramHandler) {
    auto result = run_minidbg({program("signals")}, {"run", "continue"});
    EXPECT_THAT(result.output, HasSubstr("stopped by signal SIGUSR1"));
    EXPECT_THAT(result.output, HasSubstr("handler ran: yes"));
    EXPECT_THAT(result.output, HasSubstr("exited with status 0"));
}

TEST(E2EProcess, StartiStopsBeforeFirstInstruction) {
    auto result = run_minidbg({program("hello")}, {"starti", "continue"});
    EXPECT_THAT(result.output, HasSubstr("stopped at program start"));
    EXPECT_THAT(result.output, HasSubstr("exited with status 0"));
}

TEST(E2EProcess, ErrorsAreReportedAndSessionContinues) {
    auto result = run_minidbg({program("hello")}, {"continue", "bogus", "run"});
    EXPECT_THAT(result.output, HasSubstr("error: the program is not being run"));
    EXPECT_THAT(result.output, HasSubstr("error: unknown command 'bogus'"));
    EXPECT_THAT(result.output, HasSubstr("exited with status 0"));
}

TEST(E2EProcess, QuitKillsRunningProgram) {
    auto result = run_minidbg({program("hello")}, {"starti", "quit"});
    EXPECT_FALSE(result.timed_out);
    EXPECT_EQ(result.exit_status, 0);
    EXPECT_EQ(result.output.find("hello from the inferior"), std::string::npos);
}

TEST(E2EProcess, MissingProgramIsAnError) {
    auto result = run_minidbg({"/nonexistent/prog"}, {"run"});
    EXPECT_THAT(result.output, HasSubstr("error: cannot launch '/nonexistent/prog'"));
}

TEST(E2EProcess, HelpListsCommands) {
    auto result = run_minidbg({program("hello")}, {"help", "help continue"});
    EXPECT_THAT(result.output, HasSubstr("run [args...]"));
    EXPECT_THAT(result.output, HasSubstr("aliases: c cont"));
}
