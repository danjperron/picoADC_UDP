
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
       if(block[theBlock].status == status)
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
//       printf("block[%d]:%d%c",loop,block[loop].status, loop%5 ? '\t' : '\n');
       if(block[loop].status == status)
        total++;
     }
//  printf("\n");
  return total;
}

int getBlockId(uint32_t blockID, uint16_t status)
{
  if(blockID==0) return -1;
  for(int loop=0;loop<BLOCK_MAX;loop++)
    if(block[loop].status == status)
      if(block[loop].blockId == blockID)
          return loop;
  return -1;
}


int getTailLowerBlock(uint16_t status,uint32_t blockID)
{
  uint32_t  _tblockId = 0xffffffff;
  int _tidx = -1;
  for(int loop=0;loop<BLOCK_MAX;loop++)
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
       tail_block=_tidx;
  return _tidx;
}







