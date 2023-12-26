
#ifndef fifoblock
#define fifoblock

#include <stdio.h>
#include <stdint.h>




#define SAMPLE_CHUNK_SIZE 200


#ifndef BLOCK_MAX
  #define BLOCK_MAX  400
#endif

#define BLOCK_FREE   0
#define BLOCK_READY  1
#define BLOCK_LOCK   2


typedef struct{
uint32_t blockId;
uint64_t timeStamp;
uint16_t AD_Value[SAMPLE_CHUNK_SIZE];
} SampleBlockStruct;


SampleBlockStruct block[BLOCK_MAX];

// fifo block head and tail pointer function
int getHeadBlock();
int getTailBlock();
void nextHeadBlock();
void nextTailBlock();

#endif

