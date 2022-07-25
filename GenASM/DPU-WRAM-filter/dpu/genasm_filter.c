/* MIT License

Copyright (c) 2021 SAFARI Research Group at ETH Zürich

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE. */

#include "../common/common.h"
#include "dpu_allocator_wram.h"
#include "dpu_allocator_mram.h"

unsigned long long *generatePatternBitmasksACGTN(dpu_alloc_wram_t *dpu_alloc_wram, char *pattern, int m)
{
    int count = (m + 64.0) / 64.0;

    int len = 4 * count; // A,C,G,T,N

    unsigned long long *patternBitmasks = (unsigned long long *)allocate_new(dpu_alloc_wram, len * sizeof(unsigned long long));

    unsigned long long max = ULLONG_MAX;

    // Initialize the pattern bitmasks
    for (int i = 0; i < len; i++)
    {
        patternBitmasks[i] = max;
    }

    // Update the pattern bitmasks
    int index;
    for (int i = 0; i < m; i++)
    {
        index = count - ((m - i - 1) / 64) - 1;
        if ((pattern[i] == 'A') || (pattern[i] == 'a'))
        {
            patternBitmasks[0 * count + index] &= ~(1ULL << ((m - i - 1) % 64));
        }
        else if ((pattern[i] == 'C') || (pattern[i] == 'c'))
        {
            patternBitmasks[1 * count + index] &= ~(1ULL << ((m - i - 1) % 64));
        }
        else if ((pattern[i] == 'G') || (pattern[i] == 'g'))
        {
            patternBitmasks[2 * count + index] &= ~(1ULL << ((m - i - 1) % 64));
        }
        else if ((pattern[i] == 'T') || (pattern[i] == 't'))
        {
            patternBitmasks[3 * count + index] &= ~(1ULL << ((m - i - 1) % 64));
        }
        else if ((pattern[i] == 'N') || (pattern[i] == 'n'))
        {
            patternBitmasks[0 * count + index] &= ~(1ULL << ((m - i - 1) % 64));
            patternBitmasks[1 * count + index] &= ~(1ULL << ((m - i - 1) % 64));
            patternBitmasks[2 * count + index] &= ~(1ULL << ((m - i - 1) % 64));
            patternBitmasks[3 * count + index] &= ~(1ULL << ((m - i - 1) % 64));
        }
    }

    return patternBitmasks;
}

