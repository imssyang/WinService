#include "tabulate/table.hpp"
#include "util/WsmArg.h"
#include "core/WsmSvc.h"
#include "core/WsmAgent.h"

int ConsoleMain(int argc, char *argv[], bool hasConsole)
{
    auto& m = ArgManager::Inst(argc, argv).Get("main");
    if (m.is_subcommand_used("install")) {
        auto& cmd = ArgManager::Inst().Get("install");
        auto name = cmd.get<std::string>("--name");
        auto alias = cmd.get<std::string>("--alias");
        auto desc = cmd.get<std::string>("--desc");
        auto path = cmd.get<std::string>("--path");
        bool is_agent = cmd.is_used("--agent");
        if (is_agent) {
            WsmAgent app(name, alias);
            app.Install(path);
            app.SetDescription(desc);
        } else {
            WsmApp app(name, alias);
            app.Install(path);
            app.SetDescription(desc);
        }
    } else if (m.is_subcommand_used("uninstall")) {
        auto& cmd = ArgManager::Inst().Get("uninstall");
        auto name = cmd.get<std::string>("name");
        WsmApp app(name);
        app.Uninstall();
    } else if (m.is_subcommand_used("start")) {
        auto& cmd = ArgManager::Inst().Get("start");
        auto name = cmd.get<std::string>("name");
        WsmApp app(name);
        app.Start();
    } else if (m.is_subcommand_used("stop")) {
        auto& cmd = ArgManager::Inst().Get("stop");
        auto name = cmd.get<std::string>("name");
        WsmApp app(name);
        app.Stop();
    } else if (m.is_subcommand_used("list")) {
        auto& cmd = ArgManager::Inst().Get("list");
        auto services = WsmSvc::Inst().GetServices();

        tabulate::Table services_table;
        services_table.format().multi_byte_characters(true);
        services_table.add_row({"Name", "Alias", "Type", "State", "PID", "Path", "Startup"});
        services_table.row(0).format().font_color(tabulate::Color::yellow)
            .font_align(tabulate::FontAlign::center)
            .font_style({tabulate::FontStyle::bold});
        services_table.column(0).format().width(15);
        services_table.column(1).format().width(30);
        services_table.column(2).format().width(20);
        services_table.column(3).format().width(10);
        services_table.column(4).format().width(10);
        services_table.column(5).format().width(50);
        services_table.column(6).format().width(15);

        for (auto& s : services) {
            WsmApp app(s.serviceName);
            auto wscopt = app.GetConfig();
            if (!wscopt)
                continue;

            auto& config = wscopt.value();
            if (cmd.is_used("--filter-path")) {
                auto filter_path = cmd.get<std::string>("--filter-path");
                if (config.binaryPathName.find(filter_path) == std::string::npos) {
                    continue;
                }
            }

            services_table.add_row({s.serviceName, s.displayName,
                s.getServiceType(), s.getCurrentState(), std::to_string(s.processId),
                config.binaryPathName, config.getStartType()});
        }

        SPDLOG_COUT(services_table.str());
    } else if (m.is_subcommand_used("/RunAsService")) {
        auto& cmd = ArgManager::Inst().Get("/RunAsService");
        auto name = cmd.get<std::string>("name");
        WsmAgent app(name);
        app.Dispatch();
    } else {
        SPDLOG_COUT(m.help().str());
    }

    return 0;
}

int main(int argc, char *argv[]) {
    SetConsoleOutputCP(CP_UTF8);

    InitSpdlog(false, true);

    CHAR currentDir[MAX_PATH] = {0,};
    GetCurrentDirectoryA(MAX_PATH, currentDir);
    SPDLOG_INFO("main [{}:{}] @ [{}]", argc, argv[0], currentDir);

    try {
        ConsoleMain(argc, argv, false);
    } catch (const std::runtime_error& err) {
        std::cerr << err.what() << std::endl;
    } catch (std::logic_error& err) {
        std::cerr << err.what() << std::endl;
    }
    return 0;
}


