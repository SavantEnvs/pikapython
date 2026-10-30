/* PikaScript source -> assembly and bytecode (the compiler front end, no execution). */
#include "PikaParser.h"
#include "pika_fuzz.h"

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    char* src = pf_dup(data, size);

    Args buffs = {0};
    pika_lines2Asm(&buffs, src);
    strsDeinit(&buffs);

    ByteCodeFrame bf;
    byteCodeFrame_init(&bf);
    pika_lines2Bytes(&bf, src);
    byteCodeFrame_deinit(&bf);

    free(src);
    return 0;
}
