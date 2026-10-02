#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "h264_output.h"

#include <chrono>
#include <charconv>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <thread>
#include <vector>
#include <fcntl.h>
#include <io.h>

int wmain(int argc, wchar_t** wide_arguments) {
    std::vector<std::string> arguments;
    for (int argument = 0; argument < argc; ++argument) {
        const int length = WideCharToMultiByte(CP_UTF8, 0, wide_arguments[argument], -1, nullptr, 0, nullptr, nullptr);
        std::string value(length, '\0');
        WideCharToMultiByte(CP_UTF8, 0, wide_arguments[argument], -1, value.data(), length, nullptr, nullptr);
        value.pop_back();
        arguments.push_back(std::move(value));
    }
    H264OutputOptions options;
    std::string output_path;
    size_t read_size = 8192;
    bool standard_output = false;
    bool lifecycle_test = false;
    uint64_t stop_after = 0;
    uint64_t cancel_after_ms = 0;
    uint64_t consumer_delay_ms = 0;
    for (int argument = 1; argument < argc; ++argument) {
        const std::string name = arguments[argument];
        if (name == "--stdout") standard_output = true;
        else if (name == "--lifecycle-test") lifecycle_test = true;
        else if (argument + 1 < argc) {
            const std::string value = arguments[++argument];
            if (name == "--source") options.source_uri = value;
            else if (name == "--ffmpeg") options.ffmpeg_executable = value;
            else if (name == "--output") output_path = value;
            else if (name == "--read-size" || name == "--stop-after" || name == "--io-timeout" ||
                     name == "--cancel-after" || name == "--consumer-delay") {
                uint64_t number = 0;
                const auto parsed = std::from_chars(value.data(), value.data() + value.size(), number);
                if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size() ||
                    (name == "--io-timeout" && number > 120000)) {
                    std::cerr << "Invalid number for " << name << '\n';
                    return 2;
                }
                if (name == "--read-size") read_size = static_cast<size_t>(number);
                else if (name == "--stop-after") stop_after = number;
                else if (name == "--io-timeout") options.io_timeout_ms = static_cast<uint32_t>(number);
                else if (name == "--cancel-after") cancel_after_ms = number;
                else consumer_delay_ms = number;
            } else { std::cerr << "Unknown argument: " << name << '\n'; return 2; }
        } else { std::cerr << "Missing argument: " << name << '\n'; return 2; }
    }
    if (options.source_uri.empty() || read_size == 0 || read_size > 65536 ||
        (!standard_output && output_path.empty()) || (standard_output && !output_path.empty()) ||
        options.io_timeout_ms == 0 || options.io_timeout_ms > 120000 || cancel_after_ms > 120000 || consumer_delay_ms > 1000) {
        std::cerr << "Use --source URI --ffmpeg PATH (--output FILE | --stdout) [--read-size 1..65536] [--lifecycle-test] [--stop-after BYTES] [--io-timeout 1..120000] [--cancel-after MS] [--consumer-delay 0..1000]\n";
        return 2;
    }
    std::ofstream file;
    if (standard_output) {
        if (_setmode(_fileno(stdout), _O_BINARY) == -1) return 2;
    } else {
        std::error_code path_error;
        if (std::filesystem::equivalent(std::filesystem::u8path(options.source_uri),
                                        std::filesystem::u8path(output_path), path_error)) {
            std::cerr << "Output must not overwrite the input source\n";
            return 2;
        }
        file.open(std::filesystem::u8path(output_path), std::ios::binary | std::ios::trunc);
        if (!file) { std::cerr << "Cannot open output file\n"; return 2; }
    }
    auto stream = platform_create_h264_output();
    std::string error;
    std::vector<uint8_t> buffer(read_size);
    size_t received = 0;
    if (lifecycle_test) {
        stream->stop();
        if (stream->read(buffer.data(), buffer.size(), received, 0, error) != H264ReadStatus::Error || received != 0) return 3;
        H264OutputOptions invalid = options;
        invalid.source_uri.clear();
        if (stream->open(invalid, error)) return 3;
        for (int iteration = 0; iteration < 3; ++iteration) {
            if (!stream->open(options, error)) { std::cerr << error << '\n'; return 3; }
            if (stream->read(buffer.data(), 0, received, 0, error) != H264ReadStatus::Error || received != 0) return 3;
            if (stream->read(buffer.data(), 65537, received, 0, error) != H264ReadStatus::Error || received != 0) return 3;
            if (stream->read(buffer.data(), buffer.size(), received, 120001, error) != H264ReadStatus::Error || received != 0) return 3;
            const H264ReadStatus immediate = stream->read(buffer.data(), buffer.size(), received, 0, error);
            if (immediate != H264ReadStatus::Timeout && immediate != H264ReadStatus::Data) return 3;
            if (immediate == H264ReadStatus::Timeout && received != 0) return 3;
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
            H264ReadStatus status = immediate;
            while (status == H264ReadStatus::Timeout && std::chrono::steady_clock::now() < deadline) {
                status = stream->read(buffer.data(), buffer.size(), received, 50, error);
            }
            if (status != H264ReadStatus::Data || !received) { std::cerr << error << '\n'; return 3; }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            stream->stop();
            stream->stop();
        }
    }
    if (!stream->open(options, error)) { std::cerr << error << '\n'; return 1; }
    if (stream->read(nullptr, 1, received, 0, error) != H264ReadStatus::Error || received != 0) return 3;
    uint64_t total = 0;
    bool cancelled = false;
    const auto started = std::chrono::steady_clock::now();
    auto last_data = std::chrono::steady_clock::now();
    for (;;) {
        if (cancel_after_ms && std::chrono::steady_clock::now() - started >= std::chrono::milliseconds(cancel_after_ms)) {
            stream->stop();
            cancelled = true;
            break;
        }
        const H264ReadStatus status = stream->read(buffer.data(), buffer.size(), received, 100, error);
        if (status == H264ReadStatus::Error) { std::cerr << error << '\n'; return 1; }
        if (status == H264ReadStatus::End) break;
        if (status == H264ReadStatus::Timeout) {
            if (std::chrono::steady_clock::now() - last_data > std::chrono::seconds(15)) {
                std::cerr << "H.264 output stalled\n";
                return 1;
            }
            continue;
        }
        if (!received || received > buffer.size()) return 3;
        if (standard_output) {
            if (std::fwrite(buffer.data(), 1, received, stdout) != received) { std::cerr << "Output consumer closed\n"; return 1; }
            if (std::fflush(stdout) != 0) return 1;
        } else {
            file.write(reinterpret_cast<const char*>(buffer.data()), static_cast<std::streamsize>(received));
            if (!file) { std::cerr << "Output write failed\n"; return 1; }
        }
        total += received;
        last_data = std::chrono::steady_clock::now();
        if (consumer_delay_ms) std::this_thread::sleep_for(std::chrono::milliseconds(consumer_delay_ms));
        if (stop_after && total >= stop_after) { stream->stop(); break; }
    }
    stream->stop();
    if (total == 0 && !cancelled) { std::cerr << "No H.264 data\n"; return 1; }
    if (file.is_open()) { file.close(); if (!file) return 1; }
    std::cerr << "H264_OUTPUT_BYTES=" << total << '\n';
    if (cancelled) {
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started).count();
        std::cerr << "H264_CANCEL_MS=" << elapsed << '\n';
    }
    return 0;
}
