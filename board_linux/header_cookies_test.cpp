#include "header_cookies.h"
#include <cassert>
#include <iostream>
int main() {
    assert(header_cookie_pairs("HTTP/1.1 200 OK\r\nset-cookie: A=one; Path=/\r\nSET-COOKIE: B=two\r\nX-Test: junk\r\n") == "A=one; B=two");
    assert(header_cookie_pairs("Set-Cookie: C=three\r\nContent-Length: 100\r\n") == "C=three");
    assert(header_cookie_pairs("Set-Cookie:\r\nSet-Cookie: malformed\r\n").empty());
    std::cout << "case-insensitive cookie headers and line bounds passed\n";
}
