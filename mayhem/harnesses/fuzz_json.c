/* json.loads() on arbitrary text, then json.dumps() of whatever it produced. */
#include "pika_fuzz.h"

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    char* text = pf_dup(data, size);
    PikaObj* root = pf_new_root();
    obj_setStr(root, "text", text);
    pikaVM_run(root,
               "import json\n"
               "obj = json.loads(text)\n"
               "out = json.dumps(obj)\n");
    obj_deinit(root);
    free(text);
    return 0;
}
