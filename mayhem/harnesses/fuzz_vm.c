/* PikaScript source parsed and executed by the VM on the stock PikaMain object. */
#include "pika_fuzz.h"

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    char* src = pf_dup(data, size);
    PikaObj* root = pf_new_root();
    pikaVM_run(root, src);
    obj_deinit(root);
    free(src);
    return 0;
}
