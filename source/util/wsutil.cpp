#include "util/wsutil.h"
#include "spdlog/details/null_mutex.h"
#include "spdlog/pattern_formatter.h"
#include "spdlog/sinks/base_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/sinks/rotating_file_sink.h"

#pragma comment(lib, "Shlwapi.lib")
#ifdef _DEBUG
#pragma comment(lib, "DbgHelp.lib")
#endif

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
    void format(const spdlog::details::log_msg &msg, const std::tm &, spdlog::memory_buf_t &dest) override {
        std::string payload(msg.payload.data(), msg.payload.size());
        if (payload.find("WinApi@") != std::string::npos) {
            std::string extra_msg = extra();
            dest.append(extra_msg.data(), extra_msg.data() + extra_msg.size());
        }
    }

    std::unique_ptr<custom_flag_formatter> clone() const override {
        return spdlog::details::make_unique<wsm_formatter_flag>();
    }

private:
    std::string extra() {
        DWORD dwLastError = GetLastError();
        LPVOID lpMsgBuf;
        FormatMessage(
            FORMAT_MESSAGE_ALLOCATE_BUFFER |
            FORMAT_MESSAGE_FROM_SYSTEM |
            FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL, dwLastError,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            (LPSTR) &lpMsgBuf, 0, NULL );

        std::string msg((LPCSTR)lpMsgBuf);
        msg.erase(std::remove_if(msg.begin(), msg.end(),
            [](char c){ return c == '\n'; }), msg.end());
        LocalFree(lpMsgBuf);

        std::stringstream result;
        result << dwLastError << ":" << msg;
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

            std::filesystem::path log_dir(GetLogDirectory());
            std::filesystem::path file_name(GetProgramName() + ".log");
            std::filesystem::path file_path = log_dir / file_name;
            auto max_size = 1024 * 1024 * 50; // 50MB
            auto max_files = 3;
            file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(file_path.string(), max_size, max_files);
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
    GetWorkDirectory();
}

void WriteServiceLog(const std::string& svcName, const std::string& logContext)
{
    std::filesystem::path logDir(GetLogDirectory());
    std::filesystem::path logName(svcName + ".log");
    std::filesystem::path logPath = logDir / logName;
    std::ofstream logFile(logPath.string(), std::ios::app);
    if (!logFile) {
        logFile.open(logPath.string(), std::ios::out);
    }

    if (!logFile.is_open()) {
        SPDLOG_ERROR("{} can't open!", logPath.string());
        return;
    }

    logFile << logContext;
    logFile.close();
}

std::string GetWorkDirectory()
{
    char modulePath[MAX_PATH];
    GetModuleFileName(NULL, modulePath, MAX_PATH);

    std::string currentPath(modulePath);
    std::string::size_type pos = currentPath.find_last_of("\\/");
    std::string currentDir = currentPath.substr(0, pos);
    SetCurrentDirectory(currentDir.c_str());
    return currentDir;
}

std::string GetLogDirectory()
{
    std::filesystem::path workDir(GetWorkDirectory());
    std::filesystem::path subLog("logs");
    std::filesystem::path logDir = workDir / subLog;
    CreateDirectory(logDir.string().data(), NULL);
    return logDir.string();
}

std::string GetProgramName()
{
    char modulePath[MAX_PATH];
    GetModuleFileName(NULL, modulePath, MAX_PATH);

    std::string fileName = PathFindFileName(modulePath);
    std::string fileExt = PathFindExtension(fileName.data());
    return fileName.substr(0, fileName.length() - fileExt.length());
}

std::string Utf8ToAnsi(const std::string& utf8)
{
    int wideSize = ::MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
    if (wideSize == 0)
        throw std::runtime_error("Failed to get length of wide-string.");

    wchar_t* wide = new wchar_t[wideSize];
    if (::MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, wide, wideSize) == 0)
        throw std::runtime_error("Failed to convert UTF8 to wide-string.");

    int ansiSize = ::WideCharToMultiByte(CP_ACP, 0, wide, -1, nullptr, 0, nullptr, nullptr);
    if (ansiSize == 0)
        throw std::runtime_error("Failed to get length of ACP string.");

    char* ansi = new char[ansiSize];
    if (::WideCharToMultiByte(CP_ACP, 0, wide, -1, ansi, ansiSize, nullptr, nullptr) == 0)
        throw std::runtime_error("Failed to convert wide-string to ACP");

    std::string result(ansi);
    delete[] ansi;
    delete[] wide;
    return result;
}

