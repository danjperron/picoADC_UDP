
#include <stdio.h>
#include "fifoBlock.h"



// fifo pointer
uint16_t  head_block=0;
uint16_t  tail_block=0;

// fifo block head and tail pointer function
SampleBlockStruct block[BLOCK_MAX];

int getHeadBlock()
{
  uint16_t new_head= ((head_block + 1)  % BLOCK_MAX);
  if(new_head==tail_block)
    return -1;
  return head_block;
}

int nextHeadBlock()
{
  uint16_t new_head= ((head_block + 1)  % BLOCK_MAX);
  if(new_head>=0)
       head_block=new_head;
  return new_head;
}

int getTailBlock()
{
    if(head_block == tail_block)
      return -1;
    return tail_block;
}

int nextTailBlock()
{
  uint16_t new_tail=  getTailBlock();
  if(new_tail<0)
    return -1;
  tail_block = ((++tail_block)  % BLOCK_MAX);
  return tail_block;
}






