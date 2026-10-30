#ifndef PIKA_FUZZ_H
#define PIKA_FUZZ_H
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "PikaMain.h"
#include "PikaVM.h"

extern unsigned char pikaModules_py_a[];

/* Scripts print to stdout; keep it out of the fuzzer's log. */
int LLVMFuzzerInitialize(int* argc, char*** argv) {
    (void)argc;
    (void)argv;
    if (!freopen("/dev/null", "w", stdout)) {
        return 1;
    }
    return 0;
}

/* newRootObj() replaces the SIGSEGV/SIGABRT/... handlers with one that prints and exit(1)s, which would hide
 * every sanitizer report behind a plain exit. Put the sanitizer/fuzzer handlers back after it returns. */
static const int pf_sigs[] = {SIGTERM, SIGHUP, SIGSEGV, SIGABRT, SIGQUIT, SIGTRAP};
#define PF_NSIGS (sizeof(pf_sigs) / sizeof(pf_sigs[0]))

static PikaObj* pf_new_root(void) {
    struct sigaction saved[PF_NSIGS];
    for (size_t i = 0; i < PF_NSIGS; i++) {
        sigaction(pf_sigs[i], NULL, &saved[i]);
    }
    PikaObj* root = newRootObj("pikaMain", New_PikaMain);
    for (size_t i = 0; i < PF_NSIGS; i++) {
        sigaction(pf_sigs[i], &saved[i], NULL);
    }
    obj_linkLibrary(root, pikaModules_py_a);
    return root;
}

static char* pf_dup(const uint8_t* data, size_t size) {
    char* s = (char*)malloc(size + 1);
    memcpy(s, data, size);
    s[size] = '\0';
    return s;
}
#endif
