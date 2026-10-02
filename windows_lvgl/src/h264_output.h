#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

struct H264OutputOptions {
    std::string source_uri;
    std::string ffmpeg_executable{"ffmpeg.exe"};
    std::string http_headers{"Referer: https://www.bilibili.com/\r\nUser-Agent: Mozilla/5.0\r\n"};
    uint32_t io_timeout_ms{10000};
};

enum class H264ReadStatus { Data, Timeout, End, Error };

class H264Output {
public:
    virtual ~H264Output() = default;
    virtual bool open(const H264OutputOptions& options, std::string& error) = 0;
    virtual H264ReadStatus read(uint8_t* buffer, size_t capacity, size_t& bytes_read,
                                uint32_t timeout_ms, std::string& error) = 0;
    virtual void stop() = 0;
};

std::unique_ptr<H264Output> platform_create_h264_output();
