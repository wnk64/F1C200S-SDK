extern "C" {
#include "native.h"
}
unsigned short convGBK2UTF16(unsigned short input)
{
    char text[3] = {(char)(input >> 8), (char)input, 0};
    uint32_t code;
    return ons_gbk_decode(text, &code, 1) == 1 ? (unsigned short)code : 0x25a1;
}
int convUTF16ToUTF8(unsigned char output[4], unsigned short input)
{
    if (input >= 0xd800 && input <= 0xdfff) input = 0xfffd;
    int count;
    if (input < 0x80) { output[0] = input; count = 1; }
    else if (input < 0x800) {
        output[0] = 0xc0 | (input >> 6); output[1] = 0x80 | (input & 63); count = 2;
    } else {
        output[0] = 0xe0 | (input >> 12); output[1] = 0x80 | ((input >> 6) & 63);
        output[2] = 0x80 | (input & 63); count = 3;
    }
    output[count] = 0; return count;
}
