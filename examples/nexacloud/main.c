#include "scene.h"
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc != 4 || strcmp(argv[1], "--smoke") != 0) {
        fputs("usage: nexaui_nexacloud --smoke FONT.ttf OUTPUT_PREFIX\n", stderr); return 2;
    }
    cloud_scene *scene = cloud_create(argv[2]);
    if (!scene) { fputs("Cannot open the TrueType font.\n", stderr); return 1; }
    int okay = cloud_smoke(scene, argv[3]); cloud_destroy(scene);
    return okay ? 0 : 1;
}
