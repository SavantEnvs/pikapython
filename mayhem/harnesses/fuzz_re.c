/* re (PCRE based): the input is "<pattern>\n<subject>"; compile, search and substitute. */
#include "pika_fuzz.h"

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    const uint8_t* nl = (const uint8_t*)memchr(data, '\n', size);
    if (!nl) {
        return -1;
    }
    size_t plen = (size_t)(nl - data);
    char* pattern = pf_dup(data, plen);
    char* subject = pf_dup(nl + 1, size - plen - 1);

    PikaObj* root = pf_new_root();
    obj_setStr(root, "pattern", pattern);
    obj_setStr(root, "subject", subject);
    pikaVM_run(root,
               "import re\n"
               "m = re.search(pattern, subject)\n"
               "g = re.findall(pattern, subject)\n"
               "s = re.sub(pattern, 'x', subject)\n"
               "p = re.compile(pattern)\n"
               "sp = p.split(subject)\n");
    obj_deinit(root);

    free(pattern);
    free(subject);
    return 0;
}
