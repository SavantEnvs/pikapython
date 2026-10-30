#include <stdlib.h>
#include "PikaObj.h"

/* The compiler-generated pikaScript.c normally defines this; we start the VM ourselves instead. */
volatile PikaObj* __pikaMain;

/* PikaPlatform.c defines this weak hook as `while (1);`, so a failed pika_assert hangs the process.
 * A real port resets the chip here; on Linux the equivalent is to abort so the fault is a crash. */
void pika_platform_panic_handle(void) {
    abort();
}
