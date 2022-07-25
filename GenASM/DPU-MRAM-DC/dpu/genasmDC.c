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
#include <stdio.h>
#include "dpu_allocator_mram.h"

void edit_cigar_allocate(
    edit_cigar_t *edit_cigar,
    int pattern_length,
    int text_length,
    dpu_alloc_wram_t *allocator)
{
    edit_cigar->max_operations = pattern_length + text_length;
    edit_cigar->begin_offset = edit_cigar->max_operations - 1;
    edit_cigar->end_offset = edit_cigar->max_operations;
    edit_cigar->score = INT32_MIN;
}

uint32_t generatePatternBitmasksACGT(dpu_alloc_mram_t *dpu_alloc_mram, dpu_alloc_wram_t *dpu_alloc_wram, char *pattern, int m)
{
    int count = (m + 64.0) / 64.0;

    int len = 4 * count; // A,C,G,T,N

    uint32_t patternBitmasks_m = allocate_mram(dpu_alloc_mram, len * sizeof(unsigned long long));
    patternBitmasks_m += (uint32_t)DPU_MRAM_HEAP_POINTER;
    unsigned long long *patternBitmasks_cache = (unsigned long long *)allocate_new(dpu_alloc_wram, sizeof(unsigned long long));

    unsigned long long max = ULLONG_MAX;

    // Initialize the pattern bitmasks
    for (int i = 0; i < len; i++)
    {
        patternBitmasks_cache[0] = max;
        mram_write(patternBitmasks_cache, (__mram_ptr void *)(patternBitmasks_m + i * sizeof(unsigned long long)), ROUND_UP_MULTIPLE_8(sizeof(unsigned long long)));
    }

    // Update the pattern bitmasks
    int index;
    for (int i = 0; i < m; i++)
    {
        index = count - ((m - i - 1) / 64) - 1;
        if ((pattern[i] == 'A') || (pattern[i] == 'a'))
        {
            mram_read((__mram_ptr const void *)(patternBitmasks_m + (0 * count + index) * sizeof(unsigned long long)), patternBitmasks_cache, (sizeof(unsigned long long)));
            patternBitmasks_cache[0] &= ~(1ULL << ((m - i - 1) % 64));
            mram_write(patternBitmasks_cache, (__mram_ptr void *)(patternBitmasks_m + (0 * count + index) * sizeof(unsigned long long)), (sizeof(unsigned long long)));
        }
        else if ((pattern[i] == 'C') || (pattern[i] == 'c'))
        {
            mram_read((__mram_ptr const void *)(patternBitmasks_m + (1 * count + index) * sizeof(unsigned long long)), patternBitmasks_cache, (sizeof(unsigned long long)));
            patternBitmasks_cache[0] &= ~(1ULL << ((m - i - 1) % 64));
            mram_write(patternBitmasks_cache, (__mram_ptr void *)(patternBitmasks_m + (1 * count + index) * sizeof(unsigned long long)), (sizeof(unsigned long long)));
        }
        else if ((pattern[i] == 'G') || (pattern[i] == 'g'))
        {
            mram_read((__mram_ptr const void *)(patternBitmasks_m + (2 * count + index) * sizeof(unsigned long long)), patternBitmasks_cache, (sizeof(unsigned long long)));
            patternBitmasks_cache[0] &= ~(1ULL << ((m - i - 1) % 64));
            mram_write(patternBitmasks_cache, (__mram_ptr void *)(patternBitmasks_m + (2 * count + index) * sizeof(unsigned long long)), (sizeof(unsigned long long)));
        }
        else if ((pattern[i] == 'T') || (pattern[i] == 't'))
        {
            mram_read((__mram_ptr const void *)(patternBitmasks_m + (3 * count + index) * sizeof(unsigned long long)), patternBitmasks_cache, (sizeof(unsigned long long)));
            patternBitmasks_cache[0] &= ~(1ULL << ((m - i - 1) % 64));
            mram_write(patternBitmasks_cache, (__mram_ptr void *)(patternBitmasks_m + (3 * count + index) * sizeof(unsigned long long)), (sizeof(unsigned long long)));
        }
    }

    return patternBitmasks_m;
}

