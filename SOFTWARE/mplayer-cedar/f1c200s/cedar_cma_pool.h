#ifndef CEDAR_CMA_POOL_H
#define CEDAR_CMA_POOL_H

#include <stddef.h>
#include <sc_interface.h>

/* One decoder per process; enable before the first CdcMemOpen. */
int cedar_cma_pool_enable(struct ScMemOpsS *ops, size_t bytes);

#endif
