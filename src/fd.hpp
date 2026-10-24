#pragma once

#include <unistd.h>

#include <utility>

namespace minidbg {

// Move-only owner of a POSIX file descriptor. The descriptor is closed when the owner is
// destroyed, so early returns and exceptions can never leak it.
class FileDescriptor {
  public:
    FileDescriptor() = default;
    explicit FileDescriptor(int fd) : fd_(fd) {}
    ~FileDescriptor() { reset(); }

    FileDescriptor(const FileDescriptor&) = delete;
    FileDescriptor& operator=(const FileDescriptor&) = delete;

    FileDescriptor(FileDescriptor&& other) noexcept : fd_(std::exchange(other.fd_, -1)) {}
    FileDescriptor& operator=(FileDescriptor&& other) noexcept {
        if (this != &other) {
            reset();
            fd_ = std::exchange(other.fd_, -1);
        }
        return *this;
    }

    int get() const { return fd_; }
    bool valid() const { return fd_ >= 0; }

    // Closes the current descriptor (if any) and optionally adopts a new one.
    void reset(int fd = -1) {
        if (fd_ >= 0) {
            ::close(fd_);
        }
        fd_ = fd;
    }

  private:
    int fd_ = -1;
};

// Both ends of a pipe(2), each owned by a FileDescriptor.
struct Pipe {
    FileDescriptor read_end;
    FileDescriptor write_end;
};

}  // namespace minidbg
