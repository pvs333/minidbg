#pragma once

#include <sys/types.h>

#include <memory>
#include <string>
#include <vector>

namespace minidbg {

enum class ProcessState {
    Stopped,     // ptrace-stopped; we may inspect and modify it
    Running,     // resumed, waiting for the next stop
    Exited,      // called exit(); `info` holds the exit status
    Terminated,  // killed by a signal; `info` holds the signal number
    Detached,    // we let go of it with PTRACE_DETACH; it keeps running untraced
};

// Why a SIGTRAP stop happened. The kernel tells us via siginfo.si_code.
enum class TrapType {
    None,           // the stop was not a SIGTRAP
    SoftwareBreak,  // executed an int3 (si_code == SI_KERNEL on x86)
    SingleStep,     // PTRACE_SINGLESTEP finished one instruction (TRAP_TRACE)
    HardwareBreak,  // a debug-register watchpoint fired (TRAP_HWBKPT)
    Exec,           // the stop right after a successful execve
    Unknown,
};

struct StopReason {
    ProcessState state = ProcessState::Stopped;
    int info = 0;  // exit code (Exited) or signal number (Stopped/Terminated)
    TrapType trap = TrapType::None;

    bool is_alive() const { return state == ProcessState::Stopped; }
    bool is_trap() const { return state == ProcessState::Stopped && trap != TrapType::None; }
};

// Decodes a waitpid() status word.
StopReason decode_wait_status(int status);

// "SIGSEGV" for 11, "SIG42" for unnamed real-time signals.
std::string signal_name(int signal);
// "Segmentation fault".
std::string signal_description(int signal);

struct LaunchOptions {
    bool disable_aslr = true;
    // If >= 0, the child's stdout is redirected to this descriptor (used by tests).
    int stdout_fd = -1;
};

// A traced process. This is a RAII handle: destroying a process we launched kills and reaps
// it; destroying one we attached to detaches from it so it keeps running.
class Process {
  public:
    // fork + PTRACE_TRACEME + execve. Returns with the child stopped at the post-exec SIGTRAP,
    // before a single instruction of the program (or its dynamic loader) has run.
    static std::unique_ptr<Process> launch(const std::string& path,
                                           const std::vector<std::string>& args,
                                           const LaunchOptions& options = {});
    // PTRACE_ATTACH to a running process and wait for it to stop.
    static std::unique_ptr<Process> attach(pid_t pid);

    ~Process();
    Process(const Process&) = delete;
    Process& operator=(const Process&) = delete;
    Process(Process&&) = delete;
    Process& operator=(Process&&) = delete;

    pid_t pid() const { return pid_; }
    ProcessState state() const { return state_; }
    bool is_alive() const {
        return state_ == ProcessState::Stopped || state_ == ProcessState::Running;
    }
    bool was_attached() const { return attached_; }

    // PTRACE_CONT, optionally delivering `signal` to the tracee. Does not wait.
    void resume(int signal = 0);
    // PTRACE_SINGLESTEP, optionally delivering `signal`. Does not wait.
    void single_step(int signal = 0);
    // Blocks until the tracee changes state.
    StopReason wait_on_signal();

    // SIGKILL + reap. Safe to call on a dead process.
    void kill();
    // PTRACE_DETACH; the process continues running untraced.
    void detach();

  private:
    Process(pid_t pid, bool attached) : pid_(pid), attached_(attached) {}
    TrapType classify_trap();

    pid_t pid_;
    bool attached_;
    ProcessState state_ = ProcessState::Stopped;
};

}  // namespace minidbg