int genasm_filter(dpu_alloc_wram_t *dpu_alloc_wram, char *pattern, char *text, int pattern_length, int text_length, int k, dpu_alloc_mram_t *dpu_alloc_mram)
{
    int m = pattern_length;
    int n = text_length;

    unsigned long long max = ULLONG_MAX;

    int count = (m + 64) / 64.0;

    // Allocate and initialize patternBitmask in the WRAM
    unsigned long long *patternBitmasks = generatePatternBitmasksACGTN(dpu_alloc_wram, pattern, m);

    // Initialize the bit arrays R
    int len1 = (k + 1) * count;
    // Allocate R and oldR in WRAM
    unsigned long long *R = (unsigned long long *)allocate_new(dpu_alloc_wram, len1 * sizeof(unsigned long long));

    for (int i = 0; i < len1; i++)
    {
        R[i] = max;
    }

    for (int x = 1; x < (k + 1); x++)
    {
        if ((x % 64) == 0)
        {
            int ind = count - (x / 64);
            for (int y = (count - 1); y >= ind; y--)
            {
                R[x * count + y] = 0ULL;
            }
        }
        else
        {
            int ind = count - 1 - (x / 64);
            for (int y = (count - 1); y > ind; y--)
            {
                R[x * count + y] = 0ULL;
            }
            R[x * count + ind] = max << (x % 64);
        }
    }

    // Allocate all bit-vectors in the WRAM
    unsigned long long *oldR = (unsigned long long *)allocate_new(dpu_alloc_wram, len1 * sizeof(unsigned long long));
    unsigned long long *substitution, *insertion, *match, *deletion, *curBitmask;
    substitution = (unsigned long long *)allocate_new(dpu_alloc_wram, count * sizeof(unsigned long long));
    insertion = (unsigned long long *)allocate_new(dpu_alloc_wram, count * sizeof(unsigned long long));
    match = (unsigned long long *)allocate_new(dpu_alloc_wram, count * sizeof(unsigned long long));
    deletion = (unsigned long long *)allocate_new(dpu_alloc_wram, count * sizeof(unsigned long long));
    curBitmask = (unsigned long long *)allocate_new(dpu_alloc_wram, count * sizeof(unsigned long long));

    int rem = m % 64;

    unsigned long long max1;
    if (rem == 0)
    {
        max1 = 1ULL << 63;
    }
    else
    {
        max1 = 1ULL << (rem - 1);
    }

    for (int i = n - 1; i >= 0; i--)
    {
        char c = text[i];

        if ((c == 'A') || (c == 'a') || (c == 'C') || (c == 'c') || (c == 'G') || (c == 'g') || (c == 'T') || (c == 't'))
        {
            // copy the content of R to oldR
            for (int itR = 0; itR < len1; itR++)
            {
                oldR[itR] = R[itR];
            }

            if ((c == 'A') || (c == 'a'))
            {
                for (int a = 0; a < count; a++)
                {
                    curBitmask[a] = patternBitmasks[0 * count + a];
                }
            }
            else if ((c == 'C') || (c == 'c'))
            {
                for (int a = 0; a < count; a++)
                {
                    curBitmask[a] = patternBitmasks[1 * count + a];
                }
            }
            else if ((c == 'G') || (c == 'g'))
            {
                for (int a = 0; a < count; a++)
                {
                    curBitmask[a] = patternBitmasks[2 * count + a];
                }
            }
            else if ((c == 'T') || (c == 't'))
            {
                for (int a = 0; a < count; a++)
                {
                    curBitmask[a] = patternBitmasks[3 * count + a];
                }
            }

            // update R[0] by first shifting oldR[0] and then ORing with pattern bitmask
            R[0] = oldR[0] << 1;
            for (int a = 1; a < count; a++)
            {
                R[a - 1] |= (oldR[a] >> 63);
                R[a] = oldR[a] << 1;
            }
            for (int a = 0; a < count; a++)
            {
                R[a] |= curBitmask[a];
            }

            for (int d = 1; d <= k; d++)
            {
                int index = (d - 1) * count;

                for (int a = 0; a < count; a++)
                {
                    deletion[a] = oldR[index + a];
                }

                substitution[0] = deletion[0] << 1;
                for (int a = 1; a < count; a++)
                {
                    substitution[a - 1] |= (deletion[a] >> 63);
                    substitution[a] = deletion[a] << 1;
                }

                insertion[0] = R[index] << 1;
                for (int a = 1; a < count; a++)
                {
                    insertion[a - 1] |= (R[index + a] >> 63);
                    insertion[a] = R[index + a] << 1;
                }

                index += count;

                match[0] = oldR[index] << 1;
                for (int a = 1; a < count; a++)
                {
                    match[a - 1] |= (oldR[index + a] >> 63);
                    match[a] = oldR[index + a] << 1;
                }

                for (int a = 0; a < count; a++)
                {
                    match[a] |= curBitmask[a];
                }

                for (int a = 0; a < count; a++)
                {
                    R[index + a] = deletion[a] & substitution[a] & insertion[a] & match[a];
                }
            }
        }
    }

    int minError = -1;
    unsigned long long mask = max1;

    for (int t = 0; t <= k; t++)
    {
        if ((R[t * count] & mask) == 0)
        {
            minError = t;
            break;
        }
    }

    return minError;
}

