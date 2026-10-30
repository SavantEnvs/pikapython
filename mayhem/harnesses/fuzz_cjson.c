/* The bundled cJSON: parse, print (formatted and compact), minify, and walk the tree. */
#include "cJSON.h"
#include "pika_fuzz.h"

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    char* text = pf_dup(data, size);

    cJSON* item = cJSON_Parse(text);
    if (item) {
        char* formatted = cJSON_Print(item);
        char* compact = cJSON_PrintUnformatted(item);
        free(formatted);
        free(compact);
        int n = cJSON_GetArraySize(item);
        for (int i = 0; i < n; i++) {
            cJSON_GetArrayItem(item, i);
        }
        cJSON_Delete(item);
    }

    cJSON_Minify(text);
    free(text);
    return 0;
}
