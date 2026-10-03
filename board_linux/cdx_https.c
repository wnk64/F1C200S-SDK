#include <CdxStream.h>
#include <curl/curl.h>
#include <curl/header.h>
#include <inttypes.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/socket.h>

#define WINDOW (256U * 1024U)
typedef struct {
    CdxStreamT base;
    CURL *curl;
    char *uri;
    unsigned char *buffer;
    unsigned char *next_buffer;
    size_t length;
    size_t next_length;
    int64_t position, start, total;
    int64_t next_start, next_total, requested;
    int pending, busy, next_valid, next_error, shutdown, worker_started;
    pthread_mutex_t mutex;
    pthread_cond_t changed;
    pthread_t worker;
    CdxStreamProbeDataT probe;
    atomic_int cancelled;
    int state;
} HttpsStream;

static int64_t now_us(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
}

static size_t receive(char *data, size_t size, size_t count, void *opaque)
{
    HttpsStream *s = opaque;
    if (size && count > SIZE_MAX / size) return 0;
    size_t bytes = size * count;
    if (bytes > WINDOW - s->next_length) return 0;
    memcpy(s->next_buffer + s->next_length, data, bytes);
    s->next_length += bytes;
    return bytes;
}

static int progress(void *opaque, curl_off_t a, curl_off_t b, curl_off_t c, curl_off_t d)
{
    (void)a; (void)b; (void)c; (void)d;
    return atomic_load(&((HttpsStream *)opaque)->cancelled);
}

static int download_range(HttpsStream *s, int64_t start)
{
    if (start < 0 || atomic_load(&s->cancelled)) return -1;
    int64_t end = start + WINDOW - 1;
    if (s->total >= 0 && end >= s->total) end = s->total - 1;
    if (end < start) return -1;
    char range[80];
    snprintf(range, sizeof(range), "%" PRId64 "-%" PRId64, start, end);
    s->next_length = 0;
    curl_easy_setopt(s->curl, CURLOPT_RANGE, range);
    int64_t begin = now_us();
    CURLcode result = curl_easy_perform(s->curl);
    long status = 0;
    curl_easy_getinfo(s->curl, CURLINFO_RESPONSE_CODE, &status);
    struct curl_header *header = NULL;
    int64_t first, last, total;
    if (result != CURLE_OK || status != 206 ||
        curl_easy_header(s->curl, "Content-Range", 0, CURLH_HEADER, -1, &header) != CURLHE_OK ||
        sscanf(header->value, "bytes %" SCNd64 "-%" SCNd64 "/%" SCNd64, &first, &last, &total) != 3 ||
        first != start || last != (end < total ? end : total - 1) || total <= last || total <= 0 ||
        s->next_length != (size_t)(last - first + 1) ||
        (s->total >= 0 && total != s->total)) {
        fprintf(stderr, "HTTPS range failed: curl=%d status=%ld bytes=%zu\n", result, status, s->next_length);
        s->next_length = 0;
        return -1;
    }
    s->next_total = total;
    fprintf(stderr, "HTTPS fetch start=%" PRId64 " bytes=%zu elapsed_us=%" PRId64 "\n",
            start, s->next_length, now_us() - begin);
    return 0;
}

/* CURL has one owner. The second fixed-size buffer fills while decoding uses the first. */
static void *download(void *opaque)
{
    HttpsStream *s = opaque;
    pthread_mutex_lock(&s->mutex);
    while (!s->shutdown) {
        while (!s->shutdown && (!s->pending || atomic_load(&s->cancelled)))
            pthread_cond_wait(&s->changed, &s->mutex);
        if (s->shutdown) break;
        s->next_start = s->requested;
        s->pending = 0;
        s->busy = 1;
        s->next_valid = 0;
        pthread_mutex_unlock(&s->mutex);
        int result = download_range(s, s->next_start);
        pthread_mutex_lock(&s->mutex);
        s->busy = 0;
        s->next_error = result;
        s->next_valid = result == 0;
        pthread_cond_broadcast(&s->changed);
    }
    pthread_mutex_unlock(&s->mutex);
    return NULL;
}

