#include "util/WsmUtil.h"
#include "spdlog/details/null_mutex.h"
#include "spdlog/pattern_formatter.h"
#include "spdlog/sinks/base_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/sinks/rotating_file_sink.h"

#pragma comment(lib, "Shlwapi.lib")

template<typename Mutex>
class stdout_wsm_sink : public spdlog::sinks::base_sink<Mutex>
{
public:
    stdout_wsm_sink(bool is_gui): is_gui_(is_gui), stdout_(NULL) {}

protected:
    void sink_it_(const spdlog::details::log_msg& msg) override {
        spdlog::memory_buf_t formatted;
        spdlog::sinks::base_sink<Mutex>::formatter_->format(msg, formatted);
        console(fmt::to_string(formatted));
    }

    void flush_() override {
       std::cout << std::flush;
    }

private:
    bool console(const std::string& msg) {
        if (is_gui_) {
            DWORD done_size = 0;
            if (!stdout_)
                stdout_ = GetStdHandle(STD_OUTPUT_HANDLE);
            WriteFile(stdout_, msg.data(), (DWORD)msg.size(), &done_size, NULL);
            return bool(done_size == msg.size());
        }

        std::cout << msg;
        return true;
    }

private:
    bool is_gui_;
    HANDLE stdout_;
};

using stdout_wsm_sink_mt = stdout_wsm_sink<std::mutex>;

class wsm_formatter_flag : public spdlog::custom_flag_formatter
{
public:
    void format(const spdlog::details::log_msg &, const std::tm &, spdlog::memory_buf_t &dest) override {
        std::string extra_msg = extra();
        if (!extra_msg.empty())
            dest.append(extra_msg.data(), extra_msg.data() + extra_msg.size());
    }

    std::unique_ptr<custom_flag_formatter> clone() const override {
        return spdlog::details::make_unique<wsm_formatter_flag>();
    }

private:
    std::string extra() {
        DWORD code = GetLastError();
        if (code == ERROR_SUCCESS
            || code == ERROR_INVALID_HANDLE
            || code == ERROR_INSUFFICIENT_BUFFER
            || code == ERROR_ALREADY_EXISTS
            || code == ERROR_MORE_DATA
            || code == ERROR_BROKEN_PIPE)
            return std::move(std::string());

        LPVOID lpMsgBuf;
        FormatMessage(
            FORMAT_MESSAGE_ALLOCATE_BUFFER |
            FORMAT_MESSAGE_FROM_SYSTEM |
            FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL, code,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            (LPSTR) &lpMsgBuf, 0, NULL );

        std::string msg((LPCSTR)lpMsgBuf);
        msg.erase(std::remove_if(msg.begin(), msg.end(),
            [](char c){ return std::isspace(c) && c != ' '; }), msg.end());
        LocalFree(lpMsgBuf);

        std::stringstream result;
        result << " #err:" << code << ":" << msg;
        return std::move(result.str());
    }
};

void RegisterSpdCoutLogger(bool isGui)
{
    auto wsm_sink = std::make_shared<stdout_wsm_sink_mt>(isGui);
    auto logger = std::make_shared<spdlog::logger>("cout", wsm_sink);
    logger->set_pattern("%v");
    logger->set_level(spdlog::level::info);
    logger->flush_on(spdlog::level::info);
    spdlog::register_logger(logger);
}

void InitSpdlog(bool isGui, bool enableFile)
{
    try {
        spdlog::sinks_init_list sink_list;

        auto console_formatter = std::make_unique<spdlog::pattern_formatter>();
        console_formatter->add_flag<wsm_formatter_flag>('*').set_pattern("%^[%P:%L]%$ %v%*");

        std::shared_ptr<spdlog::sinks::sink> console_sink;
        if (isGui)
            console_sink = std::make_shared<stdout_wsm_sink_mt>(isGui);
        else
            console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        console_sink->set_level(spdlog::level::info);
        console_sink->set_formatter(std::move(console_formatter));

        std::shared_ptr<spdlog::sinks::sink> file_sink;
        if (enableFile) {
            auto file_formatter = std::make_unique<spdlog::pattern_formatter>();
            file_formatter->add_flag<wsm_formatter_flag>('*').set_pattern("[%P %C-%m-%d %H:%M:%S.%e %^%L%$ %s:%#:%!:%t] %v%*");

            auto file_path = "O:/30-Project/_sample/WinServiceManager/log/wsm.txt";
            auto max_size = 1024 * 1024 * 50; // 50MB
            auto max_files = 3;
            file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(file_path, max_size, max_files);
            file_sink->set_level(spdlog::level::info);
            file_sink->set_formatter(std::move(file_formatter));

            sink_list = spdlog::sinks_init_list({console_sink, file_sink});
        } else {
            sink_list = spdlog::sinks_init_list({console_sink});
        }

        auto logger = std::make_shared<spdlog::logger>("default", sink_list);
        logger->flush_on(spdlog::level::info);

        spdlog::set_default_logger(logger);

        RegisterSpdCoutLogger(isGui);
    } catch (const spdlog::spdlog_ex& ex) {
        std::cout << "Log initialization failed: " << ex.what() << std::endl;
    }
}

std::string UTF8toGBK(const std::string& utf8)
{
    int wide_length = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), -1, NULL, 0);
    if (wide_length == 0)
        throw std::runtime_error("Failed to get wide string length for UTF-8 to GBK conversion");

    std::wstring wide(wide_length, L'\0');
    if (MultiByteToWideChar(CP_UTF8, 0, utf8.data(), -1, &wide[0], wide_length) == 0)
        throw std::runtime_error("Failed to convert UTF-8 to wide string for GBK conversion");

    int gbk_length = WideCharToMultiByte(CP_ACP, 0, wide.data(), -1, NULL, 0, NULL, NULL);
    if (gbk_length == 0)
        throw std::runtime_error("Failed to get GBK string length for UTF-8 to GBK conversion");

    std::string gbk(gbk_length, '\0');
    if (WideCharToMultiByte(CP_ACP, 0, wide.data(), -1, &gbk[0], gbk_length, NULL, NULL) == 0)
        throw std::runtime_error("Failed to convert wide string to GBK");
    return gbk;
}

std::string GBKtoUTF8(const std::string& gbk)
{
    int wide_length = MultiByteToWideChar(CP_ACP, 0, gbk.data(), -1, NULL, 0);
    if (wide_length == 0)
        throw std::runtime_error("Failed to get wide string length for GBK to UTF-8 conversion");

    std::wstring wide(wide_length, L'\0');
    if (MultiByteToWideChar(CP_ACP, 0, gbk.data(), -1, &wide[0], wide_length) == 0)
        throw std::runtime_error("Failed to convert GBK to wide string for UTF-8 conversion");

    int utf8_length = WideCharToMultiByte(CP_UTF8, 0, wide.data(), -1, NULL, 0, NULL, NULL);
    if (utf8_length == 0)
        throw std::runtime_error("Failed to get UTF-8 string length for GBK to UTF-8 conversion");

    std::string utf8(utf8_length, '\0');
    if (WideCharToMultiByte(CP_UTF8, 0, wide.data(), -1, &utf8[0], utf8_length, NULL, NULL) == 0)
        throw std::runtime_error("Failed to convert wide string to UTF-8");
    return utf8;
}

