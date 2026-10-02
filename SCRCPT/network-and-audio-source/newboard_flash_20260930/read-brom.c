// SPDX-License-Identifier: GPL-2.0-only
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

int main(int argc, char **argv)
{
    const size_t length = 32768;
    off_t address = 0;
    int fd = open("/dev/mem", O_RDONLY | O_SYNC);
    void *rom;
    size_t written;

    if (argc == 2 && strcmp(argv[1], "high-alias") == 0)
        address = (off_t)0xffff0000ULL;
    else if (argc != 1) {
        fputs("usage: read-brom [high-alias]\n", stderr);
        if (fd >= 0)
            close(fd);
        return 1;
    }
    if (fd < 0) {
        perror("open /dev/mem");
        return 1;
    }
    rom = mmap(NULL, length, PROT_READ, MAP_SHARED, fd, address);
    if (rom == MAP_FAILED) {
        perror("mmap BROM");
        close(fd);
        return 1;
    }
    written = fwrite(rom, 1, length, stdout);
    munmap(rom, length);
    close(fd);
    if (written != length || fflush(stdout)) {
        perror("write ROM dump");
        return 1;
    }
    return 0;
}
