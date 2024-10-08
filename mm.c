/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * 
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

team_t team = {
    "team 8",
    "Soojin Lee",
    "asyncwaiter@gmail.com",
    "",
    ""
};
/* basic constants and macros */
#define WSIZE 4 // Word and header/footer size
#define DSIZE 8 // Double word size
#define CHUNKSIZE (1<<12) // 12로 지정된 이유: 1. Memory Pages 2. Efficient Heap Growth 3. Alignment

#define MAX(x, y) ((x) > (y)? (x) : (y))

/* pack a size and allocated bit into a word */
#define PACK(size, alloc) ((size) | (alloc)) // 합치는 연산자

/* read and write a word at address p */
#define GET(p) (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p) = (val))

/* read the size and allocated fields from address p */
#define GET_SIZE(p) (GET(p) & ~0x7) // 마지막 3 bits를 ~연산자로 제외
#define GET_ALLOC(p) (GET(p) & 0x1) // eliminating except lower 3 bits

/* given block ptr bp, compute address of its header and footer */
#define HDRP(bp) ((char *)(bp - WSIZE)) 
#define FTRP(bp) ((char *)(bp + GET_SIZE(HDRP(bp)) -  DSIZE))

/* given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE))) // 푸터는 헤더랑 같으니까

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

/* 
 * mm_init - initialize the malloc package.
 */
static char *heap_listp;


static void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    if (prev_alloc && next_alloc) /* Case 1 */
        return bp;
    else if (prev_alloc && !next_alloc){ /* Case 2 */
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }
    else if (!prev_alloc && next_alloc){ /* Case 3 */
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    else {
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) +
            GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    return bp;
}

static void *extend_heap(size_t words)
{
    char *bp;
    size_t size;

    /* 짝수로 alignment 유지 */
    size = (words % 2) ? (words+1) * WSIZE : words * WSIZE;
    if ((long)(bp=mem_sbrk(size)) == -1 )
        return NULL;
    
    /* Free header/footer, epilogue header 초기화 */
    PUT(HDRP(bp), PACK(size, 0)); /* Free block header */
    PUT(FTRP(bp), PACK(size, 0)); /* Free block footer */
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0,1)); /* New epilogue header */

    /* coalesce 만약 전 블럭이 free된 상태이면 */
    return coalesce(bp);
}

static void *find_fit(size_t asize)
{
    void *bp;
    for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)){
        if (!GET_ALLOC(HDRP(bp)) && asize <= GET_SIZE(HDRP(bp)))
            return bp;
    }
    return NULL;
}

static void place(void *bp, size_t asize)
{
    size_t csize = GET_SIZE(HDRP(bp));
    
    if ((csize - asize) >= (2*DSIZE)){
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(csize-asize, 0));
        PUT(FTRP(bp), PACK(csize-asize, 0));
    }
    else {
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }
}

int mm_init(void)
{
    /* 초기 빈 힙을 생성 */
    if ((heap_listp = mem_sbrk(4*WSIZE)) == (void *)-1)
        return -1;
    PUT(heap_listp, 0); /* alignment padding */
    PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1)); /* prologue header*/
    PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 1)); /* prologue footer*/
    PUT(heap_listp + (3*WSIZE), PACK(0, 1)); /* epilogue header*/
    heap_listp += (2*WSIZE);

    /* extend the empty heap with a free block of CHUNKSIZE bytes */
    if (extend_heap(CHUNKSIZE/WSIZE) == NULL)
        return -1;
    return 0;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    size_t asize; /* adjusted block size */
    size_t extendsize; /* 맞는 크기가 없을 때 늘릴 사이즈 */
    char *bp;

    if (size == 0)
        return NULL;

    /* adjust block size to include overhead and alignment reqs */
    if (size <= DSIZE)
        asize = 2*DSIZE;
    else
        asize = DSIZE * ((size+(DSIZE)+(DSIZE-1)) / DSIZE);
    
    /* search the free list for a fit */
    if ((bp = find_fit(asize)) != NULL){
        place(bp, asize);
        return bp;
    }

    /* 맞는 사이즈 없을 때, get more memory and place the block */
    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize/WSIZE)) == NULL)
        return NULL;
    place(bp, asize);
    return bp; 
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
    size_t size = GET_SIZE(HDRP(ptr));
    
    PUT(HDRP(ptr), PACK(size, 0));
    PUT(FTRP(ptr), PACK(size, 0));
    coalesce(ptr);
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;
    
    newptr = mm_malloc(size);
    if (newptr == NULL)
      return NULL;
    copySize = GET_SIZE(HDRP(oldptr)) - WSIZE;
    if (size < copySize)
      copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}














