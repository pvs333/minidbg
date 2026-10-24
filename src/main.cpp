#include <signal.h>
#include <unistd.h>

#include <iostream>
#include <string>
#include <string_view>
#include <vector>

#include "cli.hpp"
#include "debugger.hpp"
#include "error.hpp"
#include "strings.hpp"

namespace {

void print_usage(std::ostream& out) {
    out << "usage: minidbg [options] <program> [args...]\n"
           "       minidbg [options] -p <pid>\n"
           "\n"
           "options:\n"
           "  -p, --pid <pid>  attach to a running process\n"
           "  -e, --echo       echo each command when reading from a pipe (for transcripts)\n"
           "  -h, --help       show this help\n";
}

struct CommandLine {
    std::string program;
    std::vector<std::string> args;
    pid_t attach_pid = 0;
    bool echo = false;
};

// Options must come before the program name; everything after it belongs to the inferior.
CommandLine parse_command_line(int argc, char** argv) {
    CommandLine cl;
    int i = 1;
    for (; i < argc; ++i) {
        std::string_view arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            print_usage(std::cout);
            std::exit(0);
        } else if (arg == "-e" || arg == "--echo") {
            cl.echo = true;
        } else if (arg == "-p" || arg == "--pid") {
            if (i + 1 >= argc) {
                minidbg::throw_error("-p requires a pid");
            }
            auto pid = minidbg::parse_uint(argv[++i]);
            if (!pid || *pid == 0 || *pid > static_cast<std::uint64_t>(INT32_MAX)) {
                minidbg::throw_error(std::string("invalid pid '") + argv[i] + "'");
            }
            cl.attach_pid = static_cast<pid_t>(*pid);
        } else if (arg == "--") {
            ++i;
            break;
        } else if (!arg.empty() && arg[0] == '-') {
            minidbg::throw_error("unknown option '" + std::string(arg) + "'");
        } else {
            break;
        }
    }
    if (i < argc) {
        cl.program = argv[i++];
        cl.args.assign(argv + i, argv + argc);
    }
    if (cl.program.empty() && cl.attach_pid == 0) {
        minidbg::throw_error("no program given");
    }
    return cl;
}

}  // namespace

int main(int argc, char** argv) {
    CommandLine cl;
    try {
        cl = parse_command_line(argc, argv);
    } catch (const minidbg::Error& e) {
        std::cerr << "minidbg: " << e.what() << "\n\n";
        print_usage(std::cerr);
        return 2;
    }

    // Ctrl-C should interrupt the inferior, not kill the debugger. The terminal delivers
    // SIGINT to the whole foreground process group; the tracee stops with it and we report.
    ::signal(SIGINT, SIG_IGN);

    minidbg::Debugger debugger(cl.program, cl.args);
    minidbg::CommandInterpreter interpreter(debugger, std::cout);

    if (cl.attach_pid != 0) {
        interpreter.execute("attach " + std::to_string(cl.attach_pid));
    }

    minidbg::Repl::Options options;
    options.interactive = ::isatty(STDIN_FILENO) != 0;
    options.echo = cl.echo;
    minidbg::Repl repl(interpreter, std::cin, std::cout, options);
    repl.run();
    return 0;
}
