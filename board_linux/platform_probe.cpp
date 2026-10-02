#include "platform_services.h"
#include <iostream>
int main() {
    std::string digest, error;
    if (!platform_md5_hex("abc", digest, error) || digest != "900150983cd24fb0d6963f7d28e17f72") return 1;
    std::vector<uint8_t> body;
    const std::string url = "https://api.bilibili.com/x/web-interface/nav";
    const std::string headers = "User-Agent: Mozilla/5.0\r\nReferer: https://www.bilibili.com/\r\n";
    platform_set_tls_verify(true);
    platform_set_http_timeouts(10000, 20000);
    if (!platform_https_get(url, headers, body, error)) {
        std::cerr << "TLS/API: " << error << '\n'; return 2;
    }
    std::cout << "TLS/API GET bytes=" << body.size() << " MD5=pass\n";
    if (platform_https_get(url, headers, body, error, nullptr, 16) || !body.empty()) return 3;
    std::cout << "body limit=pass\n";
    return 0;
}
