#include <CdxStream.h>
#include <curl/curl.h>
#include <curl/header.h>
#include <inttypes.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WINDOW (256U * 1024U)
typedef struct {
    CdxStreamT base;
    CURL *curl;
    char *uri;
    unsigned char *buffer;
    size_t length;
    int64_t position, start, total;
    CdxStreamProbeDataT probe;
    atomic_int cancelled;
    int state;
} HttpsStream;

static size_t receive(char *data, size_t size, size_t count, void *opaque)
{
    HttpsStream *s = opaque;
    if (size && count > SIZE_MAX / size) return 0;
    size_t bytes = size * count;
    if (bytes > WINDOW - s->length) return 0;
    memcpy(s->buffer + s->length, data, bytes);
    s->length += bytes;
    return bytes;
}

static int progress(void *opaque, curl_off_t a, curl_off_t b, curl_off_t c, curl_off_t d)
{
    (void)a; (void)b; (void)c; (void)d;
    return atomic_load(&((HttpsStream *)opaque)->cancelled);
}

static int fetch(HttpsStream *s, int64_t start)
{
    if (start < 0 || atomic_load(&s->cancelled)) return -1;
    int64_t end = start + WINDOW - 1;
    if (s->total >= 0 && end >= s->total) end = s->total - 1;
    if (end < start) return -1;
    char range[80];
    snprintf(range, sizeof(range), "%" PRId64 "-%" PRId64, start, end);
    s->length = 0;
    curl_easy_setopt(s->curl, CURLOPT_RANGE, range);
    CURLcode result = curl_easy_perform(s->curl);
    long status = 0;
    curl_easy_getinfo(s->curl, CURLINFO_RESPONSE_CODE, &status);
    struct curl_header *header = NULL;
    int64_t first, last, total;
    if (result != CURLE_OK || status != 206 ||
        curl_easy_header(s->curl, "Content-Range", 0, CURLH_HEADER, -1, &header) != CURLHE_OK ||
        sscanf(header->value, "bytes %" SCNd64 "-%" SCNd64 "/%" SCNd64, &first, &last, &total) != 3 ||
        first != start || last != (end < total ? end : total - 1) || total <= last || total <= 0 ||
        s->length != (size_t)(last - first + 1) ||
        (s->total >= 0 && total != s->total)) {
        fprintf(stderr, "HTTPS range failed: curl=%d status=%ld bytes=%zu\n", result, status, s->length);
        s->length = 0;
        s->state = CDX_IO_STATE_ERROR;
        return -1;
    }
    s->start = start;
    s->total = total;
    s->state = CDX_IO_STATE_OK;
    return 0;
}

static int connect_stream(CdxStreamT *stream)
{
    HttpsStream *s = (HttpsStream *)stream;
    if (fetch(s, 0)) return -1;
    s->probe.len = s->length < 2048 ? s->length : 2048;
    s->probe.buf = malloc(s->probe.len);
    if (!s->probe.buf) return -1;
    memcpy(s->probe.buf, s->buffer, s->probe.len);
    fprintf(stderr, "HTTPS range connected: total=%" PRId64 " cache=%u\n", s->total, WINDOW);
    return 0;
}

static CdxStreamProbeDataT *probe_data(CdxStreamT *stream)
{
    return &((HttpsStream *)stream)->probe;
}

static int read_stream(CdxStreamT *stream, void *buffer, cdx_uint32 length)
{
    HttpsStream *s = (HttpsStream *)stream;
    size_t copied = 0;
    if (length > INT32_MAX || s->total < 0) return -1;
    while (copied < length && s->position < s->total) {
        if (atomic_load(&s->cancelled)) return copied ? (int)copied : -1;
        if (!s->length || s->position < s->start || s->position >= s->start + (int64_t)s->length) {
            if (fetch(s, s->position)) return copied ? (int)copied : -1;
        }
        size_t offset = s->position - s->start;
        size_t bytes = s->length - offset;
        if (bytes > length - copied) bytes = length - copied;
        memcpy((unsigned char *)buffer + copied, s->buffer + offset, bytes);
        copied += bytes;
        s->position += bytes;
    }
    if (s->position == s->total) s->state = CDX_IO_STATE_EOS;
    return copied;
}

