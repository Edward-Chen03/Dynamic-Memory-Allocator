#include "helper.h"
#include "debug.h"

// definitiions
#define minSize 32

sf_block *epilogue; // global definition, fix repeating new after extending heap due to malloc overload. also useful block

// other useful global definitions would probably include general heap size, payload size and other stuff
double maxPayloadSize = 0;
double currentPayloadSize = 0;

int getSize(sf_block *block)
{

    if (block->header == 0)
    {
        // null block?
        return 0;
    }

    int size = (block->header ^ MAGIC) & ~(IN_QUICK_LIST | THIS_BLOCK_ALLOCATED | PREV_BLOCK_ALLOCATED);

    return size;
}

void *getNextBlock(sf_block *block)
{

    return ((void *)block + (getSize(block) & ~(IN_QUICK_LIST | THIS_BLOCK_ALLOCATED | PREV_BLOCK_ALLOCATED)));
}

void *getPreviousBlock(sf_block *block)
{
    // how to check if the previous block is the prologue?
    if ((block->prev_footer) == 0)
    {
        return (sf_mem_start());
    }
    else
    {
        return ((void *)block - ((block->prev_footer ^ MAGIC) & ~(IN_QUICK_LIST | THIS_BLOCK_ALLOCATED | PREV_BLOCK_ALLOCATED)));
    }
}

