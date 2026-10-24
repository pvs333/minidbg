#pragma once

#include <functional>
#include <iosfwd>
#include <string>
#include <string_view>
#include <vector>

#include "debugger.hpp"

namespace minidbg {

// Parses one command line at a time, dispatches it to the Debugger and formats the results.
// Holds no input-handling logic, so it can be driven by the REPL or directly by tests.
class CommandInterpreter {
  public:
    CommandInterpreter(Debugger& debugger, std::ostream& out);

    // Executes one line. Returns false when the user asked to quit. Errors are caught and
    // printed as "error: ..." so a bad command never kills the session.
    bool execute(std::string_view line);

  private:
    using Args = std::vector<std::string>;
    struct Command {
        std::vector<std::string> names;  // first is canonical, the rest are aliases
        std::string usage;
        std::string help;
        std::function<void(const Args&)> handler;
    };

    void register_commands();
    const Command* find_command(std::string_view name) const;

    void cmd_help(const Args& args);
    void cmd_run(const Args& args);
    void cmd_starti(const Args& args);
    void cmd_continue(const Args& args);
    void cmd_kill(const Args& args);
    void cmd_attach(const Args& args);
    void cmd_detach(const Args& args);
    void cmd_quit(const Args& args);

    void report_stop(const StopEvent& event);

    Debugger& debugger_;
    std::ostream& out_;
    std::vector<Command> commands_;
    bool quit_requested_ = false;
};

// The read-eval-print loop. Uses linenoise (history, line editing) when stdin is a terminal and
// plain std::getline otherwise, so scripts can pipe commands in.
class Repl {
  public:
    struct Options {
        bool interactive = true;  // stdin is a TTY: show a prompt and use linenoise
        bool echo = false;        // non-interactive: print "(minidbg) <cmd>" before each command
    };

    Repl(CommandInterpreter& interpreter, std::istream& in, std::ostream& out, Options options);
    void run();

  private:
    bool read_line(std::string& line);

    CommandInterpreter& interpreter_;
    std::istream& in_;
    std::ostream& out_;
    Options options_;
};

}  // namespace minidbg