static int fetch(HttpsStream *s, int64_t position)
{
    int64_t start = position / WINDOW * WINDOW;
    int64_t begin = now_us();
    pthread_mutex_lock(&s->mutex);
    while (s->busy || !s->next_valid || s->next_start != start) {
        if (s->shutdown || atomic_load(&s->cancelled) ||
            (!s->busy && !s->pending && s->next_start == start && s->next_error)) {
            pthread_mutex_unlock(&s->mutex); s->state = CDX_IO_STATE_ERROR; return -1;
        }
        if (!s->busy) {
            s->requested = start;
            s->pending = 1;
            pthread_cond_signal(&s->changed);
        }
        pthread_cond_wait(&s->changed, &s->mutex);
    }
    unsigned char *old = s->buffer;
    s->buffer = s->next_buffer;
    s->next_buffer = old;
    s->length = s->next_length;
    s->start = start;
    s->total = s->next_total;
    s->state = CDX_IO_STATE_OK;
    s->next_valid = 0;
    s->next_error = 0;
    s->requested = start + WINDOW < s->total ? start + WINDOW : 0;
    if (s->requested != start) { s->pending = 1; pthread_cond_signal(&s->changed); }
    pthread_mutex_unlock(&s->mutex);
    int64_t waited = now_us() - begin;
    if (waited > 10000) fprintf(stderr, "HTTPS decoder_wait start=%" PRId64 " wait_us=%" PRId64 "\n", start, waited);
    return 0;
}

static int connect_stream(CdxStreamT *stream)
{
    HttpsStream *s = (HttpsStream *)stream;
    if (pthread_create(&s->worker, NULL, download, s)) return -1;
    s->worker_started = 1;
    if (fetch(s, 0)) return -1;
    s->probe.len = s->length < 2048 ? s->length : 2048;
    s->probe.buf = malloc(s->probe.len);
    if (!s->probe.buf) return -1;
    memcpy(s->probe.buf, s->buffer, s->probe.len);
    fprintf(stderr, "HTTPS async connected: total=%" PRId64 " cache=%u\n", s->total, WINDOW * 2);
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
    atomic_store(&s->cancelled, 1);
    pthread_mutex_lock(&s->mutex);
    s->shutdown = 1;
    pthread_cond_broadcast(&s->changed);
    pthread_mutex_unlock(&s->mutex);
    if (s->worker_started) pthread_join(s->worker, NULL);
    curl_easy_cleanup(s->curl);
    pthread_cond_destroy(&s->changed);
    pthread_mutex_destroy(&s->mutex);
    free(s->probe.buf); free(s->uri); free(s->buffer); free(s->next_buffer); free(s);
    return 0;
}
static int io_state(CdxStreamT *stream) { return ((HttpsStream *)stream)->state; }
static cdx_uint32 attribute(CdxStreamT *stream) { (void)stream; return CDX_STREAM_FLAG_SEEK | CDX_STREAM_FLAG_NET; }
static int control(CdxStreamT *stream, int command, void *parameter)
{
    HttpsStream *s = (HttpsStream *)stream;
    if (command == STREAM_CMD_SET_FORCESTOP || command == STREAM_CMD_CLR_FORCESTOP) {
        atomic_store(&s->cancelled, command == STREAM_CMD_SET_FORCESTOP);
        pthread_mutex_lock(&s->mutex);
        pthread_cond_broadcast(&s->changed);
        pthread_mutex_unlock(&s->mutex);
        return 0;
    }
    if (command == STREAM_CMD_GET_CACHESTATE && parameter) {
        struct StreamCacheStateS *state = parameter;
        memset(state, 0, sizeof(*state));
        state->nCacheCapacity = WINDOW * 2;
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
    int rewind = position == 0 && s->position > s->total / 2;
    s->position = position;
    if (rewind && getenv("F1_OSD_EVENT_FD")) {
        const uint64_t notification = 4;
        (void)send(3, &notification, sizeof(notification), MSG_DONTWAIT | MSG_NOSIGNAL);
        fprintf(stderr, "HTTPS rewind: UI progress reset requested\n");
    }
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
    s->next_start = -1;
    if (pthread_mutex_init(&s->mutex, NULL)) { free(s); return NULL; }
    if (pthread_cond_init(&s->changed, NULL)) { pthread_mutex_destroy(&s->mutex); free(s); return NULL; }
    s->uri = strdup(source->uri);
    s->buffer = malloc(WINDOW);
    s->next_buffer = malloc(WINDOW);
    s->curl = curl_easy_init();
    if (!s->uri || !s->buffer || !s->next_buffer || !s->curl) { close_stream(&s->base); return NULL; }
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
