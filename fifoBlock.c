
#include <stdio.h>
#include "fifoBlock.h"



// fifo pointer
uint16_t  head_block=0;
uint16_t  tail_block=0;

// fifo block head and tail pointer function
SampleBlockStruct block[BLOCK_MAX];

int getBlock(uint16_t pointer, uint16_t status)
{
  for(int loop=0;loop<BLOCK_MAX;loop++)
     {
       uint16_t  theBlock = (pointer + loop) % BLOCK_MAX;
       if(block[theBlock].status & status)
            return theBlock;
     }
  return -1;
}


int getHeadBlock(uint16_t status)
{
    return( getBlock(head_block++,status));
}

int getTailBlock(uint16_t status)
{
    return( getBlock(tail_block++,status));
}

int getTotalBlock(uint16_t status)
{
  int total=0;
  for(int loop=0;loop<BLOCK_MAX;loop++)
     {
       if(block[loop].blockId==0)  // blockId==0 invalid
       continue;
       if(block[loop].status & status)
        total++;
     }
  return total;
}

int getBlockId(uint32_t blockID, uint16_t status)
{
  if(blockID==0) return -1;
  for(int loop=0;loop<BLOCK_MAX;loop++)
    if(block[loop].status & status)
      if(block[loop].blockId == blockID)
          return loop;
  return -1;
}


