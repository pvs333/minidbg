#pragma once

#include <sys/types.h>

#include <memory>
#include <string>
#include <vector>

#include "process.hpp"

namespace minidbg {

// What happened when the inferior last stopped, in a form the CLI can print.
struct StopEvent {
    StopReason reason;
    pid_t pid = 0;
};

// The debugger "engine". It owns the target program description and, while one exists, the
// live Process. It knows nothing about text input or output formatting: the CLI layer turns
// commands into calls here and StopEvents into text.
class Debugger {
  public:
    Debugger(std::string program, std::vector<std::string> args);

    const std::string& program() const { return program_; }
    const std::vector<std::string>& args() const { return args_; }
    void set_args(std::vector<std::string> args) { args_ = std::move(args); }

    bool has_process() const { return process_ != nullptr; }
    pid_t pid() const;

    // Starts the program and stops before its first instruction ("starti").
    StopEvent start();
    // Starts the program and lets it run until something interesting happens.
    StopEvent run();
    StopEvent attach(pid_t pid);
    StopEvent continue_execution();
    void kill();
    void detach();

  private:
    Process& live_process();
    StopEvent resume_and_wait();
    StopEvent finish_stop(const StopReason& reason);

    std::string program_;
    std::vector<std::string> args_;
    std::unique_ptr<Process> process_;
    // A signal that stopped the inferior and should be delivered when it resumes.
    int pending_signal_ = 0;
};

}  // namespace minidbg
