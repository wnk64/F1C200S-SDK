#!/bin/sh
set -eu
self=$(CDPATH= cd "$(dirname "$0")" && pwd)
SDK=${SDK:-$(CDPATH= cd "$self/../../../ROOTFS/buildroot-2018.02.11/output/host" && pwd)}
sysroot=$("$SDK/bin/arm-buildroot-linux-gnueabi-gcc" --print-sysroot)
mkdir -p "$self/build-curl"
cd "$self/build-curl"
env CC="$SDK/bin/arm-buildroot-linux-gnueabi-gcc" AR="$SDK/bin/arm-buildroot-linux-gnueabi-ar" \
 RANLIB="$SDK/bin/arm-buildroot-linux-gnueabi-ranlib" STRIP="$SDK/bin/arm-buildroot-linux-gnueabi-strip" \
 CPPFLAGS="-I$sysroot/usr/include" CFLAGS="-Os -ffunction-sections -fdata-sections" \
 LDFLAGS="-L$sysroot/usr/lib -Wl,-rpath-link,$sysroot/usr/lib" PKG_CONFIG=/bin/false \
 "$self/vendor/curl-8.16.0/configure" --host=arm-buildroot-linux-gnueabi --build=x86_64-pc-linux-gnu \
 --disable-shared --enable-static --with-openssl="$sysroot/usr" \
 --without-zlib --without-zstd --without-brotli --without-libpsl --without-libidn2 \
 --without-nghttp2 --without-nghttp3 --without-ngtcp2 --disable-threaded-resolver \
 --disable-ldap --disable-ldaps --disable-ftp --disable-file --disable-rtsp --disable-tftp \
 --disable-dict --disable-telnet --disable-pop3 --disable-imap --disable-smtp --disable-smb \
 --disable-gopher --disable-mqtt --disable-websockets --disable-manual --disable-docs \
 --with-ca-bundle=/etc/ssl/certs/ca-certificates.crt --without-ca-path
make -C lib -j2
