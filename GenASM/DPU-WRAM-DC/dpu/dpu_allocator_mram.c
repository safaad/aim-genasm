
#include "dpu_allocator_mram.h"

uint32_t allocate_mram(dpu_alloc_mram_t *dpu_alloc_mram, uint32_t size)
{
    uint32_t mramIdx;
    if (((ROUND_UP_MULTIPLE_8(size) + dpu_alloc_mram->mem_used_mram) >= dpu_alloc_mram->segment_size))
    {
        printf("Out of memory MRAM\n");
        exit(-1);
    }
    if (size <= 0)
    {
        (mramIdx) = 0;
        return 0;
    }
    size = ROUND_UP_MULTIPLE_8(size);
    dpu_alloc_mram->mem_used_mram += size;
    (mramIdx) = dpu_alloc_mram->CUR_PTR_MRAM;
    dpu_alloc_mram->CUR_PTR_MRAM += size;

    return mramIdx;
}