/* Rewritten only from the REA/Ghidra pseudocode of game_stripped.exe */
#include <stdio.h>
#include <stdlib.h>

typedef struct { int x, y, w, h; } Box;                       /* FUN_140001480 reads [0..3] */
typedef struct { int x, y, dx, dy, lives; unsigned score; } Obj; /* offsets 0,4,8,0xc,0x10,0x14 */

static int fun_1450(int a, int b) {
    int v = (a * 200) / b - 100;
    if (v > 100) v = 100;
    if (v < -100) v = -100;
    return v;
}

static int fun_1480(const Box *a, const Box *b) {
    if (a->x < b->w + b->x && b->x < a->x + a->w)
        if (a->y < b->h + b->y) return b->y < a->y + a->h;
    return 0;
}

static void fun_14c0(Obj *o, const Box *p, int w, int h) {
    Box old = { o->x, o->y, 8, 8 };
    int nx = o->x + o->dx, ny = o->y + o->dy;
    o->x = nx; o->y = ny;
    if (nx < 0 || w - 7 <= nx) o->dx = -o->dx;
    if (ny < 0) o->dy = -o->dy;
    if (fun_1480(&old, p)) { o->dy = -o->dy; o->score = o->score + 10 + (o->score / 500) * 5; }
    if (h < ny) { o->lives -= 1; o->x = w / 2; o->y = h / 2; }
}

int main(int argc, char **argv) {
    int n = 1000;
    if (argc >= 2) { n = atoi(argv[1]); if (n < 1) return 0; }
    Box p = { 0x118, 0x1b8, 0x50, 0xc };
    Obj o = { 0x140, 0xf0, 3, -4, 3, 0 };
    int i = 0;
    do {
        fun_14c0(&o, &p, 0x280, 0x1e0);
        if (i % 100 == 0) printf("pan=%d score=%u lives=%d\n", fun_1450(o.x, 0x280), o.score, o.lives);
    } while (i + 1 != n && (i = i + 1, 0 < o.lives));
    return 0;
}
