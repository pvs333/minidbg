#pragma once

#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <string>

namespace minidbg {

// The single exception type used throughout minidbg. Every recoverable failure (bad user
// input, failed syscall, malformed ELF/DWARF) is reported by throwing one of these; the REPL
// catches it, prints the message and keeps going. Nothing fails silently.
class Error : public std::runtime_error {
  public:
    using std::runtime_error::runtime_error;
};

[[noreturn]] inline void throw_error(const std::string& message) {
    throw Error(message);
}

// Throws an Error whose message is "<what>: <strerror(errno)>". Call it immediately after the
// failing syscall, before anything else can clobber errno.
[[noreturn]] inline void throw_errno(const std::string& what) {
    throw Error(what + ": " + std::strerror(errno));
}

}  // namespace minidbg
