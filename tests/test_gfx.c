#include "gfx.h"
#include "test.h"

#include <stdlib.h>
#include <string.h>

static void test_pick(void)
{
    CHECK_INT(gfx_pick(GFX_AUTO, "xterm-kitty", NULL, NULL, NULL), GFX_KITTY);
    CHECK_INT(gfx_pick(GFX_AUTO, "xterm-256color", NULL, "3", NULL), GFX_KITTY);
    CHECK_INT(gfx_pick(GFX_AUTO, "xterm-ghostty", NULL, NULL, NULL), GFX_KITTY);
    CHECK_INT(gfx_pick(GFX_AUTO, "xterm-256color", "ghostty", NULL, NULL), GFX_KITTY);
    CHECK_INT(gfx_pick(GFX_AUTO, "foot", NULL, NULL, NULL), GFX_SIXEL);
    CHECK_INT(gfx_pick(GFX_AUTO, "foot-extra", NULL, NULL, NULL), GFX_SIXEL);
    CHECK_INT(gfx_pick(GFX_AUTO, "xterm-256color", NULL, NULL, NULL), GFX_BRAILLE);
    CHECK_INT(gfx_pick(GFX_AUTO, NULL, NULL, NULL, NULL), GFX_BRAILLE);
    /* tmux would swallow the images */
    CHECK_INT(gfx_pick(GFX_AUTO, "xterm-kitty", NULL, "3", "/tmp/tmux-1000/default,1,0"), GFX_BRAILLE);
    /* the setting wins */
    CHECK_INT(gfx_pick(GFX_SIXEL, "xterm-256color", NULL, NULL, NULL), GFX_SIXEL);
    CHECK_INT(gfx_pick(GFX_KITTY, "foot", NULL, NULL, NULL), GFX_KITTY);
    CHECK_INT(gfx_pick(GFX_BRAILLE, "xterm-kitty", NULL, NULL, NULL), GFX_BRAILLE);
}

static void test_kitty(void)
{
    Sbuf b;
    sb_init(&b);
    const uint8_t px[8] = {255, 0, 0, 255, 0, 0, 0, 0};
    kitty_upload(&b, 7, px, 2, 1);
    CHECK_STR(b.buf, "\033_Ga=t,f=32,s=2,v=1,i=7,q=2,m=0;/wAA/wAAAAA=\033\\");
    sb_free(&b);

    /* big images go in 4096-byte chunks; later chunks carry only m and q */
    sb_init(&b);
    uint8_t *big = calloc(64 * 64, 4);
    kitty_upload(&b, 9, big, 64, 64); /* 16384 bytes -> 21848 base64 -> 6 chunks */
    int n = 0;
    for (const char *p = b.buf; (p = strstr(p, "\033_G")); p++)
        n++;
    CHECK_INT(n, 6);
    const char *head = "\033_Ga=t,f=32,s=64,v=64,i=9,q=2,m=1;";
    CHECK(strncmp(b.buf, head, strlen(head)) == 0);
    CHECK(strstr(b.buf, "\033\\\033_Gm=1,q=2;") != NULL);
    CHECK(strstr(b.buf, "\033\\\033_Gm=0,q=2;") != NULL);
    CHECK(strstr(b.buf, "\033_Gm=1,q=2;AAAA") != NULL);
    free(big);
    sb_free(&b);

    sb_init(&b);
    kitty_place(&b, 7, 3, 5, 4, 10, 100, 50);
    CHECK_STR(b.buf, "\0337\033[4;6H\033_Ga=p,i=7,p=1,X=4,x=10,y=0,w=100,h=50,C=1,z=1,q=2\033\\\0338");
    sb_free(&b);

    sb_init(&b);
    kitty_unplace(&b, 7);
    kitty_free(&b, 8);
    CHECK_STR(b.buf, "\033_Ga=d,d=i,i=7,q=2\033\\\033_Ga=d,d=I,i=8,q=2\033\\");
    sb_free(&b);
}

static void put(uint8_t *img, int w, int x, int y, uint32_t rgb)
{
    uint8_t *p = img + 4 * (y * w + x);
    p[0] = (uint8_t)(rgb >> 16);
    p[1] = (uint8_t)(rgb >> 8);
    p[2] = (uint8_t)rgb;
    p[3] = 255;
}

static void test_sixel(void)
{
    Sbuf b;
    uint8_t img[4 * 5 * 7];

    /* one red pixel, one transparent: trailing empties are trimmed */
    memset(img, 0, sizeof img);
    put(img, 2, 0, 0, 0xFF0000);
    sb_init(&b);
    sixel_encode(&b, img, 2, 0, 0, 2, 1);
    CHECK_STR(b.buf, "\033P0;1;0q\"1;1;2;1#1;2;100;0;0#1@$\033\\");
    sb_free(&b);

    /* runs of 4 or more are compressed; percents round down */
    memset(img, 0, sizeof img);
    for (int x = 0; x < 5; x++)
        put(img, 5, x, 0, 0x0080FF);
    sb_init(&b);
    sixel_encode(&b, img, 5, 0, 0, 5, 1);
    CHECK_STR(b.buf, "\033P0;1;0q\"1;1;5;1#1;2;0;50;100#1!5@$\033\\");
    sb_free(&b);

    /* a 7-row column spans two bands */
    memset(img, 0, sizeof img);
    for (int y = 0; y < 7; y++)
        put(img, 1, 0, y, 0xFF0000);
    sb_init(&b);
    sixel_encode(&b, img, 1, 0, 0, 1, 7);
    CHECK_STR(b.buf, "\033P0;1;0q\"1;1;1;7#1;2;100;0;0#1~$-#1@$\033\\");
    sb_free(&b);

    /* two colors in one band, and an empty column before the second */
    memset(img, 0, sizeof img);
    put(img, 2, 0, 0, 0xFF0000);
    put(img, 2, 1, 1, 0xFFFFFF);
    sb_init(&b);
    sixel_encode(&b, img, 2, 0, 0, 2, 2);
    CHECK_STR(b.buf, "\033P0;1;0q\"1;1;2;2#1;2;100;0;0#2;2;100;100;100#1@$#2?A$\033\\");
    sb_free(&b);

    /* a crop starts at (x0, y0) of the source */
    memset(img, 0, sizeof img);
    put(img, 5, 3, 2, 0xFFFFFF);
    sb_init(&b);
    sixel_encode(&b, img, 5, 3, 2, 2, 1);
    CHECK_STR(b.buf, "\033P0;1;0q\"1;1;2;1#1;2;100;100;100#1@$\033\\");
    sb_free(&b);

    /* nothing opaque: just the frame */
    memset(img, 0, sizeof img);
    sb_init(&b);
    sixel_encode(&b, img, 5, 0, 0, 5, 7);
    CHECK_STR(b.buf, "\033P0;1;0q\"1;1;5;7\033\\");
    sb_free(&b);
}

void suite_gfx(void)
{
    test_pick();
    test_kitty();
    test_sixel();
}
