
#include <stdio.h>
#include "fifoBlock.h"


// fifo pointer
uint16_t  head_block=0;
uint16_t  tail_block=0;

// fifo block head and tail pointer function
SampleBlockStruct block[BLOCK_MAX];

int getHeadBlock()
{
    int new_head = (head_block + 1) % BLOCK_MAX;
    if(new_head == tail_block)
       return -1;
    return head_block;
}

int getNextHeadBlock()
{
    int new_head = (head_block + 1) % BLOCK_MAX;
    if(new_head == tail_block)
       return -1;
    return new_head;
}

int getTailBlock()
{
    if( tail_block == head_block)
        return -1;
    return tail_block;
}


void nextHeadBlock()
{
   head_block = (head_block + 1) % BLOCK_MAX;
}

void nextTailBlock()
{
  tail_block = (tail_block + 1) % BLOCK_MAX;
}









