
#ifndef fifoblock
#define fifoblock

#include <stdio.h>
#include <stdint.h>




#define SAMPLE_CHUNK_SIZE  600
#define SAMPLE_BYTE_SIZE  900


#ifndef BLOCK_MAX
  #define BLOCK_MAX  220
#endif

#define BLOCK_FREE   0
#define BLOCK_READY  1
#define BLOCK_LOCK   2


typedef struct{
uint32_t blockId;
uint64_t timeStamp;
uint8_t AD_Value[SAMPLE_BYTE_SIZE];
} SampleBlockStruct;


SampleBlockStruct block[BLOCK_MAX];

// fifo block head and tail pointer function
int getHeadBlock();
int getNextHeadBlock();
int getTailBlock();
void nextHeadBlock();
void nextTailBlock();

#endif

