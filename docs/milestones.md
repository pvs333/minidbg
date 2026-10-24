# Milestone plans

Each section below was written *before* the milestone was implemented. It records
which files I planned to touch, the key design decisions, and the edge cases I expected
to be tricky. The "what actually happened" write-ups live in `resource.md` and `NOTES.md`.

---

## Milestone 1: Skeleton

**Goal:** CMake project, REPL, launch a child under ptrace, continue until it exits,
and report the exit status and any signals.

**Files**

| File | Purpose |
|---|---|
| `CMakeLists.txt`, `tests/CMakeLists.txt` | Build `minidbg_core` (static lib), the `minidbg` binary, the vendored `linenoise`, test programs, and GoogleTest suites |
| `src/error.hpp` | `minidbg::Error` exception type plus a `throw_errno` helper |
| `src/fd.hpp` | `FileDescriptor`: a move-only RAII wrapper that `close()`s on destruction |
| `src/strings.{hpp,cpp}` | Tokenizing and integer parsing for the CLI |
| `src/process.{hpp,cpp}` | `Process`: fork + `PTRACE_TRACEME` + `execve`, wait-status decoding, resume, kill |
| `src/debugger.{hpp,cpp}` | `Debugger`: owns the target program and the (optional) live process |
| `src/cli.{hpp,cpp}` | `CommandInterpreter` (parses and dispatches commands, formats output) and `Repl` (input loop) |
| `src/main.cpp` | Argument parsing |
| `tests/support/` | A helper that runs the `minidbg` binary with scripted stdin and captures its output, with a timeout |

**Key design decisions**

* **Library + thin `main`.** All logic lives in `minidbg_core` so unit tests can link it.
* **Errors are exceptions.** Every failing syscall becomes a `minidbg::Error` with
  `strerror(errno)` in the message. The REPL catches it and prints `error: ...`, and the
  debugger keeps running. Nothing fails silently.
* **`Process` is a RAII process handle.** Destroying a launched process kills and reaps
  it. Destroying an attached one detaches. Copying is forbidden.
* **Exec failures are reported through a pipe with `O_CLOEXEC`.** If `execve` succeeds, the
  kernel closes the pipe, so the parent's `read()` returns 0. If it fails, the child writes
  the error text into the pipe. The parent can then tell "launched and stopped at exec"
  apart from "the path did not exist".
* **`PTRACE_O_EXITKILL`** means that if minidbg itself crashes, the kernel kills the tracee
  instead of leaving an orphan stopped forever.
* **ASLR is disabled in the child with `personality(ADDR_NO_RANDOMIZE)`**, as gdb does, so
  addresses are reproducible between runs. A failure here is tolerated (Docker's seccomp
  profile blocks it), and no test depends on fixed addresses.
* **Signal policy (copied from gdb):** a stop caused by a signal other than SIGTRAP or
  SIGINT is reported, and the signal is *delivered* on the next resume. That way a SIGSEGV
  still kills the program and a SIGUSR1 handler still runs.
* **Commands:** `run [args]`, `starti`, `continue`, `kill`, `help`, `quit`.

**Tricky edge cases I expect**

* `waitpid` can return `EINTR`, so it has to be retried in a loop.
* The child must reset SIGINT to `SIG_DFL` before `execve`, because the debugger ignores
  SIGINT and ignored dispositions survive `exec`.
* stdout buffering: the debugger has to flush its own output before it resumes the child,
  otherwise the transcript interleaves confusingly when stdout is a pipe.
