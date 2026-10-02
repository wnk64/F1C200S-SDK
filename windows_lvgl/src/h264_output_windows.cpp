#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "h264_output.h"

#include <algorithm>
#include <array>
#include <vector>

namespace {
class Handle {
public:
    HANDLE value{};
    ~Handle() { reset(); }
    void reset(HANDLE next = nullptr) {
        if (value && value != INVALID_HANDLE_VALUE) CloseHandle(value);
        value = next;
    }
    Handle() = default;
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;
};

std::wstring wide(const std::string& text) {
    if (text.empty()) return {};
    const int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(),
                                           static_cast<int>(text.size()), nullptr, 0);
    if (!length) return {};
    std::wstring result(length, L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(),
                       static_cast<int>(text.size()), result.data(), length);
    return result;
}

std::wstring quote(const std::wstring& text) {
    std::wstring result = L"\"";
    size_t slashes = 0;
    for (wchar_t character : text) {
        if (character == L'\\') {
            ++slashes;
        } else {
            result.append(character == L'"' ? slashes * 2 + 1 : slashes, L'\\');
            result += character;
            slashes = 0;
        }
    }
    result.append(slashes * 2, L'\\');
    result += L'"';
    return result;
}

std::string win_error(const char* operation) {
    return std::string(operation) + " (Windows error " + std::to_string(GetLastError()) + ")";
}

class WindowsH264Output final : public H264Output {
public:
    ~WindowsH264Output() override { stop(); }

