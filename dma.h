#ifndef H8_DMA_H
#define H8_DMA_H

void *h8_dma_alloc(unsigned size, unsigned zero);

void h8_dma_free(void *value);

void h8_dma_set_oom_cb(void (*cb)(void));

#endif
