/* Generate the demo's default hero sprite as an 8x8 RGBA8 raw PNG-less .bin.
 *
 * Output bytes are straight RGBA8 (4 bytes/pixel), row-major, top-down.
 * No header: the loader knows the dimensions (8x8, 4 channels) from context.
 *
 * Usage:
 *   ky_mk_demo_asset [path]
 * Defaults to "assets/hero_8x8.bin" relative to the current working
 * directory. Run from the repo root or pass an absolute path.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define W 8
#define H 8

static int write_asset(const char *path) {
    FILE *f = fopen(path, "wb");
    if (!f) {
        fprintf(stderr, "cannot open %s for write\n", path);
        return 1;
    }
    /* Red/white checkerboard with a darker border. */
    static uint8_t px[W * H * 4];
    memset(px, 0, sizeof(px));
    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            uint8_t *p = &px[(y * W + x) * 4];
            int border = (x == 0 || y == 0 || x == W - 1 || y == H - 1);
            if (border) {
                p[0] = 20;  p[1] = 20;  p[2] = 20;
            } else if (((x + y) & 1) == 0) {
                p[0] = 240; p[1] = 70;  p[2] = 70;
            } else {
                p[0] = 255; p[1] = 255; p[2] = 255;
            }
            p[3] = 255;
        }
    }
    size_t n = W * H * 4;
    if (fwrite(px, 1, n, f) != n) {
        fprintf(stderr, "short write to %s\n", path);
        fclose(f);
        return 1;
    }
    if (fclose(f) != 0) {
        fprintf(stderr, "fclose failed for %s\n", path);
        return 1;
    }
    printf("wrote %zu bytes -> %s\n", n, path);
    return 0;
}

int main(int argc, char **argv) {
    const char *path = (argc > 1) ? argv[1] : "assets/hero_8x8.bin";
    return write_asset(path);
}