int checkPrevAllocBit(sf_block *block)
{
    if (((block->header ^ MAGIC) & PREV_BLOCK_ALLOCATED) == PREV_BLOCK_ALLOCATED)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

int checkAllocBit(sf_block *block)
{

    if (((block->header ^ MAGIC) & THIS_BLOCK_ALLOCATED) == THIS_BLOCK_ALLOCATED)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

int checkQuickBit(sf_block *block)
{
    if (((block->header ^ MAGIC) & IN_QUICK_LIST) == IN_QUICK_LIST)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

int findSizeClass(int size)
{

    if (size == minSize)
    {
        return 0;
    }
    else if (size <= 2 * minSize)
    {
        return 1;
    }
    else if (size <= 4 * minSize)
    {
        return 2;
    }
    else if (size <= 8 * minSize)
    {
        return 3;
    }
    else if (size <= 16 * minSize)
    {
        return 4;
    }
    else if (size <= 32 * minSize)
    {
        return 5;
    }
    else if (size <= 64 * minSize)
    {
        return 6;
    }
    else if (size <= 128 * minSize)
    {
        return 7;
    }
    else if (size <= 256 * minSize)
    {
        return 8;
    }
    else if (size > 256 * minSize)
    {
        return 9;
    }
    else
    {
        // error
        return -1;
    }
}

int findSizeQuick(int size)
{
    switch (size)
    {
    case 32:
        return 0;
        break;
    case 48:
        return 1;
        break;
    case 64:
        return 2;
        break;
    case 80:
        return 3;
        break;
    case 96:
        return 4;
        break;
    case 112:
        return 5;
        break;
    case 128:
        return 6;
        break;
    case 144:
        return 7;
        break;
    case 160:
        return 8;
        break;
    case 176:
        return 8;
        break;
    default:
        // error
        return -1;
        break;
    }
}

int checkSizeAligned(size_t sizeInput)
{

    // size must be aligned to a multiple of 16 and be above 32 which is minimum

    while (sizeInput % 16 != 0 || sizeInput < minSize)
    {

        sizeInput++;
    }

    return sizeInput;
}

void *searchFreeList(int size)
{

    // index
    int indexSize = findSizeClass(size);

    // start at this index and find block
    sf_block *start;
    for (int i = indexSize; i <= (NUM_FREE_LISTS - 1); i++)
    {

        start = &sf_free_list_heads[i];

        while (start->body.links.next != &sf_free_list_heads[i])
        {
            start = start->body.links.next;
            int freeBlockSize = getSize(start);
            if (size <= freeBlockSize)
            {
                return start;
            }
        }
    }

    return NULL;
}

void *searchQuickList(int size)
{
    // figure out later?
    int indexSize = findSizeQuick(size);

    if (indexSize == -1)
    {
        return NULL;
    }

    int length = sf_quick_lists[indexSize].length;

    if (length > 0)
    {
        return sf_quick_lists[indexSize].first;
    }

    return NULL;
}

void removeFromFree(sf_block *freeBlock)
{
    freeBlock->body.links.prev->body.links.next = freeBlock->body.links.next;
    freeBlock->body.links.next->body.links.prev = freeBlock->body.links.prev;
    freeBlock->body.links.prev = NULL;
    freeBlock->body.links.next = NULL;
}

void flushQuickList(int sizeIndex)
{

    sf_block *quickListStart = sf_quick_lists[sizeIndex].first;

    while (quickListStart != NULL)
    {

        int sizeIndex = findSizeClass(getSize(quickListStart));
        sf_header flushQuick = (quickListStart->header ^ MAGIC) & ~IN_QUICK_LIST & ~THIS_BLOCK_ALLOCATED;
        sf_block *temp = quickListStart->body.links.next;
        sf_quick_lists[sizeIndex].first = temp;
        quickListStart->header = flushQuick ^ MAGIC;

        sf_quick_lists[sizeIndex].length--;
        sf_block *setNextBlock = getNextBlock(quickListStart);
        setNextBlock->prev_footer = quickListStart->header;
        insertFreeBlock(sizeIndex, quickListStart, 0);

        coalesce(quickListStart);

        quickListStart = temp;
    }
}

void insertFreeBlock(int index, sf_block *block, int mallocBit)
{

    // condition to switch between quick list and free lists?

    // SPLITTING FROM MALLOC should NOT be added into quick list. uhh idk how to handle this yet

    // malloc bit, any insertFree calls from ANY part of malloc will disable the check for quick index.
    // only the insertcall from sf_free will allow a check for the quick list to happen
    // coalesce blocks should also not be checked for quick lists
    // coalesce is only called after extending heap(malloc) or for the free block (if free block in quicklist then theres already a check)
    // coalesce is also called in realloc, more on this later

    // check size for quick list
    int quickIndex = findSizeQuick(getSize(block));

    if (mallocBit)
    {
        currentPayloadSize = currentPayloadSize - (getSize(block) - sizeof(sf_header));
        if (currentPayloadSize > maxPayloadSize)
        {
            maxPayloadSize = currentPayloadSize;
        }
    }

    if (quickIndex != -1 && mallocBit)
    {
        // if the input of this block were to go over the quick list limit, we must flush that list according to doc

        // they are first entered into the free list and coalesced each time.
        // set quick list bit, set to allocated
        sf_header quickListHeader = (block->header ^ MAGIC) | IN_QUICK_LIST | THIS_BLOCK_ALLOCATED;
        block->header = quickListHeader ^ MAGIC;

        if (sf_quick_lists[quickIndex].length == QUICK_LIST_MAX)
        {

            flushQuickList(quickIndex);
        }

        if (sf_quick_lists[quickIndex].first == NULL)
        {
            // nothing in quick list
            sf_quick_lists[quickIndex].first = block;
        }
        else
        {
            sf_block *start = sf_quick_lists[quickIndex].first;
            while (start->body.links.next != NULL)
            {
                start = start->body.links.next;
            }

            start->body.links.next = block;
        }

        sf_quick_lists[quickIndex].length++;
    }
    else
    {

        sf_block *freeList = &sf_free_list_heads[index];

        // set previous head
        block->body.links.prev = freeList;
        block->body.links.next = freeList->body.links.next;

        freeList->body.links.next = block;
        (block->body.links.next)->body.links.prev = block;
    }
}

void initializeHeap()
{

    // grow heap, first malloc call

    sf_mem_grow();

    // set free list head sentinel pointers

    for (int i = 0; i < NUM_FREE_LISTS; i++)
    {
        // no header
        sf_free_list_heads[i].header = 0;

        sf_free_list_heads[i].body.links.next = &sf_free_list_heads[i];
        sf_free_list_heads[i].body.links.prev = &sf_free_list_heads[i];
    }

    // create prologue and epilogue "blocks"

    sf_block *prologue = sf_mem_start();
    sf_header prologueHeader = minSize | THIS_BLOCK_ALLOCATED | PREV_BLOCK_ALLOCATED;
    prologue->header = prologueHeader ^ MAGIC;

    epilogue = ((void *)sf_mem_end() - (sizeof(sf_header) + sizeof(sf_footer))); // should be 8 bytes from the end

    sf_header epilogueHeader = 0 | THIS_BLOCK_ALLOCATED;
    epilogue->header = (epilogueHeader ^ MAGIC);

    // set the rest as free and in to free lists, not quick
    int freeSize = (PAGE_SZ - minSize - (sizeof(sf_footer) + sizeof(sf_header)));
    sf_block *freeBlock = (sf_mem_start() + minSize);
    sf_header freeBlockHeader = freeSize | PREV_BLOCK_ALLOCATED;
    freeBlock->header = freeBlockHeader ^ MAGIC;
    epilogue->prev_footer = freeBlock->header;

    // free rest of heap into a big free block
    int sizeClassIndex = findSizeClass(freeSize);
    insertFreeBlock(sizeClassIndex, freeBlock, 0);
}

void allocate(sf_block *freeBlock, int size)
{

    if (checkQuickBit(freeBlock))
    {

        // block should be same size so just remove
        sf_header removeQHeader = (freeBlock->header ^ MAGIC) & ~IN_QUICK_LIST;
        freeBlock->header = (removeQHeader ^ MAGIC);
        int quickIndex = findSizeQuick(size);
        sf_quick_lists[quickIndex].length--;

        sf_quick_lists[quickIndex].first = sf_quick_lists[quickIndex].first->body.links.next;
    }
    else
    {

        int prevSize = getSize(freeBlock);
        // have to check if the reaminder is larger than 32

        if ((freeBlock->header ^ MAGIC) > size && (prevSize - (size) > 32))
        {
            sf_block *newAllocated = freeBlock;
            freeBlock = ((void *)freeBlock) + size;
            // remove this block from free lists
            removeFromFree(newAllocated);

            sf_header newAllocatedHeader = size | THIS_BLOCK_ALLOCATED;
            if (checkPrevAllocBit(newAllocated))
            {
                newAllocatedHeader |= PREV_BLOCK_ALLOCATED;
            }
            newAllocated->header = newAllocatedHeader ^ MAGIC;

            // set new free block
            sf_header freeBlockHeader = (prevSize - size) | PREV_BLOCK_ALLOCATED;
            freeBlock->header = (freeBlockHeader ^ MAGIC);

            sf_block *afterNextBlock = getNextBlock(freeBlock);
            afterNextBlock->prev_footer = freeBlock->header;

            freeBlock->prev_footer = (newAllocated->header);

            currentPayloadSize = currentPayloadSize + (getSize(newAllocated) - sizeof(sf_header));
            if (currentPayloadSize > maxPayloadSize)
            {
                maxPayloadSize = currentPayloadSize;
            }

            // need to put this new free block back in to free lists
            int freeListSize = findSizeClass((prevSize - size));
            insertFreeBlock(freeListSize, freeBlock, 0);
            coalesce(freeBlock);
        }
        else
        {
            // block is perfect match?, cant be lower since we had that check before in finding the free block, just change info

            // this indicates that either splitting would result in a block lower than MIN SIZE or that the block is a perfect match for data

            // just update block according

            sf_header changeBlock = (prevSize) | THIS_BLOCK_ALLOCATED;
            if (checkPrevAllocBit(freeBlock))
            {
                changeBlock |= PREV_BLOCK_ALLOCATED;
            }

            freeBlock->header = (changeBlock ^ MAGIC);

            removeFromFree(freeBlock);
            currentPayloadSize = currentPayloadSize + (getSize(freeBlock) - sizeof(sf_header));
            if (currentPayloadSize > maxPayloadSize)
            {
                maxPayloadSize = currentPayloadSize;
            }

            if (getNextBlock(freeBlock) == epilogue)
            {
                sf_header newEpiHeader = (epilogue->header ^ MAGIC) | PREV_BLOCK_ALLOCATED;
                epilogue->header = (newEpiHeader ^ MAGIC);
            }
        }
    }
}

void *coalesce(sf_block *block)
{

    if (checkAllocBit(block) || checkQuickBit(block))
    {
        return block;
    }
    // 4 cases

    /*
    1. The previous and next blocks are both allocated.
    2. The previous block is allocated and the next block is free.
    3. The previous block is free and the next block is allocated.
    4. The previous and next blocks are both free
    */

    sf_block *previousBlock = getPreviousBlock(block);
    sf_block *nextBlock = getNextBlock(block);

    int prevSize = getSize(previousBlock);
    int currentSize = getSize(block);
    int nextSize = getSize(nextBlock);

    int checkPreviousBlockAlloc = checkAllocBit(previousBlock);
    int checkNextBlockAlloc = checkAllocBit(nextBlock);

    int size = 0;

    if (checkPreviousBlockAlloc && checkNextBlockAlloc)
    {
        // case 1

        // nothing to do?
        return block;
    }
    else if (checkPreviousBlockAlloc && !checkNextBlockAlloc)
    {
        // case 2
        removeFromFree(block);
        removeFromFree(nextBlock);

        size = currentSize + nextSize;
        sf_header newBlockHeader = (size);
        if (checkPrevAllocBit(getPreviousBlock(block)))
        {
            newBlockHeader |= PREV_BLOCK_ALLOCATED;
        }
        block->header = (newBlockHeader ^ MAGIC);

        sf_block *afterNextBlock = getNextBlock(nextBlock);
        afterNextBlock->prev_footer = block->header;
    }
    else if (!checkPreviousBlockAlloc && checkNextBlockAlloc)
    {
        // case 3
        size = prevSize + currentSize;
        removeFromFree(previousBlock);
        removeFromFree(block);

        sf_header newBlockHeader = size;
        if (checkPrevAllocBit(getPreviousBlock(block)))
        {
            newBlockHeader |= PREV_BLOCK_ALLOCATED;
        }
        block = previousBlock;
        block->header = (newBlockHeader ^ MAGIC);
        block->prev_footer = previousBlock->prev_footer;

        nextBlock->prev_footer = block->header;
    }
    else
    {
        // last case
        removeFromFree(previousBlock);
        removeFromFree(block);
        removeFromFree(nextBlock);

        block = previousBlock;
        size = prevSize + currentSize + nextSize;
        sf_header newBlockHeader = (size);
        if (checkPrevAllocBit(previousBlock))
        {
            newBlockHeader |= PREV_BLOCK_ALLOCATED;
        }

        block->header = (newBlockHeader ^ MAGIC);

        sf_block *afterNextBlock = getNextBlock(nextBlock);
        afterNextBlock->prev_footer = block->header;
    }

    int sizeIndex = findSizeClass(size);
    insertFreeBlock(sizeIndex, block, 0);

    return block;
}

int extendHeap()
{

    // heap needs to be grown as no block was found, if heap was extended pass range then set return null? idk

    // return 1 on sucess and 0 on failure to grow heap

    if (sf_mem_grow() == NULL)
    {
        return 0;
    }

    // set new epilogue as well change the new block space to coalsce with any preceding blocks.
    sf_block *newMemBlock = epilogue;
    sf_header extendHeapHeader = (PAGE_SZ);
    if (checkPrevAllocBit(newMemBlock))
    {
        extendHeapHeader |= PREV_BLOCK_ALLOCATED;
    }
    newMemBlock->header = (extendHeapHeader ^ MAGIC);
    insertFreeBlock(findSizeClass(PAGE_SZ), newMemBlock, 0);

    // newMemBlock should be the new page, old epilogue location

    epilogue = ((void *)sf_mem_end() - (sizeof(sf_header) + sizeof(sf_footer)));
    sf_header epilogueHeader = 0 | THIS_BLOCK_ALLOCATED;
    epilogue->header = (epilogueHeader ^ MAGIC);
    epilogue->prev_footer = (newMemBlock->header);

    // put them together, previous block location should have been stored in the previous epilogue's prev footer which the new blcok shuld have

    newMemBlock = coalesce(newMemBlock);

    return 1;
}

int checkValidFree(void *pp)
{
    if (pp == NULL || (uintptr_t)pp % 16 != 0)
    {
        return 1;
    }
    sf_block *block = (pp - (sizeof(sf_header) + sizeof(sf_footer)));
    if (getSize(block) < minSize || getSize(block) % 16 != 0)
    {
        return 1;
    }
    int checkAlloc = checkAllocBit(block);
    int checkQuick = checkQuickBit(block);
    int checkPrevAlloc = checkPrevAllocBit(block);
    int prevBlockAlloc = checkAllocBit(getPreviousBlock(block));

    if (((void *)block < sf_mem_start()) || ((void *)block > sf_mem_end()) || !checkAlloc || checkQuick || (!checkPrevAlloc && prevBlockAlloc))
    {
        return 1;
    }

    return 0;
}

double calculateUtil()
{

    if (sf_mem_start() == sf_mem_end())
    {
        // nothing initialized
        return 0;
    }

    return maxPayloadSize / (sf_mem_end() - sf_mem_start());
}