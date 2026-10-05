#include "lyrics.h"
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LINES 512
#define MAX_BYTES 65536
static struct lyric { double time; unsigned order; char *text; } lines[MAX_LINES];
static unsigned count, bytes;
static double offset;
void lyrics_close(void)
{
    for (unsigned i = 0; i < count; ++i) free(lines[i].text);
    count = bytes = 0; offset = 0;
}
static int compare(const void *a, const void *b)
{
    const struct lyric *x = a, *y = b;
    if (x->time != y->time) return x->time > y->time ? 1 : -1;
    return x->order > y->order ? 1 : x->order < y->order ? -1 : 0;
}
static void parse(char *text)
{
    double stamps[32]; unsigned used = 0;
    if (!strncmp(text, "[offset:", 8)) {
        char *end; double value = strtod(text + 8, &end);
        if (*end == ']' && isfinite(value) && fabs(value) <= 86400000) offset = value / 1000;
        return;
    }
    while (*text == '[' && used < 32) {
        char *end;
        if (!isdigit((unsigned char)text[1])) break;
        long minutes = strtol(text + 1, &end, 10);
        if (*end != ':' || minutes > 1440 || !isdigit((unsigned char)end[1])) break;
        double seconds = strtod(end + 1, &end);
        if (*end != ']' || !isfinite(seconds) || seconds < 0 || seconds >= 60) break;
        stamps[used++] = minutes * 60 + seconds; text = end + 1;
    }
    if (!used) return;
    text[strcspn(text, "\r\n")] = 0;
    size_t length = strlen(text) + 1;
    for (unsigned i = 0; i < used && count < MAX_LINES && bytes + length <= MAX_BYTES; ++i) {
        char *copy = strdup(text); if (!copy) break;
        lines[count] = (struct lyric){stamps[i], count, copy}; ++count; bytes += length;
    }
}
int lyrics_load(const char *media)
{
    lyrics_close();
    char path[1024]; if (strlen(media) >= sizeof(path) - 5) return 0;
    strcpy(path, media); char *slash = strrchr(path, '/'), *dot = strrchr(path, '.');
    if (dot && (!slash || dot > slash)) *dot = 0;
    strcat(path, ".lrc"); FILE *file = fopen(path, "r"); if (!file) return 0;
    char text[2048]; unsigned total = 0;
    while (fgets(text, sizeof(text), file) && total < MAX_BYTES * 2) {
        total += strlen(text);
        if (!strchr(text, '\n') && !feof(file)) {
            int ch; while ((ch = fgetc(file)) != EOF && ch != '\n') {
                if (++total >= MAX_BYTES * 2) break;
            }
            continue;
        }
        char *start = text;
        if (strlen(start) >= 3 && (unsigned char)start[0] == 0xef && (unsigned char)start[1] == 0xbb && (unsigned char)start[2] == 0xbf) start += 3;
        parse(start);
    }
    fclose(file); qsort(lines, count, sizeof(*lines), compare);
    return (int)count;
}
int lyrics_index(double seconds)
{
    int current = -1;
    for (unsigned i = 0; i < count && lines[i].time <= seconds + offset; ++i) current = i;
    return current;
}
const char *lyrics_text(int index)
{
    return index >= 0 && (unsigned)index < count ? lines[index].text : "";
}
int lyrics_selftest(void)
{
    lyrics_close();
    char a[] = "[00:02.50][00:04.00]second", b[] = "[00:01]first", c[] = "[offset:500]", bad[] = "[00:99]bad";
    parse(a); parse(b); parse(c); parse(bad); qsort(lines, count, sizeof(*lines), compare);
    int ok = count == 3 && lyrics_index(0.49) == -1 && lyrics_index(0.5) == 0 &&
        !strcmp(lyrics_text(0), "first") && lyrics_index(2.0) == 1 && lyrics_index(3.5) == 2 &&
        lyrics_index(0.0) == -1 && !strcmp(lyrics_text(99), "");
    lyrics_close(); return ok ? 0 : 1;
}
