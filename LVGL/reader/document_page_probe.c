#include "document.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
int main(int argc, char **argv)
{
    if (argc != 4) { fprintf(stderr, "usage: document-page-probe file page-number output.ppm\n"); return 1; }
    char *end; errno = 0; long page = strtol(argv[2], &end, 10);
    if (errno || *end || page < 1 || page > 100000) return 1;
    if (document_open(argv[1])) { fprintf(stderr, "%s\n", document_error()); return 1; }
    unsigned width, height;
    uint16_t *pixels = document_render(page - 1, &width, &height);
    if (!pixels) { fprintf(stderr, "%s\n", document_error()); document_close(); return 1; }
    FILE *output = fopen(argv[3], "wb"); int rc = 0;
    if (!output) rc = 1;
    else {
        if (fprintf(output, "P6\n%u %u\n255\n", width, height) < 0) rc = 1;
        for (unsigned i = 0; i < width * height && !rc; ++i) {
            unsigned p = pixels[i];
            unsigned char rgb[3] = {((p >> 11) * 255 + 15) / 31, (((p >> 5) & 63) * 255 + 31) / 63, ((p & 31) * 255 + 15) / 31};
            if (fwrite(rgb, 1, 3, output) != 3) rc = 1;
        }
        if (fclose(output)) rc = 1;
    }
    printf("DOCUMENT_PAGE page=%ld/%d dimensions=%ux%u output_rc=%d\n", page, document_pages(), width, height, rc);
    free(pixels); document_close(); return rc;
}
