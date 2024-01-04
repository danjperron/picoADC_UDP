
#ifndef fifoblock
#define fifoblock

#include <stdio.h>
#include <stdint.h>
#include "picoADC_UDP.h"




#ifndef BLOCK_MAX
  #define BLOCK_MAX 90
#endif

#define BLOCK_FREE   0
#define BLOCK_READY  1
#define BLOCK_LOCK   2



extern SampleBlockStruct block[BLOCK_MAX];

// fifo pointer
extern int16_t  head_block;
extern int16_t  tail_block;

// fifo block head and tail pointer function
int16_t getHeadBlock(uint8_t status);
int16_t getTailBlock(uint8_t status);
int16_t getTotalBlock(uint8_t status);
int16_t getBlockId(uint32_t blockid, uint8_t status);
int16_t getTailLowerBlock(uint32_t blockID,uint8_t status);
#endif

