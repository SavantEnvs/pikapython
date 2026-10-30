/* Runs one PikaScript file on the stock PikaMain object (the functional oracle's runner). */
#include <stdio.h>
#include <stdlib.h>

#include "PikaMain.h"
#include "PikaVM.h"

int main(int argc, char** argv) {
    if (argc != 2) {
        return 2;
    }
    FILE* f = fopen(argv[1], "rb");
    if (!f) {
        return 2;
    }
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* src = (char*)malloc((size_t)n + 1);
    if (fread(src, 1, (size_t)n, f) != (size_t)n) {
        return 2;
    }
    src[n] = '\0';
    fclose(f);

    PikaObj* root = newRootObj("pikaMain", New_PikaMain);
    extern unsigned char pikaModules_py_a[];
    obj_linkLibrary(root, pikaModules_py_a);
    printf("---BEGIN---\n");
    fflush(stdout);
    pikaVM_run(root, src);
    fflush(stdout);
    obj_deinit(root);
    free(src);
    return 0;
}