int main()
{
    mem_reset();
    uint32_t tasklet_id = me();

    dpu_alloc_wram_t dpu_alloc_wram;
    dpu_alloc_mram_t dpu_alloc_mram;

    // Load parameters
    uint32_t params_m = (uint32_t)DPU_MRAM_HEAP_POINTER;
    DPUParams params_w;
    mram_read((__mram_ptr void const *)params_m, &params_w, ROUND_UP_MULTIPLE_8(sizeof(DPUParams)));
    uint32_t nb_reads_per_dpu = params_w.dpuNumReads;

    if (nb_reads_per_dpu <= 0)
        return 0;

    int nb_reads_per_tasklets = ROUND_UP_MULTIPLE_8(((nb_reads_per_dpu + NR_TASKLETS) / NR_TASKLETS));
    // Assign WRAM segment for each tasklet
    dpu_alloc_wram = init_dpu_alloc_wram(WRAM_SEGMENT);
    dpu_alloc_mram.segment_size = ROUND_UP_MULTIPLE_8((64000000 - params_w.mramTotalAllocated) / NR_TASKLETS);
    dpu_alloc_mram.HEAD_PTR_MRAM = ROUND_UP_MULTIPLE_8(dpu_alloc_mram.segment_size * tasklet_id + params_w.mramTotalAllocated);
    dpu_alloc_mram.CUR_PTR_MRAM = dpu_alloc_mram.HEAD_PTR_MRAM;
    dpu_alloc_mram.mem_used_mram = 0;

    uint32_t dpuRequests_m = ((uint32_t)DPU_MRAM_HEAP_POINTER) + params_w.dpuRequests_m;
    uint32_t dpuResults_m = ((uint32_t)DPU_MRAM_HEAP_POINTER) + params_w.dpuResults_m;
    uint32_t dpuPatterns_m = ((uint32_t)DPU_MRAM_HEAP_POINTER) + params_w.dpuPatterns_m;
    uint32_t dpuTexts_m = ((uint32_t)DPU_MRAM_HEAP_POINTER) + params_w.dpuTexts_m;

    for (int read_nb = 0; read_nb < nb_reads_per_tasklets; ++read_nb)
    {
        request_t *request_w = (request_t *)allocate_new(&dpu_alloc_wram, (sizeof(request_t)));
        result_t *result_w = (result_t *)allocate_new(&dpu_alloc_wram, (sizeof(result_t)));

        if (read_nb + tasklet_id * nb_reads_per_tasklets < nb_reads_per_dpu)
        {
            mram_read((__mram_ptr void const *)(dpuRequests_m + (read_nb + tasklet_id * nb_reads_per_tasklets) * (sizeof(request_t))), request_w, ROUND_UP_MULTIPLE_8(sizeof(request_t)));
            char *pattern = (char *)allocate_new(&dpu_alloc_wram, ROUND_UP_MULTIPLE_8(request_w->pattern_len));
            char *text = (char *)allocate_new(&dpu_alloc_wram, ROUND_UP_MULTIPLE_8(request_w->text_len));
            // DMA transfers must be less than 2048
            if (ROUND_UP_MULTIPLE_8(request_w->pattern_len) <= 2048)
            {
                mram_read((__mram_ptr void const *)(dpuPatterns_m + (read_nb + tasklet_id * nb_reads_per_tasklets) * (READ_SIZE)), pattern, ROUND_UP_MULTIPLE_8(request_w->pattern_len));
            }
            else
            {
                for (int segment_size = 0; segment_size <= ROUND_UP_MULTIPLE_8(request_w->pattern_len); segment_size += 2048)
                {
                    if (segment_size + 2048 <= ROUND_UP_MULTIPLE_8(request_w->pattern_len))
                    {
                        mram_read((__mram_ptr void const *)(dpuPatterns_m + (read_nb + tasklet_id * nb_reads_per_tasklets) * (READ_SIZE) + segment_size), &(pattern[segment_size]), 2048);
                    }
                    else
                    {
                        int size = ROUND_UP_MULTIPLE_8(request_w->pattern_len) - segment_size;
                        mram_read((__mram_ptr void const *)(dpuPatterns_m + (read_nb + tasklet_id * nb_reads_per_tasklets) * (READ_SIZE) + segment_size), &(pattern[segment_size]), ROUND_UP_MULTIPLE_8(size));
                    }
                }
            }

            if (ROUND_UP_MULTIPLE_8(request_w->text_len) <= 2048)
            {
                mram_read((__mram_ptr void const *)(dpuTexts_m + (read_nb + tasklet_id * nb_reads_per_tasklets) * (READ_SIZE)), text, ROUND_UP_MULTIPLE_8(request_w->text_len));
            }
            else
            {
                for (int segment_size = 0; segment_size <= ROUND_UP_MULTIPLE_8(request_w->text_len); segment_size += 2048)
                {
                    if (segment_size + 2048 <= ROUND_UP_MULTIPLE_8(request_w->text_len))
                    {
                        mram_read((__mram_ptr void const *)(dpuTexts_m + (read_nb + tasklet_id * nb_reads_per_tasklets) * (READ_SIZE) + segment_size), &(text[segment_size]), 2048);
                    }
                    else
                    {
                        int size = ROUND_UP_MULTIPLE_8(request_w->text_len) - segment_size;
                        mram_read((__mram_ptr void const *)(dpuTexts_m + (read_nb + tasklet_id * nb_reads_per_tasklets) * (READ_SIZE) + segment_size), &(text[segment_size]), ROUND_UP_MULTIPLE_8(size));
                    }
                }
            }

            result_w->idx = request_w->idx;
            int min_error = genasm_filter(&dpu_alloc_wram, pattern, text, request_w->pattern_len, request_w->text_len, MAX_SCORE, &dpu_alloc_mram);



            result_w->score = min_error;
            mram_write(result_w, (__mram_ptr void *)(dpuResults_m + (read_nb + tasklet_id * nb_reads_per_tasklets) * (sizeof(result_t))), (sizeof(result_t)));
        }
        // Each tasklet resets its WRAM after every alignment independently
        reset_dpu_alloc_wram(&dpu_alloc_wram);
        dpu_alloc_mram.CUR_PTR_MRAM = dpu_alloc_mram.HEAD_PTR_MRAM;
        dpu_alloc_mram.mem_used_mram = 0;
    }
    return 0;
}
