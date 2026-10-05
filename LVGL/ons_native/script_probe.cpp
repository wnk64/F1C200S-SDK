#include "ScriptHandler.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <ctime>

static bool integer(ScriptHandler &script, const char *expression, int expected)
{
    char buffer[128];
    if (std::strlen(expression) >= sizeof(buffer)) return false;
    std::strcpy(buffer, expression);
    script.enterExternalScript(buffer);
    int value = script.readInt();
    script.leaveExternalScript();
    if (value != expected) {
        std::fprintf(stderr, "Expression mismatch: %s got=%d expected=%d\n", expression, value, expected);
        return false;
    }
    return true;
}
int main(int argc, char **argv)
{
    if (argc != 2) { std::fprintf(stderr, "usage: ons-script-probe game-directory\n"); return 1; }
    std::string path(argv[1]);
    if (path.empty() || path.size() > 200) return 1;
    if (path.back() != '/') path += '/';
    ScriptHandler script;
    script.reset();
    script.setKeyTable(NULL);
    std::clock_t begin = std::clock();
    if (script.openScript(&path[0]) < 0) return 1;
    std::printf("Original ONS GBK script opened: %dx%d variables=%d CPU=%ld ms\n",
        script.screen_width, script.screen_height, script.variable_range,
        (long)((std::clock() - begin) * 1000 / CLOCKS_PER_SEC));
    const char *labels[] = {"define", "start"};
    for (unsigned i = 0; i < 2; ++i) {
        ScriptHandler::LabelInfo label = script.lookupLabel(labels[i]);
        if (!label.start_address || label.num_of_lines < 1) return 1;
        script.setCurrent(label.start_address);
        const char *token = script.readToken();
        if (!token) return 1;
        std::printf("Label PASS %s line=%d lines=%d first-token-bytes=%zu\n",
            label.name, label.start_line, label.num_of_lines, std::strlen(token));
    }
    script.setNumVariable(1, 42);
    script.addNumAlias("native_test_value", 17);
    if (!integer(script, "3+4*2", 11) || !integer(script, "%1+2", 44) ||
        !integer(script, "native_test_value*2", 34) || !integer(script, "20/3", 6) ||
        !integer(script, "20 mod 3", 2)) return 1;
    std::puts("Original ONS labels/expressions/variables PASS (no SDL, no save writes)");
    return 0;
}
