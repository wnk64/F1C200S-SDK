#include "native.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <png.h>
#include <jpeglib.h>

int main(int argc, char **argv)
{
    assert(ons_over(0, 0xff123456) == 0xff123456);
    assert(ons_over(0xffff0000, 0xff0000ff) == 0xffff0000);
    assert(ons_over(0x80ff0000, 0xff0000ff) == 0xff80007f);
    assert(ons_over(0x80ff0000, 0) == 0x80ff0000);
    assert(!ons_surface_new(-1, 1) && !ons_surface_new(2048, 2048));
    ons_surface *d = ons_surface_new(2, 2), *s = ons_surface_new(2, 2);
    assert(d && s);
    for (int i = 0; i < 4; ++i) s->pixels[i] = 0xffffffff;
    ons_blit(d, s, -1, -1);
    assert(d->pixels[0] == 0xffffffff && d->pixels[1] == 0 && d->pixels[2] == 0 && d->pixels[3] == 0);
    ons_surface_free(s); ons_surface_free(d);
    const uint8_t compressed[] = {0xa0, 0xbb, 0xc0}; uint8_t decompressed[4] = {0};
    assert(!ons_lzss(compressed, sizeof(compressed), decompressed, 3));
    assert(!memcmp(decompressed, "AAA", 3));
    assert(ons_lzss(compressed, 1, decompressed, 3) == -1);
    assert(ons_lzss(compressed, sizeof(compressed), decompressed, 2) == -1);
    png_image png; memset(&png, 0, sizeof(png)); png.version = PNG_IMAGE_VERSION;
    png.width = 2; png.height = 1; png.format = PNG_FORMAT_RGBA;
    const uint8_t rgba[] = {255, 0, 0, 128, 0, 255, 0, 0};
    uint8_t encoded[512]; png_alloc_size_t size = sizeof(encoded);
    assert(png_image_write_to_memory(&png, encoded, &size, 0, rgba, 0, NULL));
    s = ons_image_decode(encoded, size); assert(s && s->w == 2 && s->h == 1);
    assert(s->pixels[0] == 0x80ff0000 && s->pixels[1] == 0x0000ff00); ons_surface_free(s);
    assert(!ons_image_decode(encoded, 12));
    struct jpeg_compress_struct jpeg; struct jpeg_error_mgr error;
    jpeg.err = jpeg_std_error(&error); jpeg_create_compress(&jpeg);
    unsigned char *jpg = NULL; unsigned long jpg_size = 0;
    jpeg_mem_dest(&jpeg, &jpg, &jpg_size);
    jpeg.image_width = 2; jpeg.image_height = 1; jpeg.input_components = 3; jpeg.in_color_space = JCS_RGB;
    jpeg_set_defaults(&jpeg); jpeg_start_compress(&jpeg, TRUE);
    uint8_t rgb[] = {255, 255, 255, 255, 255, 255}; JSAMPROW row = rgb;
    jpeg_write_scanlines(&jpeg, &row, 1); jpeg_finish_compress(&jpeg); jpeg_destroy_compress(&jpeg);
    s = ons_image_decode(jpg, jpg_size); assert(s && s->w == 2 && s->h == 1 && (s->pixels[0] >> 24) == 255);
    ons_surface_free(s); free(jpg);
    uint8_t bmp[62] = {0}; bmp[0] = 'B'; bmp[1] = 'M'; bmp[10] = 54; bmp[14] = 40;
    bmp[18] = 2; bmp[22] = 1; bmp[26] = 1; bmp[28] = 24;
    bmp[56] = 255; bmp[58] = 255;
    s = ons_image_decode(bmp, sizeof(bmp)); assert(s && s->pixels[0] == 0xffff0000 && s->pixels[1] == 0xff00ff00);
    ons_surface_free(s); assert(!ons_image_decode(bmp, 58));
    if (argc == 3) {
        const char *resources[] = {"cg/sayajpgfirstedition10244.png", "cg/000.jpg", "data/line.bmp"};
        for (unsigned i = 0; i < 3; ++i) {
            uint8_t *data; size_t length;
            assert(!ons_nsa_read(argv[1], resources[i], &data, &length));
            s = ons_image_decode(data, length); free(data);
            if (!s) { fprintf(stderr, "Resource decode failed: %s\n", resources[i]); return 1; }
            printf("Resource PASS %s %dx%d\n", resources[i], s->w, s->h); ons_surface_free(s);
        }
        uint8_t *data = (void *)1; size_t length = 1;
        assert(ons_nsa_read(argv[1], "missing.png", &data, &length) == -1 && !data && !length);
        d = ons_surface_new(200, 60); assert(d);
        assert(ons_text_gbk(d, argv[2], 24, 8, 32, "\xd6\xd0\xce\xc4", 0xffffffff) == 2);
        int visible = 0; for (int i = 0; i < 12000; ++i) if (d->pixels[i]) ++visible;
        assert(visible > 20); printf("GBK font PASS %d painted pixels\n", visible); ons_surface_free(d);
    }
    puts("Native image/alpha/clipping/LZSS tests PASS (no SDL)"); return 0;
}
