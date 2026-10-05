#ifndef READER_CONTENT_H
#define READER_CONTENT_H
#include <stdint.h>
#include <sys/types.h>
int text_open(const char *path);
void text_close(void);
int text_page(off_t start, char *output, size_t capacity, off_t *end);
off_t text_start(void);
off_t text_size(void);
const char *text_encoding(void);
void text_cycle_encoding(void);
int text_selftest(void);
uint16_t *image_open(const char *path, int native, unsigned *width, unsigned *height);
const char *image_error(void);
const char *image_info(void);
#endif
