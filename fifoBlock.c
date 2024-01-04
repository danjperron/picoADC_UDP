
#include <stdio.h>
#include "fifoBlock.h"


// fifo pointer
int16_t  head_block=0;
int16_t  tail_block=0;

// fifo block head and tail pointer function
SampleBlockStruct block[BLOCK_MAX];

int16_t getBlock(int16_t pointer, uint8_t status)
{
  for(int16_t loop=0;loop<BLOCK_MAX;loop++)
     {
       int16_t  theBlock = (pointer + loop) % BLOCK_MAX;
       if(block[theBlock].status == status)
            return theBlock;
     }
  return -1;
}


int16_t getHeadBlock(uint8_t status)
{
  int16_t t=getBlock(head_block,status);
  if(t>=0)
          head_block=((t+1) % BLOCK_MAX);
    return t;
}

int16_t getTailBlock(uint8_t status)
{
  int16_t t=getBlock(tail_block,status);
  if(t>=0)
          tail_block=((t+1) % BLOCK_MAX);
    return t;
}

int16_t getTotalBlock(uint8_t status)
{
  uint16_t total=0;
  for(int loop=0;loop<BLOCK_MAX;loop++)
     {
       if(block[loop].status == status)
        total++;
     }
  return total;
}

int16_t getBlockId(uint32_t blockID, uint8_t status)
{
  if(blockID==0) return -1;
  for(int16_t loop=0;loop<BLOCK_MAX;loop++)
    if(block[loop].status == status)
      if(block[loop].blockId == blockID)
          return loop;
  return -1;
}


int16_t getTailLowerBlock(uint32_t blockID,uint8_t status)
{
  uint32_t  _tblockId = 0xffffffff;
  int16_t _tidx = -1;
  for(int16_t loop=0;loop<BLOCK_MAX;loop++)
    if(block[loop].status == status)
       {
         if(block[loop].blockId< blockID)
           {
             block[loop].status == BLOCK_FREE;
             continue;
           }
         if(block[loop].blockId < _tblockId)
            {
               _tidx = loop;
               _tblockId = block[loop].blockId;
            }
       }
  if(_tidx>=0)
       tail_block=(( _tidx + 1) % BLOCK_MAX);
  return _tidx;
}







