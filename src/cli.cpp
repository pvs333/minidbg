#include "cli.hpp"

#include <iomanip>
#include <iostream>

#include "error.hpp"
#include "linenoise.h"
#include "strings.hpp"

namespace minidbg {

namespace {

constexpr const char* kPrompt = "(minidbg) ";

std::string join(const std::vector<std::string>& parts, std::size_t first = 0) {
    std::string out;
    for (std::size_t i = first; i < parts.size(); ++i) {
        if (i > first) {
            out += ' ';
        }
        out += parts[i];
    }
    return out;
}

void require_no_args(const std::vector<std::string>& args) {
    if (args.size() > 1) {
        throw_error("'" + args[0] + "' takes no arguments");
    }
}

}  // namespace

CommandInterpreter::CommandInterpreter(Debugger& debugger, std::ostream& out)
    : debugger_(debugger), out_(out) {
    register_commands();
}

void CommandInterpreter::register_commands() {
    auto add = [this](std::vector<std::string> names, std::string usage, std::string help,
                      void (CommandInterpreter::*method)(const Args&)) {
        commands_.push_back(Command{std::move(names), std::move(usage), std::move(help),
                                    [this, method](const Args& args) { (this->*method)(args); }});
    };
    add({"help", "h"}, "help [command]", "List commands, or describe one command.",
        &CommandInterpreter::cmd_help);
    add({"run", "r"}, "run [args...]",
        "Start the program (restarting it if needed) and run until it stops.",
        &CommandInterpreter::cmd_run);
    add({"starti"}, "starti [args...]", "Start the program and stop before its first instruction.",
        &CommandInterpreter::cmd_starti);
    add({"continue", "c", "cont"}, "continue", "Resume the program until it stops again.",
        &CommandInterpreter::cmd_continue);
    add({"kill", "k"}, "kill", "Kill the running program.", &CommandInterpreter::cmd_kill);
    add({"attach"}, "attach <pid>", "Attach to a running process.",
        &CommandInterpreter::cmd_attach);
    add({"detach"}, "detach", "Detach from the process and let it run freely.",
        &CommandInterpreter::cmd_detach);
    add({"quit", "q", "exit"}, "quit", "Exit minidbg (kills a launched program).",
        &CommandInterpreter::cmd_quit);
}

const CommandInterpreter::Command* CommandInterpreter::find_command(std::string_view name) const {
    for (const auto& command : commands_) {
        for (const auto& alias : command.names) {
            if (alias == name) {
                return &command;
            }
        }
    }
    return nullptr;
}

bool CommandInterpreter::execute(std::string_view line) {
    Args args = split_whitespace(line);
    if (args.empty()) {
        return true;
    }
    const Command* command = find_command(args[0]);
    try {
        if (!command) {
            throw_error("unknown command '" + args[0] + "' (try 'help')");
        }
        command->handler(args);
    } catch (const Error& e) {
        out_ << "error: " << e.what() << '\n';
    }
    out_.flush();
    return !quit_requested_;
}

void CommandInterpreter::cmd_help(const Args& args) {
    if (args.size() > 1) {
        const Command* command = find_command(args[1]);
        if (!command) {
            throw_error("unknown command '" + args[1] + "'");
        }
        out_ << "usage: " << command->usage << '\n' << command->help << '\n';
        if (command->names.size() > 1) {
            out_ << "aliases: " << join(command->names, 1) << '\n';
        }
        return;
    }
    out_ << "Commands:\n";
    for (const auto& command : commands_) {
        out_ << "  " << std::left << std::setw(34) << command.usage << command.help << '\n';
    }
}

void CommandInterpreter::cmd_run(const Args& args) {
    if (args.size() > 1) {
        debugger_.set_args(Args(args.begin() + 1, args.end()));
    }
    if (debugger_.has_process()) {
        out_ << "Restarting program.\n";
    }
    out_ << "Starting program: " << debugger_.program();
    if (!debugger_.args().empty()) {
        out_ << ' ' << join(debugger_.args());
    }
    out_ << std::endl;
    report_stop(debugger_.run());
}

void CommandInterpreter::cmd_starti(const Args& args) {
    if (args.size() > 1) {
        debugger_.set_args(Args(args.begin() + 1, args.end()));
    }
    report_stop(debugger_.start());
}

void CommandInterpreter::cmd_continue(const Args& args) {
    require_no_args(args);
    out_.flush();
    report_stop(debugger_.continue_execution());
}

void CommandInterpreter::cmd_quit(const Args& args) {
    require_no_args(args);
    quit_requested_ = true;
}

void CommandInterpreter::cmd_kill(const Args& args) {
    require_no_args(args);
    pid_t pid = debugger_.pid();
    debugger_.kill();
    out_ << "Process " << pid << " killed\n";
}

void CommandInterpreter::cmd_attach(const Args& args) {
    if (args.size() != 2) {
        throw_error("usage: attach <pid>");
    }
    auto pid = parse_uint(args[1]);
    if (!pid || *pid == 0 || *pid > static_cast<std::uint64_t>(INT32_MAX)) {
        throw_error("invalid pid '" + args[1] + "'");
    }
    out_ << "Attaching to process " << *pid << '\n';
    report_stop(debugger_.attach(static_cast<pid_t>(*pid)));
}

void CommandInterpreter::cmd_detach(const Args& args) {
    require_no_args(args);
    pid_t pid = debugger_.pid();
    debugger_.detach();
    out_ << "Detached from process " << pid << '\n';
}

void CommandInterpreter::report_stop(const StopEvent& event) {
    const StopReason& reason = event.reason;
    switch (reason.state) {
        case ProcessState::Exited:
            out_ << "Process " << event.pid << " exited with status " << reason.info << '\n';
            return;
        case ProcessState::Terminated:
            out_ << "Process " << event.pid << " terminated by signal " << signal_name(reason.info)
                 << " (" << signal_description(reason.info) << ")\n";
            return;
        case ProcessState::Stopped:
            break;
        case ProcessState::Running:
        case ProcessState::Detached:
            return;
    }
    if (reason.trap == TrapType::Exec) {
        out_ << "Process " << event.pid << " stopped at program start\n";
    } else {
        out_ << "Process " << event.pid << " stopped by signal " << signal_name(reason.info) << " ("
             << signal_description(reason.info) << ")\n";
    }
}

Repl::Repl(CommandInterpreter& interpreter, std::istream& in, std::ostream& out, Options options)
    : interpreter_(interpreter), in_(in), out_(out), options_(options) {}

bool Repl::read_line(std::string& line) {
    if (options_.interactive) {
        char* raw = ::linenoise(kPrompt);
        if (raw == nullptr) {
            return false;  // Ctrl-D (or Ctrl-C at an empty prompt)
        }
        line = raw;
        ::linenoiseFree(raw);
        return true;
    }
    if (!std::getline(in_, line)) {
        return false;
    }
    if (options_.echo) {
        out_ << kPrompt << line << '\n';
    }
    return true;
}

void Repl::run() {
    std::string line;
    std::string last_command;
    while (read_line(line)) {
        if (options_.interactive) {
            // Like gdb, pressing Enter on an empty line repeats the previous command, which
            // makes "step, step, step" pleasant.
            if (trim(line).empty()) {
                line = last_command;
            } else {
                ::linenoiseHistoryAdd(line.c_str());
                last_command = line;
            }
        }
        if (!interpreter_.execute(line)) {
            return;
        }
    }
}

}  // namespace minidbg
