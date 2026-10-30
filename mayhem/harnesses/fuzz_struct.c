/* struct.unpack(): the input is "<format>\n<bytes>". */
#include "pika_fuzz.h"

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    const uint8_t* nl = (const uint8_t*)memchr(data, '\n', size);
    if (!nl) {
        return -1;
    }
    size_t flen = (size_t)(nl - data);
    char* fmt = pf_dup(data, flen);

    PikaObj* root = pf_new_root();
    obj_setStr(root, "fmt", fmt);
    obj_setBytes(root, "buf", (uint8_t*)(nl + 1), size - flen - 1);
    pikaVM_run(root,
               "import struct\n"
               "vals = struct.unpack(fmt, buf)\n");
    obj_deinit(root);

    free(fmt);
    return 0;
}
