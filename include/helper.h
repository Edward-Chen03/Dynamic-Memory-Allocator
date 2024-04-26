#ifndef HELPER_H
#define HELPER_H

#include "sfmm.h" 
#include "debug.h"

int findSizeClass(int size);
int checkSizeAligned(size_t sizeInput);
void initializeHeap();
int extendHeap();
int findSizeQuick(int size);
int findSizeClass(int size);
void* searchQuickList(int size);
void* searchFreeList(int size);
void allocate(sf_block* freeBlock, int size);
void *coalesce(sf_block *block);
void insertFreeBlock(int index, sf_block *block, int mallocBit);
void* getNextBlock(sf_block* block);
int getSize(sf_block* block);
int checkPrevAllocBit(sf_block* block);
void *getPreviousBlock(sf_block *block);
int checkValidFree(void* pp);
void flushQuickList(int sizeIndex);
int checkAllocBit(sf_block *block);
int checkQuickBit(sf_block *block);
void removeFromFree(sf_block *freeBlock);
double calculateUtil();

#endif