static int close_stream(CdxStreamT *stream)
{
    HttpsStream *s = (HttpsStream *)stream;
    curl_easy_cleanup(s->curl);
    free(s->probe.buf); free(s->uri); free(s->buffer); free(s);
    return 0;
}
static int io_state(CdxStreamT *stream) { return ((HttpsStream *)stream)->state; }
static cdx_uint32 attribute(CdxStreamT *stream) { (void)stream; return CDX_STREAM_FLAG_SEEK | CDX_STREAM_FLAG_NET; }
static int control(CdxStreamT *stream, int command, void *parameter)
{
    HttpsStream *s = (HttpsStream *)stream;
    if (command == STREAM_CMD_SET_FORCESTOP || command == STREAM_CMD_CLR_FORCESTOP) {
        atomic_store(&s->cancelled, command == STREAM_CMD_SET_FORCESTOP);
        return 0;
    }
    if (command == STREAM_CMD_GET_CACHESTATE && parameter) {
        struct StreamCacheStateS *state = parameter;
        memset(state, 0, sizeof(*state));
        state->nCacheCapacity = WINDOW;
        state->nCacheSize = s->length;
        return 0;
    }
    if (command == STREAM_CMD_SET_CALLBACK || command == STREAM_CMD_SET_PROBE_SIZE) return 0;
    return -1;
}
static int seek_stream(CdxStreamT *stream, int64_t offset, int whence)
{
    HttpsStream *s = (HttpsStream *)stream;
    int64_t base = whence == SEEK_SET ? 0 : whence == SEEK_CUR ? s->position : whence == SEEK_END ? s->total : -1;
    if (base < 0 || (offset > 0 && base > INT64_MAX - offset) || offset < -base) return -1;
    int64_t position = base + offset;
    if (position > s->total) return -1;
    s->position = position;
    s->state = position == s->total ? CDX_IO_STATE_EOS : CDX_IO_STATE_OK;
    return 0;
}
static cdx_bool eos(CdxStreamT *stream) { HttpsStream *s = (HttpsStream *)stream; return s->total >= 0 && s->position == s->total; }
static int64_t tell_stream(CdxStreamT *stream) { return ((HttpsStream *)stream)->position; }
static int64_t size_stream(CdxStreamT *stream) { return ((HttpsStream *)stream)->total; }
static struct CdxStreamOpsS ops = {
    .connect = connect_stream, .getProbeData = probe_data, .read = read_stream,
    .close = close_stream, .getIOState = io_state, .attribute = attribute, .control = control,
    .seek = seek_stream, .eos = eos, .tell = tell_stream, .size = size_stream
};

static CdxStreamT *create_stream(CdxDataSourceT *source)
{
    const char *ca = getenv("F1_CA_FILE");
    if (!source || !source->uri || strncmp(source->uri, "https://", 8) || !ca || !*ca) return NULL;
    HttpsStream *s = calloc(1, sizeof(*s));
    if (!s) return NULL;
    s->base.ops = &ops;
    s->total = -1;
    s->state = CDX_IO_STATE_INVALID;
    atomic_init(&s->cancelled, 0);
    s->uri = strdup(source->uri);
    s->buffer = malloc(WINDOW);
    s->curl = curl_easy_init();
    if (!s->uri || !s->buffer || !s->curl) { close_stream(&s->base); return NULL; }
    curl_easy_setopt(s->curl, CURLOPT_URL, s->uri);
    curl_easy_setopt(s->curl, CURLOPT_PROTOCOLS_STR, "https");
    curl_easy_setopt(s->curl, CURLOPT_REDIR_PROTOCOLS_STR, "https");
    curl_easy_setopt(s->curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(s->curl, CURLOPT_MAXREDIRS, 3L);
    curl_easy_setopt(s->curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(s->curl, CURLOPT_SSL_VERIFYHOST, 2L);
    curl_easy_setopt(s->curl, CURLOPT_CAINFO, ca);
    curl_easy_setopt(s->curl, CURLOPT_USERAGENT, "Mozilla/5.0");
    curl_easy_setopt(s->curl, CURLOPT_REFERER, "https://www.bilibili.com/");
    curl_easy_setopt(s->curl, CURLOPT_CONNECTTIMEOUT, 10L);
    curl_easy_setopt(s->curl, CURLOPT_TIMEOUT, 20L);
    curl_easy_setopt(s->curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(s->curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(s->curl, CURLOPT_XFERINFOFUNCTION, progress);
    curl_easy_setopt(s->curl, CURLOPT_XFERINFODATA, s);
    curl_easy_setopt(s->curl, CURLOPT_WRITEFUNCTION, receive);
    curl_easy_setopt(s->curl, CURLOPT_WRITEDATA, s);
    return &s->base;
}

static CdxStreamCreatorT creator = { .create = create_stream };
static void register_https(void) __attribute__((constructor));
static void register_https(void)
{
    if (curl_global_init(CURL_GLOBAL_DEFAULT) == CURLE_OK) AwStreamRegister(&creator, "https");
}
