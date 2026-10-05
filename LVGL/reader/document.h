#ifndef READER_DOCUMENT_H
#define READER_DOCUMENT_H
#include <stdint.h>
int document_open(const char *path);
void document_close(void);
int document_pages(void);
int document_reflowable(void);
int document_resize_text(int page);
const char *document_error(void);
const char *document_mode(void);
uint16_t *document_render(int page, unsigned *width, unsigned *height);
#endif
