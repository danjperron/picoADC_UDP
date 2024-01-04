
#ifndef fifoblock
#define fifoblock

#include <stdio.h>
#include <stdint.h>


#define SAMPLE_CHUNK_SIZE  600
#define SAMPLE_BYTE_SIZE   1200


#ifndef BLOCK_MAX
  #define BLOCK_MAX  100
#endif



typedef struct{
uint32_t blockId;
uint64_t timeStamp;
uint8_t AD_Value[SAMPLE_BYTE_SIZE];
} SampleBlockStruct;



extern SampleBlockStruct block[BLOCK_MAX];

// fifo pointer
extern uint16_t  head_block;
extern uint16_t  tail_block;

// fifo block head and tail pointer function
int getHeadBlock();
int getTailBlock();
int nextHeadBlock();
int nextTailBlock();

#endif

