#pragma once

#include "argparse/argparse.hpp"
#include "util/wsutil.h"

class ArgManager
{
public:
    static ArgManager& Inst(int argc = 0, char *argv[] = nullptr) {
        static ArgManager inst(argc, argv);
        return inst;
    }

    ArgManager(const std::string& cmd) {
        std::stringstream ss(cmd);
        std::vector<std::string> args;
        std::string arg;
        while (ss >> arg) {
            args.push_back(arg);
        }

        try {
            version_ = "1.0";
            InitMain(args, "main");
        } catch (const std::runtime_error &err) {
            SPDLOG_ERROR(err.what());
        }
    }

    auto Get(const std::string& name) const -> const argparse::ArgumentParser& {
        auto it = cmds_.find(name);
        if (it == cmds_.end())
            throw std::logic_error("No such name: " + name);
        return *it->second;
    }

private:
    ArgManager(int argc, char *argv[]) {
        try {
            version_ = "1.0";
            InitMain(argc, argv, "main");
        } catch (const std::runtime_error &err) {
            SPDLOG_ERROR(err.what());
        }
    }

    void InitMain(int argc, char *argv[], const std::string& name) {
        std::vector<std::string> args;
        for (int i = 0; i < argc; ++i) {
            args.push_back(argv[i]);
        }
        InitMain(args, name);
    }

    void InitMain(const std::vector<std::string>& args, const std::string& name) {
        CHAR modulePath[MAX_PATH];
        GetModuleFileName(NULL, modulePath, MAX_PATH);

        std::string fileName = PathFindFileName(modulePath);
        std::string fileExt = PathFindExtension(fileName.data());
        fileName = fileName.substr(0, fileName.length() - fileExt.length());

        auto c = std::make_unique<argparse::ArgumentParser>(fileName, version_, argparse::default_arguments::none);
        AddHelpArgument(*c);
        AddVersionArgument(*c);

        c->add_subparser(InitSubcommand(AddInstallArgument, "install"));
        c->add_subparser(InitSubcommand(AddUninstallArgument, "uninstall"));
        c->add_subparser(InitSubcommand(AddStartArgument, "start"));
        c->add_subparser(InitSubcommand(AddStopArgument, "stop"));
        c->add_subparser(InitSubcommand(AddListArgument, "list"));
        c->add_subparser(InitSubcommand(AddAgentArgument, "/RunAsService"));

        c->parse_args(AmendArgument(args));
        cmds_.insert_or_assign(name, std::move(c));
    }

    template<typename ArgumentFunc>
    auto InitSubcommand(ArgumentFunc func, const std::string& name) -> argparse::ArgumentParser& {
        auto c = std::make_unique<argparse::ArgumentParser>(name, version_, argparse::default_arguments::none);
        AddHelpArgument(*c);
        func(*c);
        cmds_.insert_or_assign(name, std::move(c));
        return *cmds_[name];
    }

    static std::vector<std::string> AmendArgument(const std::vector<std::string>& arguments) {
        std::vector<std::string> args;
        for (auto& argument : arguments) {
            std::string arg(argument);
            std::string pattern = "/RunAsService:";
            size_t position = arg.find(pattern);
            if (position == std::string::npos) {
                args.push_back(arg);
            } else {
                args.push_back(arg.substr(0, position+pattern.length()-1));
                args.push_back(arg.substr(position+pattern.length()));
            }
        }
        return args;
    }

    static void AddInstallArgument(argparse::ArgumentParser& c) {
        c.add_description("Install command as service.");
        c.add_argument("-a", "--agent")
            .help("Run command in proxy mode.")
            .default_value(false)
            .implicit_value(true);
        c.add_argument("-n", "--name")
            .help("Service name.")
            .metavar("NAME");
        c.add_argument("-s", "--alias")
            .help("Alias of service.")
            .metavar("NAME");
        c.add_argument("-d", "--desc")
            .help("Service description.")
            .metavar("DESC");
        c.add_argument("-p", "--path")
            .help("Command path.")
            .metavar("PATH");
    }

    static void AddUninstallArgument(argparse::ArgumentParser& c) {
        c.add_description("Uninstall service.");
        c.add_argument("name")
            .help("Service name.")
            .metavar("NAME")
            .required();
    }

    static void AddStartArgument(argparse::ArgumentParser& c) {
        c.add_description("Start service.");
        c.add_argument("name")
            .help("Service name.")
            .metavar("NAME")
            .required();
    }

    static void AddStopArgument(argparse::ArgumentParser& c) {
        c.add_description("Stop service.");
        c.add_argument("name")
            .help("Service name.")
            .metavar("NAME")
            .required();
    }

    static void AddListArgument(argparse::ArgumentParser& c) {
        c.add_description("List service.");
        c.add_argument("-fp", "--filter-path")
            .help("Filter commands by path.")
            .metavar("VALUE");
    }

    static void AddAgentArgument(argparse::ArgumentParser& c) {
        c.add_description("Agent program as service.");
        c.add_argument("name")
            .help("Service name.")
            .metavar("NAME")
            .required();
        c.add_argument("argv")
            .help("Args of command.")
            .remaining();
    }

    void AddHelpArgument(argparse::ArgumentParser& c) {
        c.add_argument("-h", "--help")
            .action([&](const auto &) {
                SPDLOG_COUT(c.help().str());
                std::exit(0);
            })
            .default_value(false)
            .help("Show help message and exit.")
            .implicit_value(true)
            .nargs(0);
    }

    void AddVersionArgument(argparse::ArgumentParser& c) {
        c.add_argument("-v", "--version")
            .action([&](const auto &) {
                SPDLOG_COUT(version_);
                std::exit(0);
            })
            .default_value(false)
            .help("Print version information and exit.")
            .implicit_value(true)
            .nargs(0);
    }

private:
    std::string version_;
    std::map<std::string, std::unique_ptr<argparse::ArgumentParser>> cmds_;
};
