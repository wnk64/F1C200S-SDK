#include "ONScripter.h"
#include <cstdio>
#include <cstdlib>
#include <memory>
int main(int argc, char **argv)
{
    if (argc != 2) { std::fprintf(stderr, "usage: ons-lvgl game-directory\n"); return 1; }
    setenv("ons_rt_DISPLAY_W", "640", 1);
    unsetenv("ONS_F1C200S_DRAW_MOUSE");
    std::unique_ptr<ONScripter> engine(new ONScripter());
    engine->setArchivePath(argv[1]);
    if (engine->openScript() || engine->init()) return 1;
    engine->executeLabel(); return 0;
}