void genasmTB(dpu_alloc_mram_t *dpu_alloc_mram, dpu_alloc_wram_t *dpu_alloc_wram, uint32_t tracebackMatrix_m, int n, int k, int count, uint32_t tasklet_id, int m, int minError, int *ed, unsigned long long mask, char *lastChar, int *charCount, char *CIGARstr, int *max_operations, char *text, int *countM, int *countS, int *countD, int *countI, int *countOpen, int *countExtend, bool *isFirst)
{
    int ind;
    int r;
    int curPattern = m - 1;
    int curText = 0;
    int curError = minError;
    int COUNT = (m + 64) / 64.0;
    unsigned long long *traceback_cache0 = (unsigned long long *)allocate_new(dpu_alloc_wram, sizeof(unsigned long long));
    unsigned long long *traceback_cache1 = (unsigned long long *)allocate_new(dpu_alloc_wram, sizeof(unsigned long long));
    unsigned long long *traceback_cache2 = (unsigned long long *)allocate_new(dpu_alloc_wram, sizeof(unsigned long long));
    unsigned long long *traceback_cache3 = (unsigned long long *)allocate_new(dpu_alloc_wram, sizeof(unsigned long long));
    int c = 0;
    while ((curPattern >= 0) && (curError >= 0))
    {
        ind = count - (curPattern / 64) - 1;

        mram_read((__mram_ptr void const *)(((uint32_t)DPU_MRAM_HEAP_POINTER) + tracebackMatrix_m + (((curText * (MAX_SCORE + 1) + curError) * 4 + 0) * COUNT + ind) * sizeof(unsigned long long)), traceback_cache0, sizeof(unsigned long long));
        mram_read((__mram_ptr void const *)(((uint32_t)DPU_MRAM_HEAP_POINTER) + tracebackMatrix_m + (((curText * (MAX_SCORE + 1) + curError) * 4 + 1) * COUNT + ind) * sizeof(unsigned long long)), traceback_cache1, sizeof(unsigned long long));
        mram_read((__mram_ptr void const *)(((uint32_t)DPU_MRAM_HEAP_POINTER) + tracebackMatrix_m + (((curText * (MAX_SCORE + 1) + curError) * 4 + 2) * COUNT + ind) * sizeof(unsigned long long)), traceback_cache2, sizeof(unsigned long long));
        mram_read((__mram_ptr void const *)(((uint32_t)DPU_MRAM_HEAP_POINTER) + tracebackMatrix_m + (((curText * (MAX_SCORE + 1) + curError) * 4 + 3) * COUNT + ind) * sizeof(unsigned long long)), traceback_cache3, sizeof(unsigned long long));

        // affine-insertion
        if (*lastChar == 'I' && ((traceback_cache2[0] & mask) == 0))
        {
            curPattern -= 1;
            curError -= 1;

            mask = 1ULL << (curPattern % 64);
            if (*lastChar == 'I')
            {
                *charCount += 1;
                *countExtend += 1;
            }
            else
            {
                if (!*isFirst)
                {

                    int num = *charCount;
                    while (num != 0)
                    {
                        r = num % 10;
                        num = num / 10;
                        CIGARstr[c] = (char)(r + '0');
                        c++;
                    }
                    CIGARstr[c] = (*lastChar);
                    c++;
                }
                *charCount = 1;
                *lastChar = 'I';
                *countOpen += 1;
            }

            *countI += 1;
            *ed += 1;
        }
        // affine-deletion
        else if (*lastChar == 'D' && ((traceback_cache3[0] & mask) == 0))
        {
            curText += 1;
            curError -= 1;
            if (*lastChar == 'D')
            {
                *charCount += 1;
                *countExtend += 1;
            }
            else
            {
                if (!*isFirst)
                {
                    int num = *charCount;
                    while (num != 0)
                    {
                        r = num % 10;
                        num = num / 10;
                        CIGARstr[c] = (char)(r + '0');
                        c++;
                    }
                    CIGARstr[c] = (*lastChar);
                    c++;
                }
                *charCount = 1;
                *lastChar = 'D';
                *countOpen += 1;
            }

            *countD += 1;
            *ed += 1;
        }
        // match
        else if ((traceback_cache0[0] & mask) == 0)
        {
            curText += 1;
            curPattern -= 1;
            mask = 1ULL << (curPattern % 64);
            if (*lastChar == 'M')
            {
                *charCount += 1;
            }
            else
            {
                if (!*isFirst)
                {

                    int num = *charCount;
                    while (num != 0)
                    {
                        r = num % 10;
                        num = num / 10;
                        CIGARstr[c] = (char)(r + '0');
                        c++;
                    }
                    CIGARstr[c] = (*lastChar);
                    c++;
                }
                *charCount = 1;
                *lastChar = 'M';
            }
            *countM += 1;
        }
        // substitution
        else if ((traceback_cache1[0] & mask) == 0)
        {
            curText += 1;
            curPattern -= 1;
            curError -= 1;
            mask = 1ULL << (curPattern % 64);
            if (*lastChar == 'S')
            {
                *charCount += 1;
            }
            else
            {
                if (!*isFirst)
                {
                    int num = *charCount;
                    while (num != 0)
                    {
                        r = num % 10;
                        num = num / 10;
                        CIGARstr[c] = (char)(r + '0');
                        c++;
                    }
                    CIGARstr[c] = (*lastChar);
                    c++;
                    // sprintf(CIGARstr, "%s%d", CIGARstr, *charCount);
                    // sprintf(CIGARstr, "%s%c", CIGARstr, *lastChar);
                }
                *charCount = 1;
                *lastChar = 'S';
            }
            *countS += 1;
            *ed += 1;
        }
        // deletion
        else if ((traceback_cache3[0] & mask) == 0)
        {
            curText += 1;
            curError -= 1;
            if (*lastChar == 'D')
            {
                *charCount += 1;
                *countExtend += 1;
            }
            else
            {
                if (!*isFirst)
                {
                    int num = *charCount;
                    while (num != 0)
                    {
                        r = num % 10;
                        num = num / 10;
                        CIGARstr[c] = (char)(r + '0');
                        c++;
                    }
                    CIGARstr[c] = (*lastChar);
                    c++;
                    // sprintf(CIGARstr, "%s%d", CIGARstr, *charCount);
                    // sprintf(CIGARstr, "%s%c", CIGARstr, *lastChar);
                }
                *charCount = 1;
                *lastChar = 'D';
                *countOpen += 1;
            }

            *countD += 1;
            *ed += 1;
        }
        // insertion
        else if ((traceback_cache2[0] & mask) == 0)
        {
            curPattern -= 1;
            curError -= 1;
            mask = 1ULL << (curPattern % 64);
            if (*lastChar == 'I')
            {
                *charCount += 1;
                *countExtend += 1;
            }
            else
            {
                if (!*isFirst)
                {
                    int num = *charCount;
                    while (num != 0)
                    {
                        r = num % 10;
                        num = num / 10;
                        CIGARstr[c] = (char)(r + '0');
                        c++;
                    }
                    CIGARstr[c] = (*lastChar);
                    c++;
                    // sprintf(CIGARstr, "%s%d", CIGARstr, *charCount);
                    // sprintf(CIGARstr, "%s%c", CIGARstr, *lastChar);
                }
                *charCount = 1;
                *lastChar = 'I';
                *countOpen += 1;
            }

            *countI += 1;
            *ed += 1;
        }

        *isFirst = false;
    }

    int num = *charCount;
    while (num != 0)
    {
        r = num % 10;
        num = num / 10;
        CIGARstr[c] = (char)(r + '0');
        c++;
    }
    CIGARstr[c] = (*lastChar);
    c++;
    CIGARstr[c] = '\0';
    *max_operations = c + 1;
}

