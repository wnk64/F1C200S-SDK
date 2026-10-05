#include "document.h"
#include "content.h"
#include <mupdf/fitz.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef union { size_t size; long double align; void *pointer; } allocation;
static size_t live, peak;
static const size_t budget = 20u * 1024 * 1024;
static fz_context *context;
static fz_document *document;
static int pages, reflow, font_step;
static char error[160], mode[32];
static const unsigned font_sizes[] = {24, 28, 32};
static void *limited_malloc(void *user, size_t size)
{
    (void)user;
    if (size > budget || live > budget - size) return NULL;
    allocation *a = malloc(sizeof(*a) + size);
    if (!a) return NULL;
    a->size = size; live += size; if (live > peak) peak = live;
    return a + 1;
}
static void limited_free(void *user, void *pointer)
{
    (void)user;
    if (!pointer) return;
    allocation *a = (allocation *)pointer - 1;
    live -= a->size; free(a);
}
static void *limited_realloc(void *user, void *pointer, size_t size)
{
    if (!pointer) return limited_malloc(user, size);
    allocation *a = (allocation *)pointer - 1;
    size_t old = a->size;
    if (size > budget || live - old > budget - size) return NULL;
    allocation *next = realloc(a, sizeof(*a) + size);
    if (!next) return NULL;
    next->size = size; live = live - old + size; if (live > peak) peak = live;
    return next + 1;
}
const char *document_error(void) { return error; }
const char *document_mode(void) { return mode; }
int document_pages(void) { return pages; }
int document_reflowable(void) { return reflow; }
void document_close(void)
{
    if (context) {
        fz_drop_document(context, document); document = NULL;
        fz_drop_context(context); context = NULL;
        fprintf(stderr, "READER document allocator_peak=%lu live=%lu\n", (unsigned long)peak, (unsigned long)live);
    }
    pages = reflow = 0;
}
int document_open(const char *path)
{
    document_close(); error[0] = 0; peak = live; font_step = 0;
    fz_alloc_context allocator = {NULL, limited_malloc, limited_realloc, limited_free};
    context = fz_new_context(&allocator, NULL, 2 * 1024 * 1024);
    if (!context) { snprintf(error, sizeof(error), "文档内存不足"); return -1; }
    fz_try(context) {
        fz_register_document_handlers(context);
        document = fz_open_document(context, path);
        if (fz_needs_password(context, document)) fz_throw(context, FZ_ERROR_ARGUMENT, "encrypted document needs a password");
        reflow = fz_is_document_reflowable(context, document);
        if (reflow) fz_layout_document(context, document, READER_WIDTH, READER_HEIGHT, font_sizes[font_step]);
        pages = fz_count_pages(context, document);
        if (pages <= 0 || pages > 100000) fz_throw(context, FZ_ERROR_LIMIT, "invalid or excessive page count");
    }
    fz_catch(context) {
        snprintf(error, sizeof(error), "打开失败：%.110s", fz_caught_message(context));
        fz_report_error(context);
        document_close(); return -1;
    }
    snprintf(mode, sizeof(mode), reflow ? "%u px" : "Fit", font_sizes[font_step]);
    fprintf(stderr, "READER document pages=%d reflow=%d live=%lu\n", pages, reflow, (unsigned long)live);
    return 0;
}
int document_resize_text(int page)
{
    if (!context || !document || !reflow) return page;
    int target = page;
    fz_var(target); fz_var(page);
    fz_try(context) {
        fz_bookmark bookmark = fz_make_bookmark(context, document, fz_location_from_page_number(context, document, page));
        font_step = (font_step + 1) % 3;
        fz_layout_document(context, document, READER_WIDTH, READER_HEIGHT, font_sizes[font_step]);
        pages = fz_count_pages(context, document);
        target = fz_page_number_from_location(context, document, fz_lookup_bookmark(context, document, bookmark));
        if (target < 0) target = 0;
        if (target >= pages) target = pages - 1;
        snprintf(mode, sizeof(mode), "%u px", font_sizes[font_step]);
    }
    fz_catch(context) { snprintf(error, sizeof(error), "排版失败：%.110s", fz_caught_message(context)); fz_report_error(context); return -1; }
    return target;
}
uint16_t *document_render(int number, unsigned *width, unsigned *height)
{
    if (!context || !document || number < 0 || number >= pages) return NULL;
    fz_page *page = NULL;
    fz_pixmap *pixmap = NULL;
    fz_device *device = NULL;
    uint16_t *pixels = NULL;
    fz_var(page); fz_var(pixmap); fz_var(device); fz_var(pixels);
    fz_try(context) {
        page = fz_load_page(context, document, number);
        fz_rect bounds = fz_bound_page(context, page);
        float w = bounds.x1 - bounds.x0, h = bounds.y1 - bounds.y0;
        if (!isfinite(w) || !isfinite(h) || !isfinite(bounds.x0) || !isfinite(bounds.y0) || w <= 0 || h <= 0)
            fz_throw(context, FZ_ERROR_ARGUMENT, "invalid page bounds");
        float scale = fminf(READER_WIDTH / w, READER_HEIGHT / h);
        unsigned out_w = (unsigned)fmaxf(1, fminf(READER_WIDTH, ceilf(w * scale)));
        unsigned out_h = (unsigned)fmaxf(1, fminf(READER_HEIGHT, ceilf(h * scale)));
        fz_irect box = {0, 0, out_w, out_h};
        pixmap = fz_new_pixmap_with_bbox(context, fz_device_rgb(context), box, NULL, 0);
        fz_clear_pixmap_with_value(context, pixmap, 255);
        device = fz_new_draw_device(context, fz_identity, pixmap);
        fz_matrix transform = fz_pre_translate(fz_scale(scale, scale), -bounds.x0, -bounds.y0);
        fz_run_page(context, page, device, transform, NULL);
        fz_close_device(context, device);
        pixels = malloc((size_t)out_w * out_h * 2);
        if (!pixels) fz_throw(context, FZ_ERROR_LIMIT, "screen buffer allocation failed");
        unsigned char *samples = fz_pixmap_samples(context, pixmap);
        int stride = fz_pixmap_stride(context, pixmap);
        for (unsigned y = 0; y < out_h; ++y) for (unsigned x = 0; x < out_w; ++x) {
            unsigned char *rgb = samples + y * stride + x * 3;
            pixels[(size_t)y * out_w + x] = ((rgb[0] & 0xf8) << 8) | ((rgb[1] & 0xfc) << 3) | (rgb[2] >> 3);
        }
        *width = out_w; *height = out_h;
    }
    fz_always(context) {
        fz_drop_device(context, device);
        fz_drop_pixmap(context, pixmap);
        fz_drop_page(context, page);
    }
    fz_catch(context) {
        snprintf(error, sizeof(error), "页面失败：%.110s", fz_caught_message(context));
        fz_report_error(context);
        free(pixels); return NULL;
    }
    return pixels;
}
