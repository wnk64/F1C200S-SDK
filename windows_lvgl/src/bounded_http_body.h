#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

template <typename QueryAvailable, typename ReadBytes>
bool read_bounded_http_body(QueryAvailable query_available, ReadBytes read_bytes,
                            std::vector<uint8_t>& body, size_t max_bytes, std::string& error) {
    body.clear();
    error.clear();
    if (body.capacity() > max_bytes) std::vector<uint8_t>().swap(body);
    auto fail = [&body, &error](const char* message) {
        std::vector<uint8_t>().swap(body);
        error = message;
        return false;
    };
    for (;;) {
        size_t available = 0;
        if (!query_available(available)) return fail("HTTPS response availability query failed");
        if (available == 0) {
            uint8_t probe = 0;
            size_t received = 0;
            if (!read_bytes(&probe, 1, received)) return fail("HTTPS EOF confirmation failed");
            if (received != 0) return fail("HTTPS data followed an empty availability result");
            return true;
        }
        if (available > max_bytes - body.size()) return fail("HTTPS response exceeded configured limit");
        while (available > 0) {
            const size_t offset = body.size();
            const size_t requested = std::min<size_t>(available, 16U * 1024U);
            const size_t needed = offset + requested;
            if (needed > body.capacity()) {
                const size_t growth = body.capacity() > max_bytes / 2U ? max_bytes : body.capacity() * 2U;
                body.reserve(std::min(max_bytes, std::max(needed, growth)));
            }
            body.resize(needed);
            size_t received = 0;
            if (!read_bytes(body.data() + offset, requested, received)) return fail("HTTPS response read failed");
            if (received == 0 || received > requested) return fail("HTTPS response ended before announced bytes");
            available -= received;
            body.resize(offset + received);
        }
    }
}
