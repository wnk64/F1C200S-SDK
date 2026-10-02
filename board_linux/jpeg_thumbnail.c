#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <setjmp.h>
#include <jpeglib.h>

struct thumb_error { struct jpeg_error_mgr base; jmp_buf jump; };
struct decoder {
    struct jpeg_decompress_struct jpeg;
    struct thumb_error error;
    uint8_t *row;
    int created;
};
static void fail(j_common_ptr jpeg) {
    struct thumb_error *error = (struct thumb_error *)jpeg->err;
    longjmp(error->jump, 1);
}
int board_jpeg_thumbnail(const uint8_t *input, size_t length, int width, int height, uint16_t *pixels)
{
    if (!input || !pixels || length < 2 || length > 131072 || input[0] != 0xff || input[1] != 0xd8 ||
        width < 1 || height < 1 || width > 256 || height > 256) return 0;
    struct decoder *d = calloc(1, sizeof(*d));
    if (!d) return 0;
    d->jpeg.err = jpeg_std_error(&d->error.base); d->error.base.error_exit = fail;
    if (setjmp(d->error.jump)) {
        if (d->created) jpeg_destroy_decompress(&d->jpeg);
        free(d->row); free(d); return 0;
    }
    jpeg_create_decompress(&d->jpeg); d->created = 1;
    jpeg_mem_src(&d->jpeg, input, length);
    jpeg_read_header(&d->jpeg, TRUE);
    if (d->jpeg.image_width > 2048 || d->jpeg.image_height > 2048) {
        jpeg_destroy_decompress(&d->jpeg); free(d); return 0;
    }
    d->jpeg.out_color_space = JCS_RGB;
    d->jpeg.scale_num = 1;
    d->jpeg.scale_denom = d->jpeg.image_width > 1024 ? 8 : d->jpeg.image_width > 512 ? 4 :
        d->jpeg.image_width > 256 ? 2 : 1;
    jpeg_start_decompress(&d->jpeg);
    d->row = malloc((size_t)d->jpeg.output_width * 3);
    if (!d->row) { jpeg_destroy_decompress(&d->jpeg); free(d); return 0; }
    int next_y = 0;
    while (d->jpeg.output_scanline < d->jpeg.output_height) {
        unsigned source_y = d->jpeg.output_scanline;
        JSAMPROW row = d->row;
        if (jpeg_read_scanlines(&d->jpeg, &row, 1) != 1) fail((j_common_ptr)&d->jpeg);
        while (next_y < height && (unsigned)(next_y * d->jpeg.output_height / height) == source_y) {
            for (int x = 0; x < width; ++x) {
                const uint8_t *p = d->row + (size_t)(x * d->jpeg.output_width / width) * 3;
                pixels[next_y * width + x] = (uint16_t)(((p[0] >> 3) << 11) | ((p[1] >> 2) << 5) | (p[2] >> 3));
            }
            ++next_y;
        }
    }
    jpeg_finish_decompress(&d->jpeg); jpeg_destroy_decompress(&d->jpeg);
    free(d->row); free(d); return next_y == height;
}
