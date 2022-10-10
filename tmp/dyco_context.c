
#include "dyco_context.h"

void _dyco_getcontext(dycoctx *ctx);
void _dyco_setcontext(const dycoctx *ctx);
void _dyco_makecontext(dycoctx *ctx, void (*__func) (void *), void *__arg); // only 1 argument, no ret
void _dyco_swapcontext(dycoctx *oldctx, const dycoctx *newctx);