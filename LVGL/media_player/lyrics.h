#ifndef MEDIA_LYRICS_H
#define MEDIA_LYRICS_H
int lyrics_load(const char *media);
int lyrics_index(double seconds);
const char *lyrics_text(int index);
void lyrics_close(void);
int lyrics_selftest(void);
#endif
