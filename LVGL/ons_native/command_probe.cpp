#include "ScriptParser.h"
#include "engine_types.h"
#include <cstdio>
#include <cstring>
#include <vector>
#include <string>
#include <cassert>

class NativeCommands : public ScriptParser {
public:
    explicit NativeCommands(const char *directory)
    {
        script_h.reset(); script_h.setKeyTable(NULL); reset();
        std::string root(directory); if (root.back() != '/') root += '/';
        setStr(&archive_path, root.c_str());
        if (openScript()) throw std::string("Failed to open original script");
        setCurrentLabel("define");
    }
    ~NativeCommands() { delete script_h.cBR; script_h.cBR = NULL; }
    void execute(const char *command, int (ScriptParser::*method)())
    {
        char text[256]; assert(std::strlen(command) < sizeof(text)); std::strcpy(text, command);
        script_h.enterExternalScript(text);
        script_h.readToken();
        (this->*method)(); script_h.leaveExternalScript();
    }
    int number(int id) { return script_h.getVariableData(id).num; }
    const char *string(int id) { return script_h.getVariableData(id).str; }
    void tests()
    {
        execute("mov %1,7", &ScriptParser::movCommand); assert(number(1) == 7);
        execute("add %1,5", &ScriptParser::addCommand); assert(number(1) == 12);
        execute("sub %1,2", &ScriptParser::subCommand); assert(number(1) == 10);
        execute("mul %1,3", &ScriptParser::mulCommand); assert(number(1) == 30);
        execute("div %1,4", &ScriptParser::divCommand); assert(number(1) == 7);
        execute("mov $2,\"native\"", &ScriptParser::movCommand);
        execute("add $2,\"-ons\"", &ScriptParser::addCommand); assert(!std::strcmp(string(2), "native-ons"));
        execute("len %3,$2", &ScriptParser::lenCommand); assert(number(3) == 10);
        execute("numalias native_value,21", &ScriptParser::numaliasCommand);
        execute("mov %4,native_value*2", &ScriptParser::movCommand); assert(number(4) == 42);
        setCurrentLabel("start"); assert(!std::strcmp(current_label_info.name, "start"));
        std::printf("Original command arithmetic/string/alias/label PASS mode=%dx%d\n", screen_width, screen_height);
        execute("nsa", &ScriptParser::nsaCommand);
        const char *names[] = {"cg/sayajpgfirstedition10244.png", "cg/000.jpg", "data/line.bmp"};
        for (unsigned i = 0; i < 3; ++i) {
            size_t length = script_h.cBR->getFileLength(names[i]);
            assert(length > 0 && length < 8388608);
            std::vector<unsigned char> bytes(length);
            assert(script_h.cBR->getFile(names[i], &bytes[0]) == length);
            ons_surface *image = ons_image_decode(&bytes[0], bytes.size());
            assert(image && image->w == 800 && image->h == 600);
            std::printf("Original NSA -> native decoder PASS %s %dx%d\n", names[i], image->w, image->h);
            ons_surface_free(image);
        }
    }
};
int main(int argc, char **argv)
{
    if (argc != 2 || !argv[1][0]) { std::fprintf(stderr, "usage: ons-command-probe game-directory\n"); return 1; }
    try { NativeCommands commands(argv[1]); commands.tests(); }
    catch (const std::string &error) { std::fprintf(stderr, "%s\n", error.c_str()); return 1; }
    std::puts("Native original command/reader integration PASS (no SDL, no saves)"); return 0;
}
