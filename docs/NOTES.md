# Learning journal

This is a running log of concepts, design decisions, and the bugs we hit, with the reason
behind each one. Read it top to bottom to study the project.

---

## Milestone 1: Skeleton

### Concepts

* **Tracer/tracee.** The tracee only runs between our `PTRACE_CONT`/`PTRACE_SINGLESTEP`
  call and the next ptrace-stop, which we collect with `waitpid`. The debugger is
  completely *synchronous*: every command that resumes the inferior immediately waits for
  it to stop again. That's why there are no threads, no async state and no locks anywhere.
* **The post-exec stop happens before `ld.so`.** At the first stop, the kernel has mapped
  the executable and the dynamic loader. `rip` is at the *loader's* entry point, not at
  the program's `_start`, and `libc` isn't loaded yet. That matters for milestone 4 (only
  the main executable's symbols exist at this point) and milestone 2 (`[rsp]` holds
  `argc`).
* **Wait status encoding.** It's a single int:
  * low 7 bits `0`: exited, and the code is in bits 8–15;
  * low byte `0x7f`: stopped, and the signal is in bits 8–15;
  * anything else: killed by the signal in the low 7 bits.

  `test_process.cpp` checks those hand-built values.

### Design decisions

* **Library + thin main.** All the logic is in `minidbg_core`. `main.cpp` only parses the
  argv and wires `Debugger`, `CommandInterpreter` and `Repl` together.
* **Three layers.** Each layer is testable on its own:
  * `Process`: raw ptrace mechanics.
  * `Debugger`: debugger policy (signal passing; later, breakpoints and stepping). It
    returns plain structs such as `StopEvent`.
  * `CommandInterpreter`: text in, text out.
* **The command table is data.** Each entry is names + usage + help + handler. `help` is
  generated from it, so the help text can't drift from the real commands. The table is a
  member of the interpreter, not a global.
* **Errors are exceptions.** A single `minidbg::Error` type. `throw_errno()` attaches
  `strerror(errno)`. The interpreter catches `Error` around each command, so a typo or a
  failed syscall prints `error: ...` and the session survives.
* **ASLR is disabled like gdb**, via `personality(ADDR_NO_RANDOMIZE)` in the child. A
  failure is tolerated because Docker's default seccomp profile blocks it. The tests
  never hard-code addresses, so they pass either way.
* **The e2e harness** runs `minidbg` in its own process group. On timeout it kills `-pgid`,
  so a hung inferior can't outlive the test. It collects output until EOF on the pipe,
  i.e. until *every* writer (the debugger and the inferior) has exited.

### Bugs and gotchas hit

1. **`'noreturn' function does return`.** The child's error reporter was originally a
   lambda that called `_exit`. The compiler can't see that a lambda never returns, so it
   flagged `exec_child` (declared `[[noreturn]]`) as possibly returning. With `-Werror`
   that broke the build. Fix: make it a named `[[noreturn]]` free function. Lesson:
   `[[noreturn]]` must be visible on *every* path's last call.
2. **Reading an uninitialized wait status.** The first version of `Process::kill()` looped
   on `while (waitpid(...) >= 0 || errno == EINTR)` and then inspected `status`. On
   `EINTR`, `waitpid` doesn't write `status`, so the loop read garbage. Fix: initialize
   `status` inside the loop and `continue` on `EINTR` before looking at it.
3. **"Detached" is not "Exited".** After `PTRACE_DETACH`, the first version marked the
   process `Exited`, which was a lie that would confuse any future code that reports
   state. Fix: added `ProcessState::Detached`.
4. **Output ordering through pipes.** When stdout is a pipe, `std::cout` is fully
   buffered, and the inferior writes to the same pipe. Without flushing before resuming,
   "Starting program:" could appear *after* the program's own output. Fix: the
   interpreter flushes before resuming (`std::endl` on the start banner) and after every
   command.
