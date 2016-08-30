#ifndef __ZIMMERMAN_EVALUATOR__
#define __ZIMMERMAN_EVALUATOR__

#include "obfuscator.h"
#include <acirc.h>

void evaluate (const mmap_vtable *mmap, int *rop, acirc *c, int *inputs, obfuscation *obf);

#endif
