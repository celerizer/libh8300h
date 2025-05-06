#include "config.h"
#include "dma.h"
#include "types.h"

#if H8_NO_DMA
static h8_u8 h8_heap[H8_NO_DMA_SIZE];
static unsigned h8_heap_alloc = 0;

static void (*h8_dma_oom_cb)(void) = NULL;
#else
#include <stdlib.h>
#endif

void *h8_dma_alloc(unsigned size, unsigned zero)
{
/**
 * Implements very simple DMA for embedded systems that have no access to
 * malloc. Uses a static-sized heap, allocates purely linearly, and does not
 * actually free values. Use only if absolutely necessary.
 */
#if H8_NO_DMA
  if (size + h8_heap_alloc >= H8_NO_DMA_SIZE)
  {
    if (h8_dma_oom_cb)
      h8_dma_oom_cb();

    return NULL;
  }
  else
  {
    h8_u8 *allocated_value = &h8_heap[h8_heap_alloc];

    if (zero)
    {
      unsigned offset;

      for (offset = h8_heap_alloc; offset < h8_heap_alloc + size; offset++)
        h8_heap[offset] = 0;
    }
    h8_heap_alloc += size;

    return allocated_value;
  }
#else
  return zero ? calloc(size, 1) : malloc(size);
#endif
}

void h8_dma_free(void *value)
{
#if H8_NO_DMA
  H8_UNUSED(value);
#else
  free(value);
#endif
}

void h8_dma_set_oom_cb(void (*cb)(void))
{
#if H8_NO_DMA
  h8_dma_oom_cb = cb;
#else
  H8_UNUSED(cb);
#endif
}
