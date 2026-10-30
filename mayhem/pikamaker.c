/* Compiles the PikaScript modules of the build workspace into pikascript-api/pikaModules.py.a and
 * its C array (__asset_pikaModules_py_a.c). Same steps the upstream pika compiler runs after binding. */
#include "PikaCompiler.h"
#include "PikaObj.h"

int main(void) {
    PikaMaker* maker = New_PikaMaker();
    PIKA_RES res = pikaMaker_compileModuleWithDepends(maker, "main");
    if (PIKA_RES_OK == res) {
        res = pikaMaker_linkCompiledModules(maker, "pikaModules.py.a");
    }
    pikaMaker_deinit(maker);
    return PIKA_RES_OK == res ? 0 : 1;
}
