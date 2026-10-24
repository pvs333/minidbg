# Milestone resources

There is one section per milestone. Each section has a summary of what changed, the
underlying concepts in plain language, and interview-style questions you should be able
to answer afterwards.

---

## Milestone 1: Skeleton

### (a) What changed

* **Build.** A CMake project builds `minidbg_core` (a static library holding all the logic),
  the `minidbg` executable (a thin `main.cpp`), and the vendored `linenoise` (compiled
  without `-Werror` because it's third-party code). First-party code builds with
  `-Wall -Wextra -Werror` through the `minidbg_warnings` interface target.
* **`src/process.{hpp,cpp}`, the `Process` class:**
  * `launch()` does fork, then `PTRACE_TRACEME`, then `execv`. It returns with the child
    stopped at the post-exec SIGTRAP.
  * `attach()` uses `PTRACE_ATTACH`.
  * `resume()` / `single_step()` resume the tracee, and `wait_on_signal()` waits for it to
    stop again.
  * `kill()` / `detach()` end the session.
  * The class is a RAII handle: its destructor kills a launched child, or detaches from an
    attached one.
* **`decode_wait_status()`** turns the `waitpid` status word into a `StopReason`: exited
  with a code, terminated by a signal, or stopped by a signal. For SIGTRAP stops it also
  classifies the trap from `siginfo.si_code`, which later milestones rely on.
* **`src/debugger.{hpp,cpp}`, the `Debugger` engine.** It owns the program description and
  the live process, and implements the "pass the signal on resume" policy.
* **`src/cli.{hpp,cpp}`:**
  * `CommandInterpreter` holds a table of commands (with aliases) and formats stop
    reports. Errors become `error: ...` lines, and the session keeps going.
  * `Repl` uses linenoise on a TTY and `std::getline` on a pipe, so tests can script it.
    The `-e/--echo` flag prints each command, which is handy for transcripts.
* **Commands:** `run [args]`, `starti`, `continue`, `kill`, `attach <pid>`, `detach`,
  `help [cmd]`, `quit`.
* **Tests (23):**
  * Unit tests for string parsing and for `Process`: launch/exit, exit codes, missing
    binary, SIGSEGV, the destructor reaping the child, wait-status decoding.
  * End-to-end tests that run the real binary with scripted stdin, using a harness with a
    timeout and process-group kill.

### (b) Concepts in plain language

**ptrace's model.** A *tracer* (minidbg) controls a *tracee* (your program). The tracee
runs normally until something "interesting" happens: it receives any signal, or it hits
a trap. The kernel then freezes it (a *ptrace-stop*) and notifies the tracer, which picks
the notification up with `waitpid`. While the tracee is frozen, the tracer may read and
write its registers and memory. Nothing runs again until the tracer says
`PTRACE_CONT` (run) or `PTRACE_SINGLESTEP` (run exactly one instruction).

**Launching under the debugger.** fork gives you two copies of the debugger. The child:

1. calls `ptrace(PTRACE_TRACEME)`, meaning "my parent is my tracer";
2. calls `execve` on the target.

A successful `execve` by a traced process makes the kernel stop it with SIGTRAP before the
new program executes a single instruction. Not even the dynamic loader (`ld.so`) has run
at that point. That is the moment `starti` shows you.

**The wait status word.** `waitpid` returns one `int` that encodes three cases. The
`WIFEXITED`/`WEXITSTATUS`, `WIFSIGNALED`/`WTERMSIG` and `WIFSTOPPED`/`WSTOPSIG` macros
decode them. Stopped means the process is still alive and can be resumed. The other two
mean it is gone, and the debugger must forget it.

**Who gets a signal?** When the tracee stops "because of a signal", that signal has not
been delivered yet. The tracer decides: `PTRACE_CONT` with `sig = 0` *swallows* it, and
`PTRACE_CONT` with `sig = SIGSEGV` *delivers* it. We follow gdb's policy:

* swallow SIGTRAP (it's ours), SIGINT (the user pressing Ctrl-C) and SIGSTOP (from attach);
* deliver everything else.

That's why the segfault test shows two reports: first "stopped by SIGSEGV", and then, on
`continue`, "terminated by SIGSEGV". It's also why the SIGUSR1 handler in `signals.c`
still runs.

> ⚠️ **Subtle: the exec error pipe.** After `fork`, how does the parent know whether
> `execve` succeeded? We create a pipe with `O_CLOEXEC` on both ends. A *successful*
> exec closes the child's copy of the write end automatically, so the parent's `read()`
> returns 0 (EOF). A *failed* exec lets the child write `execv: No such file or
> directory` into the pipe before calling `_exit(127)`. One `read()` loop distinguishes
> the two cases with no races and no timeouts.

> ⚠️ **Subtle: signal dispositions survive exec.** minidbg ignores SIGINT so that Ctrl-C
> doesn't kill the debugger. But `SIG_IGN` is *inherited across exec*. If the child
> didn't reset SIGINT to `SIG_DFL` before `execv`, your program would be immune to Ctrl-C.

> ⚠️ **Subtle: classifying SIGTRAP.** One signal, many meanings. `PTRACE_GETSIGINFO` gives
> the `si_code`:
>
> | `si_code` | Meaning |
> |---|---|
> | `SI_KERNEL` | an `int3` instruction (on x86) |
> | `TRAP_TRACE` | a finished single-step |
> | `TRAP_HWBKPT` | a debug-register hit |
> | `SI_USER` | the implicit post-exec trap, which the kernel sends as if by `kill()` |
>
> Milestone 3 (breakpoints) depends on getting this right.

**`PTRACE_O_EXITKILL`.** If the debugger crashes, the kernel SIGKILLs the tracee. Without
it, a tracee would be left frozen forever in a ptrace-stop.

**RAII for processes.** `std::unique_ptr<Process>` owns the child. Destroying it (quitting,
restarting with `run`, an exception unwinding the stack) always kills *and reaps* the
child, so no zombies and no orphaned stopped processes are left behind.

### (c) Interview questions

1. Walk through exactly what happens, syscall by syscall, from `run` until the program's
   `main` executes. Where is the first point at which the debugger has control?
2. A tracee stops with SIGSEGV and you call `PTRACE_CONT` with `sig = 0`. What happens,
   and why does gdb not do that by default?
3. How does minidbg distinguish "execve failed" from "execve succeeded and the child is
   stopped"? Why doesn't `waitpid` alone solve this?
4. Why must the child reset SIGINT to `SIG_DFL` before exec? Which property of signal
   dispositions makes this necessary?
5. What do `WIFEXITED`, `WIFSIGNALED` and `WIFSTOPPED` mean, and which of them leave a
   process you can still inspect? What does `PTRACE_O_EXITKILL` protect against?
