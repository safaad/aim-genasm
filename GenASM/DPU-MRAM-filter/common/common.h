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
#ifndef COMMON_H__
#define COMMON_H__

#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>

#define DPU_CAPACITY (64 << 20)

#ifndef MATCH
#define MATCH 0
#endif

#ifndef MISMATCH
#define MISMATCH 3
#endif

#ifndef GAP_O
#define GAP_O 4
#endif

#ifndef GAP_E
#define GAP_E 1
#endif

#ifndef MAX_SCORE
#define MAX_SCORE 5
#endif

#ifndef READ_SIZE
#define READ_SIZE 120
#endif

#ifndef WRAM_SEGMENT
#define WRAM_SEGMENT 1024
#endif

#define MIN(a, b) (((a) <= (b)) ? (a) : (b))
#define MAX(a, b) (((a) >= (b)) ? (a) : (b))
#define ABS(a) (((a) >= 0) ? (a) : -1 * (a))
#define ROUND_UP_MULTIPLE_8(x) ((((x) + 7) / 8) * 8)
#define ROUND_UP_MULTIPLE_4(x) ((((x) + 3) / 4) * 4)

#define ROUND_UP_MULTIPLE_2(x) (x + 1) & (-2)

typedef struct request_t
{
    int16_t pattern_len;
    int16_t text_len;
    uint32_t idx;
} request_t;

typedef struct result_t
{
    int score;
    uint32_t idx;
    uint64_t cycles;
} result_t;

typedef struct DPUParams
{
    uint32_t dpuNumReads;        /* Number of reads assigned to the DPU */
    uint32_t dpuRequests_m;      /* Base address of the requests in the MRAM */
    uint32_t dpuResults_m;       /* Base address of the results in the MRAM */
    uint32_t dpuPatterns_m;      /* Base address of the patterns sequences in the MRAM */
    uint32_t dpuTexts_m;         /* Base address of the text sequences in the MRAM */
    uint32_t mramTotalAllocated; /* Size of the MRAM memory allocated by the host */
} DPUParams;

#endif
