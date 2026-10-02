#include "sprite.h"

/* W snow, S frost shade, E eye, N claws/nose. Drawn after the wolves in the
   rune pixel-art the user picked; the legs cycle stretch, gather, collect, push. */
static const char *const WOLF[WOLF_FRAMES][WOLF_H] = {
    {
        ".......................W..W.....",
        "......................WW.WW.....",
        ".........SSSSS.......WWWWWW.....",
        ".....SSSSSSSSSSSSSSSWWWWWEWW....",
        "...WWWWWWWWWWWWWWWWWWWWWWWWWWWN.",
        ".WWWWWWWWWWWWWWWWWWWWWWWWWW.....",
        "WWWWWWWWWWWWWWWWWWWWWWWWWW......",
        "WWW..WWWWWWWWWWWWWWWWWWWWW......",
        "WW....WWWWWWWW.....WWWWWW.......",
        ".....WWW............WWW.........",
        "....WW.WW............WW.WW......",
        "...WW...WW..........WW...WW.....",
        "..WW.....WW........WW.....WW....",
        "..N.......N........N.......N....",
    },
    {
        ".......................W..W.....",
        "......................WW.WW.....",
        ".........SSSSS.......WWWWWW.....",
        ".....SSSSSSSSSSSSSSSWWWWWEWW....",
        "...WWWWWWWWWWWWWWWWWWWWWWWWWWWN.",
        ".WWWWWWWWWWWWWWWWWWWWWWWWWW.....",
        "WWWWWWWWWWWWWWWWWWWWWWWWWW......",
        "WWW..WWWWWWWWWWWWWWWWWWWWW......",
        "WW....WWWWWWWW.....WWWWWW.......",
        "......WWW..........WWW..........",
        "......WW.W.........WW.W.........",
        ".....WW..WW.......WW..WW........",
        ".....W....W.......W....W........",
        ".....N....N.......N....N........",
    },
    {
        ".......................W..W.....",
        "......................WW.WW.....",
        ".........SSSSS.......WWWWWW.....",
        ".....SSSSSSSSSSSSSSSWWWWWEWW....",
        "...WWWWWWWWWWWWWWWWWWWWWWWWWWWN.",
        ".WWWWWWWWWWWWWWWWWWWWWWWWWW.....",
        "WWWWWWWWWWWWWWWWWWWWWWWWWW......",
        "WWW..WWWWWWWWWWWWWWWWWWWWW......",
        "WW....WWWWWWWW.....WWWWWW.......",
        "........WWW.....WWW.............",
        "........WWWW...WWWW.............",
        ".........WWW...WWW..............",
        ".........WW....WW...............",
        ".........NN....NN...............",
    },
    {
        ".......................W..W.....",
        "......................WW.WW.....",
        ".........SSSSS.......WWWWWW.....",
        ".....SSSSSSSSSSSSSSSWWWWWEWW....",
        "...WWWWWWWWWWWWWWWWWWWWWWWWWWWN.",
        ".WWWWWWWWWWWWWWWWWWWWWWWWWW.....",
        "WWWWWWWWWWWWWWWWWWWWWWWWWW......",
        "WWW..WWWWWWWWWWWWWWWWWWWWW......",
        "WW....WWWWWWWW.....WWWWWW.......",
        "......WWW..........WWWW.........",
        ".....WW.WW..........WW.WW.......",
        "....WW...WW........WW...WW......",
        "...WW.....W.......WW.....WW.....",
        "...N......N.......N.......N.....",
    },
};

int wolf_pixel(int frame, int x, int y)
{
    if (x < 0 || y < 0 || x >= WOLF_W || y >= WOLF_H)
        return PX_NONE;
    frame %= WOLF_FRAMES;
    if (frame < 0)
        frame += WOLF_FRAMES;
    switch (WOLF[frame][y][x]) {
    case 'W': return PX_SNOW;
    case 'S': return PX_SHADE;
    case 'E': return PX_EYE;
    case 'N': return PX_DARK;
    default: return PX_NONE;
    }
}

uint32_t rune(int i)
{
    static const uint32_t FUTHARK[RUNE_COUNT] = {
        0x16A0, 0x16A2, 0x16A6, 0x16A8, 0x16B1, 0x16B2, 0x16B7, 0x16B9, /* ᚠᚢᚦᚨᚱᚲᚷᚹ */
        0x16BA, 0x16BE, 0x16C1, 0x16C3, 0x16C7, 0x16C8, 0x16C9, 0x16CA, /* ᚺᚾᛁᛃᛇᛈᛉᛊ */
        0x16CF, 0x16D2, 0x16D6, 0x16D7, 0x16DA, 0x16DC, 0x16DE, 0x16DF, /* ᛏᛒᛖᛗᛚᛜᛞᛟ */
    };
    i %= RUNE_COUNT;
    return FUTHARK[i < 0 ? i + RUNE_COUNT : i];
}
