#include <stdio.h>
#include <stdlib.h>

typedef struct { int x, y, w, h; } Rect;
typedef struct { int x, y, dx, dy; int lives; unsigned score; } Ball;

/* stereo pan from horizontal position: -100 (left) .. 100 (right) */
__attribute__((noinline)) int sound_pan(int x, int screen_w) {
    int pan = (x * 200) / screen_w - 100;
    if (pan < -100) pan = -100;
    if (pan > 100) pan = 100;
    return pan;
}

__attribute__((noinline)) int hits(const Rect *a, const Rect *b) {
    return a->x < b->x + b->w && b->x < a->x + a->w &&
           a->y < b->y + b->h && b->y < a->y + a->h;
}

__attribute__((noinline)) void step(Ball *b, const Rect *paddle, int screen_w, int screen_h) {
    Rect r = { b->x, b->y, 8, 8 };
    b->x += b->dx; b->y += b->dy;
    if (b->x < 0 || b->x > screen_w - 8) b->dx = -b->dx;
    if (b->y < 0) b->dy = -b->dy;
    if (hits(&r, paddle)) { b->dy = -b->dy; b->score += 10 + (b->score / 500) * 5; }
    if (b->y > screen_h) { b->lives--; b->x = screen_w / 2; b->y = screen_h / 2; }
}

int main(int argc, char **argv) {
    int n = argc > 1 ? atoi(argv[1]) : 1000;
    Ball b = { 320, 240, 3, -4, 3, 0 };
    Rect paddle = { 280, 440, 80, 12 };
    for (int i = 0; i < n && b.lives > 0; i++) {
        step(&b, &paddle, 640, 480);
        if (i % 100 == 0) printf("pan=%d score=%u lives=%d\n", sound_pan(b.x, 640), b.score, b.lives);
    }
    return 0;
}