std::string AnsiToUtf8(const std::string& ansi)
{
    int wideSize = ::MultiByteToWideChar(CP_ACP, 0, ansi.c_str(), -1, nullptr, 0);
    if (wideSize == 0)
        throw std::runtime_error("Failed to get length of wide-string.");

    wchar_t* wide = new wchar_t[wideSize];
    if (::MultiByteToWideChar(CP_ACP, 0, ansi.c_str(), -1, wide, wideSize) == 0)
        throw std::runtime_error("Failed to convert ACP to wide-string.");

    int utf8Size = ::WideCharToMultiByte(CP_UTF8, 0, wide, -1, nullptr, 0, nullptr, nullptr);
    if (utf8Size == 0)
        throw std::runtime_error("Failed to get length of UTF8 string.");

    char* utf8 = new char[utf8Size];
    if (::WideCharToMultiByte(CP_UTF8, 0, wide, -1, utf8, utf8Size, nullptr, nullptr) == 0)
        throw std::runtime_error("Failed to convert wide-string to UTF8");

    std::string result(utf8);
    delete[] utf8;
    delete[] wide;
    return result;
}

void ForceKillProcess(DWORD processId)
{
    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, processId);
    if (!hProcess) {
        SPDLOG_ERROR("Process:{} open failed! WinApi@", processId);
        return;
    }

    if (!TerminateProcess(hProcess, 0)) {
        SPDLOG_ERROR("Process:{} kill failed! WinApi@", processId);
        return;
    }

    SPDLOG_INFO("Force kill process:{}", processId);
    CloseHandle(hProcess);
}

void PrintStackContext(CONTEXT* ctx)
{
#ifdef _DEBUG
    HANDLE process = GetCurrentProcess();
    HANDLE thread = GetCurrentThread();

    STACKFRAME64 stack;
    memset(&stack, 0, sizeof(STACKFRAME64));
#if !defined(_M_AMD64)
    stack.AddrPC.Offset = (*ctx).Eip;
    stack.AddrPC.Mode = AddrModeFlat;
    stack.AddrStack.Offset = (*ctx).Esp;
    stack.AddrStack.Mode = AddrModeFlat;
    stack.AddrFrame.Offset = (*ctx).Ebp;
    stack.AddrFrame.Mode = AddrModeFlat;
#endif

    // On x64, StackWalk64 modifies the context record, that could
    // cause crashes, so we create a copy to prevent it.
    CONTEXT ctxCopy;
    memcpy(&ctxCopy, ctx, sizeof(CONTEXT));

    SymInitialize(process, NULL, TRUE ); // load symbols

    const int maxNameLen = 256;
    CHAR moduleName[maxNameLen];
    CHAR symbolBuf[sizeof(SYMBOL_INFO) + MAX_SYM_NAME * sizeof(TCHAR)];
    for (ULONG frame = 0; ; frame++) {
        // get next call from stack
        BOOL result = StackWalk64(
#if defined(_M_AMD64)
            IMAGE_FILE_MACHINE_AMD64,
#else
            IMAGE_FILE_MACHINE_I386,
#endif
            process,
            thread,
            &stack,
            &ctxCopy,
            NULL,
            SymFunctionTableAccess64,
            SymGetModuleBase64,
            NULL
        );

        if (!result)
            break;

        // get symbol name for address
        DWORD64 symbolDisp = 0;
        PSYMBOL_INFO symbol = (PSYMBOL_INFO)symbolBuf;
        symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
        symbol->MaxNameLen = MAX_SYM_NAME;
        SymFromAddr(process, (ULONG64)stack.AddrPC.Offset, &symbolDisp, symbol);

        //try to get line
        DWORD lineDisp = 0;
        IMAGEHLP_LINE64 *line = (IMAGEHLP_LINE64 *)malloc(sizeof(IMAGEHLP_LINE64));
        line->SizeOfStruct = sizeof(IMAGEHLP_LINE64);
        if (SymGetLineFromAddr64(process, stack.AddrPC.Offset, &lineDisp, line)) {
            SPDLOG_INFO("\tat {} in {}: line: {}: address: 0x{:X}", symbol->Name, line->FileName, line->LineNumber, symbol->Address);
        } else {
            SPDLOG_INFO("\tat {}, address 0x{:X}", symbol->Name, symbol->Address);
            HMODULE hModule = NULL;
            GetModuleHandleEx(
                GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                (LPCTSTR)(stack.AddrPC.Offset),
                &hModule
            );

            // at least print module name
            lstrcpyA(moduleName,"");
            if(hModule != NULL)
                GetModuleFileNameA(hModule, moduleName, maxNameLen);
            SPDLOG_INFO ("in {}", moduleName);
        }

        free(line);
        line = NULL;
    }
#endif // DEBUG
}
