#include "FontInfo.h"
#include "font.h"
#include <string>

struct FontEntry {
    std::string path; int size; ons_font *faces[2];
    FontEntry() : size(0) { faces[0] = faces[1] = NULL; }
    ~FontEntry() { ons_font_close(faces[0]); ons_font_close(faces[1]); }
};
static FontEntry cache[16];
void *FontInfo::openFont(char *path, int ratio1, int ratio2)
{
    if (!path || ratio1 < 1 || ratio2 < 1) return NULL;
    int size = (font_size_xy[0] < font_size_xy[1] ? font_size_xy[0] : font_size_xy[1]);
    int64_t scaled = (int64_t)size * ratio1 / ratio2;
    if (scaled < 1 || scaled > 128) return NULL;
    FontEntry *entry = NULL;
    for (unsigned i = 0; i < 16; ++i) {
        if (cache[i].faces[0] && cache[i].path == path && cache[i].size == scaled) { entry = &cache[i]; break; }
        if (!cache[i].faces[0] && !entry) entry = &cache[i];
    }
    if (!entry) return NULL;
    if (!entry->faces[0]) {
        ons_font *normal = ons_font_open(path, scaled), *outline = ons_font_open(path, scaled);
        if (!normal || !outline) { ons_font_close(normal); ons_font_close(outline); return NULL; }
        entry->path = path; entry->size = scaled; entry->faces[0] = normal; entry->faces[1] = outline;
        ons_font_outline(outline, 1);
    }
    ttf_font[0] = entry->faces[0]; ttf_font[1] = entry->faces[1];
    return entry->faces;
}
