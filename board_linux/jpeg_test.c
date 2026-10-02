#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <jpeglib.h>
int board_jpeg_thumbnail(const uint8_t *, size_t, int, int, uint16_t *);
int main(void) {
    uint16_t output[16];
    uint8_t invalid[] = {0xff, 0xd8, 0xff, 0xd9};
    assert(!board_jpeg_thumbnail(invalid, sizeof(invalid), 4, 4, output));
    assert(!board_jpeg_thumbnail(invalid, sizeof(invalid), 257, 4, output));
    struct jpeg_compress_struct jpeg;
    struct jpeg_error_mgr error;
    jpeg.err = jpeg_std_error(&error); jpeg_create_compress(&jpeg);
    unsigned char *data = NULL; unsigned long length = 0;
    jpeg_mem_dest(&jpeg, &data, &length);
    jpeg.image_width = jpeg.image_height = 2; jpeg.input_components = 3; jpeg.in_color_space = JCS_RGB;
    jpeg_set_defaults(&jpeg); jpeg_set_quality(&jpeg, 90, TRUE); jpeg_start_compress(&jpeg, TRUE);
    uint8_t pixels[] = {255, 0, 0, 255, 0, 0}; JSAMPROW row = pixels;
    while (jpeg.next_scanline < jpeg.image_height) jpeg_write_scanlines(&jpeg, &row, 1);
    jpeg_finish_compress(&jpeg); jpeg_destroy_compress(&jpeg);
    assert(board_jpeg_thumbnail(data, length, 4, 4, output));
    for (int i = 0; i < 16; ++i) assert((output[i] >> 11) > 28 && (output[i] & 31) < 3);
    free(data); puts("JPEG corrupt input, limits and RGB565 resize passed"); return 0;
}
