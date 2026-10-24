#include "debugger.hpp"

#include <signal.h>

#include <utility>

#include "error.hpp"

namespace minidbg {

Debugger::Debugger(std::string program, std::vector<std::string> args)
    : program_(std::move(program)), args_(std::move(args)) {}

pid_t Debugger::pid() const {
    return process_ ? process_->pid() : 0;
}

Process& Debugger::live_process() {
    if (!process_) {
        throw_error("the program is not being run");
    }
    return *process_;
}

StopEvent Debugger::start() {
    if (program_.empty()) {
        throw_error("no program to run (pass one on the command line)");
    }
    process_.reset();  // kills any previous instance
    pending_signal_ = 0;
    process_ = Process::launch(program_, args_);
    StopReason reason;
    reason.state = ProcessState::Stopped;
    reason.info = SIGTRAP;
    reason.trap = TrapType::Exec;
    return finish_stop(reason);
}

StopEvent Debugger::run() {
    start();
    return continue_execution();
}

StopEvent Debugger::attach(pid_t pid) {
    process_.reset();
    pending_signal_ = 0;
    process_ = Process::attach(pid);
    StopReason reason;
    reason.state = ProcessState::Stopped;
    reason.info = SIGSTOP;
    return finish_stop(reason);
}

StopEvent Debugger::continue_execution() {
    return resume_and_wait();
}

void Debugger::kill() {
    live_process().kill();
    process_.reset();
}

void Debugger::detach() {
    live_process().detach();
    process_.reset();
}

StopEvent Debugger::resume_and_wait() {
    Process& process = live_process();
    process.resume(std::exchange(pending_signal_, 0));
    return finish_stop(process.wait_on_signal());
}

StopEvent Debugger::finish_stop(const StopReason& reason) {
    StopEvent event;
    event.reason = reason;
    event.pid = process_->pid();
    if (reason.state == ProcessState::Stopped) {
        // gdb's default policy: SIGTRAP belongs to the debugger and SIGINT is the user
        // interrupting; every other signal is handed to the program when it resumes.
        if (reason.info != SIGTRAP && reason.info != SIGINT && reason.info != SIGSTOP) {
            pending_signal_ = reason.info;
        }
    } else {
        // Exited or killed: nothing left to control.
        process_.reset();
    }
    return event;
}

}  // namespace minidbg