int genasmDC(dpu_alloc_wram_t *dpu_alloc_wram, char *pattern, char *text, int pattern_length, int text_length, int k, uint32_t tasklet_id, char *CIGARstr, int *max_operations, dpu_alloc_mram_t *dpu_alloc_mram)
{
    int m = pattern_length;
    int n = text_length;

    int ed = 0;

    unsigned long long max = ULLONG_MAX;

    int count = (m + 64) / 64.0;
    int charCount = 0;

    char lastChar = '0';

    bool isFirst = true;

    int countM = 0;
    int countS = 0;
    int countI = 0;
    int countD = 0;
    int countOpen = 0;
    int countExtend = 0;

    // Allocate and Initialize the patternBitmask in the MRAM
    uint32_t patternBitmasks_m = generatePatternBitmasksACGT(dpu_alloc_mram, dpu_alloc_wram, pattern, m);

    int len1 = (k + 1) * count;

    // Allocate R and oldR vectors in the MRAM
    uint32_t R_m = allocate_mram(dpu_alloc_mram, len1 * sizeof(unsigned long long));
    uint32_t oldR_m = allocate_mram(dpu_alloc_mram, len1 * sizeof(unsigned long long));
    // Allocate cache cells in the WRAM
    unsigned long long *R_cache = (unsigned long long *)allocate_new(dpu_alloc_wram, sizeof(unsigned long long));
    unsigned long long *oldR_cache = (unsigned long long *)allocate_new(dpu_alloc_wram, sizeof(unsigned long long));

    // Store subsitution, insertion, match, deletion, currBitmask in WRAM since they are of small sizes
    unsigned long long *substitution, *insertion, *match, *deletion, *curBitmask;
    substitution = (unsigned long long *)allocate_new(dpu_alloc_wram, count * sizeof(unsigned long long));
    insertion = (unsigned long long *)allocate_new(dpu_alloc_wram, count * sizeof(unsigned long long));
    match = (unsigned long long *)allocate_new(dpu_alloc_wram, count * sizeof(unsigned long long));
    deletion = (unsigned long long *)allocate_new(dpu_alloc_wram, count * sizeof(unsigned long long));
    curBitmask = (unsigned long long *)allocate_new(dpu_alloc_wram, count * sizeof(unsigned long long));
    unsigned long long *traceback_cache = (unsigned long long *)allocate_new(dpu_alloc_wram, sizeof(unsigned long long));

    // Store the traceback matrix in the MRAM
    uint32_t tracebackMatrix_m = allocate_mram(dpu_alloc_mram, n * (MAX_SCORE + 1) * 4 * count * sizeof(unsigned long long));

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

    // Initialize the bit arrays R
    for (int i = 0; i < len1; i++)
    {
        R_cache[0] = max;
        mram_write(R_cache, (__mram_ptr void *)((uint32_t)DPU_MRAM_HEAP_POINTER + R_m + (i) * sizeof(unsigned long long)), (sizeof(unsigned long long)));
    }
    for (int x = 1; x < (k + 1); x++)
    {
        if ((x % 64) == 0)
        {
            int ind = count - (x / 64);
            for (int y = (count - 1); y >= ind; y--)
            {
                R_cache[0] = 0ULL;
                mram_write(R_cache, (__mram_ptr void *)((uint32_t)DPU_MRAM_HEAP_POINTER + R_m + (x * count + y) * sizeof(unsigned long long)), (sizeof(unsigned long long)));
            }
        }
        else
        {
            int ind = count - 1 - (x / 64);
            for (int y = (count - 1); y > ind; y--)
            {
                R_cache[0] = 0ULL;
                mram_write(R_cache, (__mram_ptr void *)((uint32_t)DPU_MRAM_HEAP_POINTER + R_m + (x * count + y) * sizeof(unsigned long long)), (sizeof(unsigned long long)));
            }

            R_cache[0] = max << (x % 64);
            mram_write(R_cache, (__mram_ptr void *)((uint32_t)DPU_MRAM_HEAP_POINTER + R_m + (x * count + ind) * sizeof(unsigned long long)), (sizeof(unsigned long long)));
        }
    }
    unsigned long long *patternBitmasks_cache = (unsigned long long *)allocate_new(dpu_alloc_wram, sizeof(unsigned long long));

    for (int i = n - 1; i >= 0; i--)
    {
        char c = text[i];

        if ((c == 'A') || (c == 'a') || (c == 'C') || (c == 'c') || (c == 'G') || (c == 'g') || (c == 'T') || (c == 't'))
        {
            // Swap the MRAM addresses of R and oldR
            uint32_t tmp_m = oldR_m;
            oldR_m = R_m;
            R_m = tmp_m;

            if ((c == 'A') || (c == 'a'))
            {
                for (int a = 0; a < count; a++)
                {
                    mram_read((__mram_ptr void const *)(patternBitmasks_m + (0 * count + a) * sizeof(unsigned long long)), patternBitmasks_cache, sizeof(unsigned long long));
                    curBitmask[a] = patternBitmasks_cache[0];
                }
            }
            else if ((c == 'C') || (c == 'c'))
            {
                for (int a = 0; a < count; a++)
                {
                    mram_read((__mram_ptr void const *)(patternBitmasks_m + (1 * count + a) * sizeof(unsigned long long)), patternBitmasks_cache, sizeof(unsigned long long));
                    curBitmask[a] = patternBitmasks_cache[0];
                }
            }
            else if ((c == 'G') || (c == 'g'))
            {
                for (int a = 0; a < count; a++)
                {
                    mram_read((__mram_ptr void const *)(patternBitmasks_m + (2 * count + a) * sizeof(unsigned long long)), patternBitmasks_cache, sizeof(unsigned long long));
                    curBitmask[a] = patternBitmasks_cache[0];
                }
            }
            else if ((c == 'T') || (c == 't'))
            {
                for (int a = 0; a < count; a++)
                {
                    mram_read((__mram_ptr void const *)(patternBitmasks_m + (3 * count + a) * sizeof(unsigned long long)), patternBitmasks_cache, sizeof(unsigned long long));
                    curBitmask[a] = patternBitmasks_cache[0];
                }
            }

            // update R[0] by first shifting oldR[0] and then ORing with pattern bitmask
            mram_read((__mram_ptr const void *)((uint32_t)DPU_MRAM_HEAP_POINTER + oldR_m + (0) * sizeof(unsigned long long)), oldR_cache, (sizeof(unsigned long long)));
            R_cache[0] = oldR_cache[0] << 1;
            mram_write(R_cache, (__mram_ptr void *)((uint32_t)DPU_MRAM_HEAP_POINTER + R_m + (0) * sizeof(unsigned long long)), (sizeof(unsigned long long)));
            for (int a = 1; a < count; a++)
            {
                mram_read((__mram_ptr const void *)((uint32_t)DPU_MRAM_HEAP_POINTER + oldR_m + (a) * sizeof(unsigned long long)), oldR_cache, (sizeof(unsigned long long)));
                mram_read((__mram_ptr const void *)((uint32_t)DPU_MRAM_HEAP_POINTER + R_m + (a - 1) * sizeof(unsigned long long)), R_cache, (sizeof(unsigned long long)));
                R_cache[0] |= (oldR_cache[0] >> 63);
                mram_write(R_cache, (__mram_ptr void *)((uint32_t)DPU_MRAM_HEAP_POINTER + R_m + (a - 1) * sizeof(unsigned long long)), (sizeof(unsigned long long)));

                mram_read((__mram_ptr const void *)((uint32_t)DPU_MRAM_HEAP_POINTER + R_m + (a) * sizeof(unsigned long long)), R_cache, (sizeof(unsigned long long)));
                R_cache[0] = oldR_cache[0] << 1;
                mram_write(R_cache, (__mram_ptr void *)((uint32_t)DPU_MRAM_HEAP_POINTER + R_m + (a) * sizeof(unsigned long long)), (sizeof(unsigned long long)));
            }
            for (int a = 0; a < count; a++)
            {
                mram_read((__mram_ptr const void *)((uint32_t)DPU_MRAM_HEAP_POINTER + R_m + (a) * sizeof(unsigned long long)), R_cache, (sizeof(unsigned long long)));
                R_cache[0] |= curBitmask[a];
                mram_write(R_cache, (__mram_ptr void *)((uint32_t)DPU_MRAM_HEAP_POINTER + R_m + (a) * sizeof(unsigned long long)), (sizeof(unsigned long long)));
            }

            for (int a = 0; a < count; a++)
            {
                mram_read((__mram_ptr const void *)((uint32_t)DPU_MRAM_HEAP_POINTER + R_m + (a) * sizeof(unsigned long long)), R_cache, (sizeof(unsigned long long)));
                traceback_cache[0] = R_cache[0];
                mram_write(traceback_cache, (__mram_ptr void *)(((uint32_t)DPU_MRAM_HEAP_POINTER) + tracebackMatrix_m + (((i * (MAX_SCORE + 1) + 0) * 4 + 0) * count + a) * sizeof(unsigned long long)), sizeof(unsigned long long));
                traceback_cache[0] = max;
                traceback_cache[0] = max;
                traceback_cache[0] = max;
                mram_write(traceback_cache, (__mram_ptr void *)(((uint32_t)DPU_MRAM_HEAP_POINTER) + tracebackMatrix_m + (((i * (MAX_SCORE + 1) + 0) * 4 + 1) * count + a) * sizeof(unsigned long long)), sizeof(unsigned long long));
                mram_write(traceback_cache, (__mram_ptr void *)(((uint32_t)DPU_MRAM_HEAP_POINTER) + tracebackMatrix_m + (((i * (MAX_SCORE + 1) + 0) * 4 + 2) * count + a) * sizeof(unsigned long long)), sizeof(unsigned long long));
                mram_write(traceback_cache, (__mram_ptr void *)(((uint32_t)DPU_MRAM_HEAP_POINTER) + tracebackMatrix_m + (((i * (MAX_SCORE + 1) + 0) * 4 + 3) * count + a) * sizeof(unsigned long long)), sizeof(unsigned long long));
            }

            for (int d = 1; d <= k; d++)
            {
                int index = (d - 1) * count;

                for (int a = 0; a < count; a++)
                {
                    mram_read((__mram_ptr const void *)((uint32_t)DPU_MRAM_HEAP_POINTER + oldR_m + (index + a) * sizeof(unsigned long long)), oldR_cache, (sizeof(unsigned long long)));
                    deletion[a] = oldR_cache[0];
                }

                substitution[0] = deletion[0] << 1;
                for (int a = 1; a < count; a++)
                {

                    substitution[a - 1] |= (deletion[a] >> 63);
                    substitution[a] = deletion[a] << 1;
                }

                mram_read((__mram_ptr const void *)((uint32_t)DPU_MRAM_HEAP_POINTER + R_m + (index) * sizeof(unsigned long long)), R_cache, (sizeof(unsigned long long)));
                insertion[0] = R_cache[0] << 1;
                for (int a = 1; a < count; a++)
                {
                    mram_read((__mram_ptr const void *)((uint32_t)DPU_MRAM_HEAP_POINTER + R_m + (index + a) * sizeof(unsigned long long)), R_cache, (sizeof(unsigned long long)));
                    insertion[a - 1] |= (R_cache[0] >> 63);
                    insertion[a] = R_cache[0] << 1;
                }

                index += count;
                mram_read((__mram_ptr const void *)((uint32_t)DPU_MRAM_HEAP_POINTER + oldR_m + (index) * sizeof(unsigned long long)), oldR_cache, (sizeof(unsigned long long)));
                match[0] = oldR_cache[0] << 1;
                for (int a = 1; a < count; a++)
                {
                    mram_read((__mram_ptr const void *)((uint32_t)DPU_MRAM_HEAP_POINTER + oldR_m + (index + a) * sizeof(unsigned long long)), oldR_cache, (sizeof(unsigned long long)));
                    match[a - 1] |= (oldR_cache[0] >> 63);
                    match[a] = oldR_cache[0] << 1;
                }

                for (int a = 0; a < count; a++)
                {
                    match[a] |= curBitmask[a];
                }

                for (int a = 0; a < count; a++)
                {
                    R_cache[0] = deletion[a] & substitution[a] & insertion[a] & match[a];
                    mram_write(R_cache, (__mram_ptr void *)((uint32_t)DPU_MRAM_HEAP_POINTER + R_m + (index + a) * sizeof(unsigned long long)), (sizeof(unsigned long long)));
                }

                for (int a = 0; a < count; a++)
                {
                    traceback_cache[0] = match[a];
                    mram_write(traceback_cache, (__mram_ptr void *)(((uint32_t)DPU_MRAM_HEAP_POINTER) + tracebackMatrix_m + (((i * (MAX_SCORE + 1) + d) * 4 + 0) * count + a) * sizeof(unsigned long long)), sizeof(unsigned long long));
                    traceback_cache[0] = substitution[a];
                    mram_write(traceback_cache, (__mram_ptr void *)(((uint32_t)DPU_MRAM_HEAP_POINTER) + tracebackMatrix_m + (((i * (MAX_SCORE + 1) + d) * 4 + 1) * count + a) * sizeof(unsigned long long)), sizeof(unsigned long long));
                    traceback_cache[0] = insertion[a];
                    mram_write(traceback_cache, (__mram_ptr void *)(((uint32_t)DPU_MRAM_HEAP_POINTER) + tracebackMatrix_m + (((i * (MAX_SCORE + 1) + d) * 4 + 2) * count + a) * sizeof(unsigned long long)), sizeof(unsigned long long));
                    traceback_cache[0] = deletion[a];
                    mram_write(traceback_cache, (__mram_ptr void *)(((uint32_t)DPU_MRAM_HEAP_POINTER) + tracebackMatrix_m + (((i * (MAX_SCORE + 1) + d) * 4 + 3) * count + a) * sizeof(unsigned long long)), sizeof(unsigned long long));
                }
            }
        }
    }

    int minError = -1;
    unsigned long long mask = max1;

    for (int t = 0; t <= k; t++)
    {
        mram_read((__mram_ptr const void *)((uint32_t)DPU_MRAM_HEAP_POINTER + R_m + (t * count) * sizeof(unsigned long long)), R_cache, (sizeof(unsigned long long)));
        if ((R_cache[0] & mask) == 0)
        {
            minError = t;
            break;
        }
    }

    if (minError == -1)
    {
        printf("No alignment found!\t");
        return -1;
    }

    genasmTB(dpu_alloc_mram, dpu_alloc_wram, tracebackMatrix_m, n, k, count, tasklet_id, m, minError, &ed, mask, &lastChar, &charCount, CIGARstr, max_operations, text, &countM, &countS, &countD, &countI, &countOpen, &countExtend, &isFirst);

    int bitmacScore = countM * MATCH + countS * MISMATCH + countOpen * (GAP_O + GAP_E) + countExtend * GAP_E;

    return bitmacScore;
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

    int nb_reads_per_tasklets = (((nb_reads_per_dpu + NR_TASKLETS) / NR_TASKLETS));

    // Assign WRAM and MRAM segments for each tasklet
    dpu_alloc_wram = init_dpu_alloc_wram(WRAM_SEGMENT);
    // dpu_alloc_wram = init_dpu_alloc_wram((62000 - 1024 * NR_TASKLETS) / NR_TASKLETS);
    dpu_alloc_mram.segment_size = ROUND_UP_MULTIPLE_8((64000000 - params_w.mramTotalAllocated) / NR_TASKLETS);
    dpu_alloc_mram.HEAD_PTR_MRAM = ROUND_UP_MULTIPLE_8(dpu_alloc_mram.segment_size * tasklet_id + params_w.mramTotalAllocated);
    dpu_alloc_mram.CUR_PTR_MRAM = dpu_alloc_mram.HEAD_PTR_MRAM;
    dpu_alloc_mram.mem_used_mram = 0;

    uint32_t dpuRequests_m = ((uint32_t)DPU_MRAM_HEAP_POINTER) + params_w.dpuRequests_m;
    uint32_t dpuResults_m = ((uint32_t)DPU_MRAM_HEAP_POINTER) + params_w.dpuResults_m;
    uint32_t dpuPatterns_m = ((uint32_t)DPU_MRAM_HEAP_POINTER) + params_w.dpuPatterns_m;
    uint32_t dpuTexts_m = ((uint32_t)DPU_MRAM_HEAP_POINTER) + params_w.dpuTexts_m;
    uint32_t dpuOperations_m = ((uint32_t)DPU_MRAM_HEAP_POINTER) + params_w.dpuOperations_m;

    for (int read_nb = 0; read_nb < nb_reads_per_tasklets; ++read_nb)
    {
        request_t *request_w = (request_t *)allocate_new(&dpu_alloc_wram, (sizeof(request_t)));
        result_t *result_w = (result_t *)allocate_new(&dpu_alloc_wram, (sizeof(result_t)));

        edit_cigar_t *cigar;
        cigar = (edit_cigar_t *)allocate_new(&dpu_alloc_wram, sizeof(edit_cigar_t));

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
            edit_cigar_allocate(cigar, request_w->pattern_len, request_w->text_len, &dpu_alloc_wram);

            cigar->operations = (char *)allocate_new(&dpu_alloc_wram, 2 * READ_SIZE);

            int min_error = genasmDC(&dpu_alloc_wram, pattern, text, request_w->pattern_len, request_w->text_len, MAX_SCORE, tasklet_id, cigar->operations, &cigar->max_operations, &dpu_alloc_mram);

            result_w->idx = request_w->idx;

            if (ROUND_UP_MULTIPLE_8(cigar->max_operations) <= 2048)
            {
                mram_write((cigar->operations), (__mram_ptr void *)(dpuOperations_m + (read_nb + tasklet_id * nb_reads_per_tasklets) * (2 * READ_SIZE)), ROUND_UP_MULTIPLE_8(cigar->max_operations));
            }
            else
            {
                for (int segment_size = 0; segment_size <= ROUND_UP_MULTIPLE_8(cigar->max_operations); segment_size += 2048)
                {
                    if (segment_size + 2048 <= ROUND_UP_MULTIPLE_8(cigar->max_operations))
                    {
                        mram_write(&(cigar->operations[segment_size]), (__mram_ptr void *)(dpuOperations_m + (read_nb + tasklet_id * nb_reads_per_tasklets) * (2 * READ_SIZE) + segment_size), 2048);
                    }
                    else
                    {
                        int size = ROUND_UP_MULTIPLE_8(cigar->max_operations) - segment_size;
                        mram_write(&(cigar->operations[segment_size]), (__mram_ptr void *)(dpuOperations_m + (read_nb + tasklet_id * nb_reads_per_tasklets) * (2 * READ_SIZE) + segment_size), ROUND_UP_MULTIPLE_8(size));
                    }
                }
            }

            result_w->score = min_error;
            result_w->max_operations = cigar->max_operations;

            mram_write(result_w, (__mram_ptr void *)(dpuResults_m + (read_nb + tasklet_id * nb_reads_per_tasklets) * (sizeof(result_t))), sizeof(result_t));
        }
        // Each tasklet resets its WRAM after every alignment independently
        reset_dpu_alloc_wram(&dpu_alloc_wram);
        dpu_alloc_mram.CUR_PTR_MRAM = dpu_alloc_mram.HEAD_PTR_MRAM;
        dpu_alloc_mram.mem_used_mram = 0;
    }
    return 0;
}