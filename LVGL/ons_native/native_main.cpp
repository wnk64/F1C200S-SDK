#include "ONScripter.h"
#include <cstdio>
#include <cstdlib>
#include <memory>
static void audio_report()
{
    if (!std::getenv("ONS_NATIVE_AUDIO_DEBUG")) return;
    uint64_t frames; unsigned xruns; ons_mix_Diagnostics(&frames, &xruns);
    std::fprintf(stderr, "Native engine audio frames=%llu underruns=%u\n", (unsigned long long)frames, xruns);
}
int main(int argc, char **argv)
{
    if (argc != 2) { std::fprintf(stderr, "usage: ons-lvgl game-directory\n"); return 1; }
    setenv("ons_rt_DISPLAY_W", "640", 1);
    unsetenv("ONS_F1C200S_DRAW_MOUSE");
    std::atexit(audio_report);
    std::unique_ptr<ONScripter> engine(new ONScripter());
    engine->setArchivePath(argv[1]);
    if (engine->openScript() || engine->init()) return 1;
    engine->executeLabel(); return 0;
}
