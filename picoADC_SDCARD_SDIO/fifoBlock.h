
#ifndef fifoblock
#define fifoblock

#include <stdio.h>
#include <stdint.h>
#include "picoADC_SDCARD.h"




#ifndef BLOCK_MAX
  #define BLOCK_MAX 42
#endif

#define BLOCK_FREE   0
#define BLOCK_READY  1
#define BLOCK_LOCK   2



extern SampleBlockStruct block[BLOCK_MAX];

// fifo pointer
extern uint16_t  head_block;
extern uint16_t  tail_block;

// fifo block head and tail pointer function
int getHeadBlock(uint16_t status);
int getTailBlock(uint16_t status);
int getTotalBlock(uint16_t status);
int getBlockId(uint32_t blockid, uint16_t status);
int getTailLowerBlock(uint16_t status,uint32_t blockID);
#endif

