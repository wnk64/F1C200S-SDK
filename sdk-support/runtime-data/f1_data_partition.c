#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <linux/blkpg.h>
#include <linux/fs.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>

static uint32_t le32(const unsigned char *p) {
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
static void put32(unsigned char *p, uint32_t value) {
    for (unsigned i = 0; i < 4; ++i) p[i] = (unsigned char)(value >> (8 * i));
}
static int empty(const unsigned char *p, size_t bytes) {
    while (bytes--) if (*p++) return 0;
    return 1;
}
static int fail(const char *message) { fprintf(stderr, "F1DATA: %s\n", message); return 1; }

int main(int argc, char **argv) {
    int image = argc == 4 && !strcmp(argv[1], "--image");
    const char *path = image ? argv[2] : "/dev/mmcblk0";
    const char *mode = image ? argv[3] : argc == 2 ? argv[1] : "";
    int fresh = !strcmp(mode, "--fresh"), create = !strcmp(mode, "--create");
    if (!fresh && !create && strcmp(mode, "--resume")) return fail("expected --fresh, --resume or --create");
    int fd = open(path, (create ? O_RDWR : O_RDONLY) | O_CLOEXEC | O_NOFOLLOW);
    struct stat st;
    unsigned char mbr[512], entry[16];
    uint64_t bytes = 0;
    int result = 1;
    if (fd < 0) return fail("cannot open disk");
    if (fstat(fd, &st) || (image ? !S_ISREG(st.st_mode) : !S_ISBLK(st.st_mode))) {
        fail("unexpected target type"); goto out;
    }
    if (!image) {
        struct stat root, system;
        if (stat("/", &root) || lstat("/dev/mmcblk0p2", &system) ||
            !S_ISBLK(system.st_mode) || root.st_dev != system.st_rdev) {
            fail("running root is not mmcblk0p2"); goto out;
        }
    }
    if (image) bytes = (uint64_t)st.st_size;
    else if (ioctl(fd, BLKGETSIZE64, &bytes)) { fail("cannot read disk size"); goto out; }
    uint64_t sectors = bytes / 512;
    if (bytes % 512 || sectors > UINT32_MAX || sectors < 1048576 + 131072) {
        fail("disk must be at least 576 MiB and fit an MBR"); goto out;
    }
    if (pread(fd, mbr, sizeof(mbr), 0) != sizeof(mbr) || mbr[510] != 0x55 || mbr[511] != 0xaa ||
        le32(mbr + 440) != 0xf1c20050) { fail("not an SDK MBR"); goto out; }
    const unsigned char *p1 = mbr + 446, *p2 = p1 + 16, *p3 = p2 + 16;
    if (p1[0] != 0x80 || p1[4] != 0x0e || le32(p1 + 8) != 2048 || le32(p1 + 12) != 32768 ||
        p2[0] || p2[4] != 0x83 || le32(p2 + 8) != 34816 || le32(p2 + 12) != 1013760 ||
        !empty(p3 + 16, 16)) { fail("unexpected system partitions or fourth partition"); goto out; }
    uint32_t length = (uint32_t)(sectors / 2048 * 2048 - 1048576);
    memset(entry, 0, sizeof(entry));
    entry[1] = entry[5] = 0xfe; entry[2] = entry[3] = entry[6] = entry[7] = 0xff;
    entry[4] = 0x0c; put32(entry + 8, 1048576); put32(entry + 12, length);
    if (!empty(p3, 16) && (fresh || memcmp(p3, entry, 16))) {
        fail("existing data partition is protected"); goto out;
    }
    if (create && empty(p3, 16)) {
        /* Only the third entry is written; boot code and both system entries stay intact. */
        if (pwrite(fd, entry, 16, 478) != 16 || fsync(fd)) { fail("partition entry write failed"); goto out; }
    }
    if (create && !image) {
        struct blkpg_partition part;
        memset(&part, 0, sizeof(part));
        part.start = (long long)1048576 * 512; part.length = (long long)length * 512; part.pno = 3;
        struct blkpg_ioctl_arg argument = {BLKPG_ADD_PARTITION, 0, sizeof(part), &part};
        /* A mounted root prevents BLKRRPART; add only the new partition instead. */
        if (ioctl(fd, BLKPG, &argument) && errno != EBUSY) { fail("kernel partition registration failed"); goto out; }
    }
    printf("1048576 %" PRIu32 "\n", length);
    result = 0;
out:
    close(fd);
    return result;
}
