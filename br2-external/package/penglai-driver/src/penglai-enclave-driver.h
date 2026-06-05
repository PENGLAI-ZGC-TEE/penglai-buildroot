#ifndef _PENGLAI_ENCLAVE_DRIVER
#define _PENGLAI_ENCLAVE_DRIVER

#define DEFAULT_SECURE_PAGES_ORDER 10
/* the size of cma area */
#define CMA_SHIFT	28
#define CMA_SIZE    (1UL << CMA_SHIFT) // 256 MB

typedef struct cma_mem
{
	void           *cma_start_va;
	phys_addr_t     cma_start_pa;
	dma_addr_t      cma_handle;  // bus address
} cma_mem_t;

#endif
