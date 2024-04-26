/**
 * Do not submit your assignment with a main function in this file.
 * If you submit with a main function in this file, you will get a zero.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "debug.h"
#include "sfmm.h"
#include "errno.h"

// helper file
#include "helper.h"

void *sf_malloc(size_t size)
{

    if (size == 0)
    {
        return NULL;
    }

    // when memstart == memend, means no heap

    // initialize

    if (sf_mem_start() == sf_mem_end())
    {

        initializeHeap();

    }

    
    // change to payload pointer later

    // heap and free lists initalized

    // now time to add malloced block

    // memory size align
    size = size + sizeof(sf_header);
    int sizeAligned = checkSizeAligned(size);

    // search quick list -> free lists

    sf_block *allocatedBlock = searchQuickList(sizeAligned);

    if (allocatedBlock == NULL)
    {
        // nothing in quick lists, search main free lists
        allocatedBlock = searchFreeList(sizeAligned);
    }

    while (allocatedBlock == NULL)
    {
        // if allocated block is still null, indicates no space, have to grow heap, <<< can do this later just allocate block for now

        // grow heap and continuosly check if we can find a new allocation block

        if (extendHeap())
        {
            allocatedBlock = searchFreeList(sizeAligned);
        }
        else
        {
            sf_errno = ENOMEM;
            return NULL;
        }

        // shouldn't affect quicklists so only search free lists

        allocatedBlock = searchFreeList(sizeAligned);
    }

    // error checks done here

    // we have free block, now we splinter for appropriate space

    allocate(allocatedBlock, sizeAligned);

    return allocatedBlock->body.payload;    
}

void sf_free(void *pp)
{

    
    if (checkValidFree(pp))
    {
        abort();
    }

    sf_block *free = (pp - (sizeof(sf_header) + sizeof(sf_footer)));

    // remove allocation and change info

    sf_header freeHeader = (free->header ^ MAGIC) & ~THIS_BLOCK_ALLOCATED;
    free->header = (freeHeader ^ MAGIC);
    int sizeIndex = findSizeClass(getSize(free));
    
    sf_block *setNextBlock = getNextBlock(free);
    setNextBlock->prev_footer = free->header;
    insertFreeBlock(sizeIndex, free, 1);
    
    if(!checkQuickBit(free)){

        sf_header nextBlockHeader = (setNextBlock->header ^ MAGIC) & ~PREV_BLOCK_ALLOCATED;
        setNextBlock->header = (nextBlockHeader ^ MAGIC);

    }

    // if in quicklist no coalesce?
    
    coalesce(free);
}

void *sf_realloc(void *pp, size_t rsize)
{

    if (checkValidFree(pp))
    {
        sf_errno = EINVAL;
        return NULL;
    }

    /*
    When reallocating to a larger size, always follow these three steps:


    Call sf_malloc to obtain a larger block.


    Call memcpy to copy the data in the block given by the client to the block
    returned by sf_malloc.  Be sure to copy the entire payload area, but no more.


    Call sf_free on the block given by the client (inserting into a freelist
    and coalescing if required).


    Return the block given to you by sf_malloc to the client.


    If sf_malloc returns NULL, sf_realloc must also return NULL. Note that
    in this case you do not need to set sf_errno in sf_realloc because sf_malloc
    should have taken care of this.
    */
    if(rsize == 0){
        sf_free(pp);
        return NULL;
    }

    sf_block *reallocBlock = (pp - (sizeof(sf_header) + sizeof(sf_footer)));
    int size = getSize(reallocBlock);

    if (rsize > size)
    {

        void *newAllocBlock = sf_malloc(rsize);
   
        if (newAllocBlock == NULL)
        {
            return NULL;
        }

        memcpy(newAllocBlock, pp, rsize);

        sf_free(pp);

        return newAllocBlock;
    }
    else if (size > rsize)
    {

        int adjustedRsize = checkSizeAligned(rsize);
        int sizeClassIndex = findSizeClass(getSize(reallocBlock));
        sf_header freeRealloc = (reallocBlock->header ^ MAGIC) & ~THIS_BLOCK_ALLOCATED;
        reallocBlock->header = (freeRealloc ^ MAGIC);

        insertFreeBlock(sizeClassIndex, reallocBlock, 0);

        allocate(reallocBlock, adjustedRsize);

        return pp;
    }

    return NULL;
}

double sf_fragmentation()
{
    // To be implemented.
    abort();

    // excluded from test
}

double sf_utilization()
{
    // To be implemented.

    // utilization

    // peak aggregate payload over current heap size

    // max payload over heap size

    return calculateUtil();
}