    bool open(const H264OutputOptions& options, std::string& error) override {
        stop();
        error.clear();
        if (options.source_uri.size() > 16384 || options.ffmpeg_executable.size() > 16384 || options.http_headers.size() > 8192) {
            error = "H.264 output options exceed size limits";
            return false;
        }
        const std::wstring source = wide(options.source_uri);
        const std::wstring executable = wide(options.ffmpeg_executable);
        const std::wstring headers = wide(options.http_headers);
        if (source.empty() || executable.empty() || options.source_uri.find('\0') != std::string::npos ||
            options.ffmpeg_executable.find('\0') != std::string::npos ||
            options.http_headers.find('\0') != std::string::npos ||
            (!options.http_headers.empty() && headers.empty()) ||
            options.io_timeout_ms == 0 || options.io_timeout_ms > 120000) {
            error = "Invalid H.264 output options";
            return false;
        }
        std::wstring command = quote(executable) +
            L" -hide_banner -loglevel error -nostdin -xerror -protocol_whitelist file,http,https,tcp,tls,crypto"
            L" -probesize 65536 -analyzeduration 1000000 -max_probe_packets 32 -threads 1"
            L" -rw_timeout " + std::to_wstring(static_cast<uint64_t>(options.io_timeout_ms) * 1000);
        if (options.source_uri.rfind("http://", 0) == 0 || options.source_uri.rfind("https://", 0) == 0) {
            if (!headers.empty()) command += L" -headers " + quote(headers);
        }
        command += L" -i " + quote(source) +
            L" -map 0:v:0 -an -sn -dn -c:v copy -bsf:v h264_mp4toannexb -f h264 pipe:1";
        if (command.size() >= 32767) {
            error = "H.264 output command is too long";
            return false;
        }
        SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
        Handle output_write;
        Handle diagnostic_write;
        Handle input;
        if (!CreatePipe(&output.value, &output_write.value, &security, 16384) ||
            !CreatePipe(&diagnostic.value, &diagnostic_write.value, &security, 4096) ||
            !SetHandleInformation(output.value, HANDLE_FLAG_INHERIT, 0) ||
            !SetHandleInformation(diagnostic.value, HANDLE_FLAG_INHERIT, 0)) {
            error = win_error("Create H.264 output pipe");
            stop();
            return false;
        }
        input.reset(CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                &security, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
        job.reset(CreateJobObjectW(nullptr, nullptr));
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
        limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        if (input.value == INVALID_HANDLE_VALUE || !job.value ||
            !SetInformationJobObject(job.value, JobObjectExtendedLimitInformation, &limits, sizeof(limits))) {
            error = win_error("Initialize H.264 output process");
            stop();
            return false;
        }
        SIZE_T attribute_size = 0;
        InitializeProcThreadAttributeList(nullptr, 1, 0, &attribute_size);
        std::vector<uint8_t> storage(attribute_size);
        auto* attributes = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(storage.data());
        if (!InitializeProcThreadAttributeList(attributes, 1, 0, &attribute_size)) {
            error = win_error("Initialize H.264 process attributes");
            stop();
            return false;
        }
        HANDLE inherited[] = {output_write.value, diagnostic_write.value, input.value};
        STARTUPINFOEXW startup{};
        startup.StartupInfo.cb = sizeof(startup);
        startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
        startup.StartupInfo.wShowWindow = SW_HIDE;
        startup.StartupInfo.hStdOutput = output_write.value;
        startup.StartupInfo.hStdError = diagnostic_write.value;
        startup.StartupInfo.hStdInput = input.value;
        startup.lpAttributeList = attributes;
        PROCESS_INFORMATION child{};
        const bool updated = UpdateProcThreadAttribute(attributes, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
                                                        inherited, sizeof(inherited), nullptr, nullptr);
        const bool created = updated && CreateProcessW(nullptr, command.data(), nullptr, nullptr, TRUE,
            CREATE_NO_WINDOW | CREATE_SUSPENDED | EXTENDED_STARTUPINFO_PRESENT,
            nullptr, nullptr, &startup.StartupInfo, &child);
        const DWORD create_error = GetLastError();
        DeleteProcThreadAttributeList(attributes);
        if (!created) {
            SetLastError(create_error);
            error = win_error("Start FFmpeg H.264 output");
            stop();
            return false;
        }
        process.reset(child.hProcess);
        Handle thread;
        thread.reset(child.hThread);
        if (!AssignProcessToJobObject(job.value, process.value) || ResumeThread(thread.value) == DWORD(-1)) {
            error = win_error("Attach FFmpeg H.264 output process");
            stop();
            return false;
        }
        return true;
    }

    H264ReadStatus read(uint8_t* buffer, size_t capacity, size_t& bytes_read,
                        uint32_t timeout_ms, std::string& error) override {
        bytes_read = 0;
        error.clear();
        if (!process.value || !buffer || capacity == 0 || capacity > 65536 || timeout_ms > 120000) {
            error = "Invalid H.264 read or output is stopped";
            return H264ReadStatus::Error;
        }
        const ULONGLONG deadline = GetTickCount64() + timeout_ms;
        for (;;) {
            drain_diagnostic();
            DWORD available = 0;
            const bool peeked = PeekNamedPipe(output.value, nullptr, 0, nullptr, &available, nullptr);
            const DWORD pipe_error = peeked ? ERROR_SUCCESS : GetLastError();
            if (peeked && available) {
                DWORD received = 0;
                if (!ReadFile(output.value, buffer, static_cast<DWORD>(std::min<size_t>(capacity, available)),
                              &received, nullptr)) {
                    error = win_error("Read H.264 output");
                    return H264ReadStatus::Error;
                }
                bytes_read = received;
                return H264ReadStatus::Data;
            }
            if (!peeked && pipe_error != ERROR_BROKEN_PIPE) {
                SetLastError(pipe_error);
                error = win_error("Inspect H.264 output pipe");
                return H264ReadStatus::Error;
            }
            if (WaitForSingleObject(process.value, 0) == WAIT_OBJECT_0) {
                drain_diagnostic();
                DWORD final_available = 0;
                if (PeekNamedPipe(output.value, nullptr, 0, nullptr, &final_available, nullptr) && final_available) continue;
                DWORD exit_code = 0;
                if (!GetExitCodeProcess(process.value, &exit_code) || exit_code != 0) {
                    error = "FFmpeg rejected or failed H.264 output (exit " + std::to_string(exit_code) + "): " + diagnostic_tail;
                    return H264ReadStatus::Error;
                }
                return H264ReadStatus::End;
            }
            if (GetTickCount64() >= deadline) return H264ReadStatus::Timeout;
            Sleep(2);
        }
    }

    void stop() override {
        if (process.value && WaitForSingleObject(process.value, 0) == WAIT_TIMEOUT) {
            TerminateProcess(process.value, 1);
            WaitForSingleObject(process.value, 2000);
        }
        process.reset();
        job.reset();
        output.reset();
        diagnostic.reset();
        diagnostic_tail.clear();
    }

private:
    void drain_diagnostic() {
        std::array<char, 4096> buffer{};
        DWORD available = 0;
        if (!diagnostic.value || !PeekNamedPipe(diagnostic.value, nullptr, 0, nullptr, &available, nullptr) || !available) return;
        DWORD received = 0;
        if (ReadFile(diagnostic.value, buffer.data(), std::min<DWORD>(available, buffer.size()), &received, nullptr)) {
            diagnostic_tail.append(buffer.data(), received);
            if (diagnostic_tail.size() > buffer.size()) diagnostic_tail.erase(0, diagnostic_tail.size() - buffer.size());
        }
    }

    Handle process;
    Handle job;
    Handle output;
    Handle diagnostic;
    std::string diagnostic_tail;
};
}

std::unique_ptr<H264Output> platform_create_h264_output() {
    return std::make_unique<WindowsH264Output>();
}
