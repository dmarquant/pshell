#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>

#define LINE_BUFFER_BLOCK_SIZE 300//1024 * 1024
#define LINE_CAPACITY 8192


struct memory_block
{
    memory_block* NextBlock;
    char* NextFreeLocation;

    char Data[];
};

struct line
{
    char* String;

    // Size in bytes
    int Size;
};

struct line_buffer
{
    memory_block* FirstBlock;

    int NumLines;
    int LineCapacity;
    line* Lines;
};

void
LineBufferInit(line_buffer* LineBuffer)
{
    char* Block = (char*)malloc(LINE_BUFFER_BLOCK_SIZE);
    LineBuffer->FirstBlock = (memory_block*)Block;
    LineBuffer->FirstBlock->NextBlock = 0;
    LineBuffer->FirstBlock->NextFreeLocation = Block + offsetof(memory_block, Data);

    LineBuffer->NumLines = 0;
    LineBuffer->LineCapacity = LINE_CAPACITY;
    LineBuffer->Lines = (line*)malloc(LineBuffer->LineCapacity * sizeof(line));
}

void
LineBufferAddLine(line_buffer* LineBuffer, const char* CStr)
{
    memory_block* CurrentBlock = LineBuffer->FirstBlock;
    while (CurrentBlock->NextBlock)
        CurrentBlock = CurrentBlock->NextBlock;

    int CapacityLeft = LINE_BUFFER_BLOCK_SIZE - (CurrentBlock->NextFreeLocation - (char*)CurrentBlock);

    int Length = strlen(CStr);

    if (Length > CapacityLeft)
    {
        // TODO: Alloc new block
        printf("Block full\n");

        char* NewBlock = (char*)malloc(LINE_BUFFER_BLOCK_SIZE);
        CurrentBlock->NextBlock = (memory_block*)NewBlock;
        CurrentBlock = CurrentBlock->NextBlock;
        CurrentBlock->NextFreeLocation = NewBlock + offsetof(memory_block, Data);
        CurrentBlock->NextBlock = 0;
    }

    // TODO: Check capacity
    if (LineBuffer->LineCapacity == LineBuffer->NumLines)
    {
        LineBuffer->LineCapacity += LINE_CAPACITY;
        LineBuffer->Lines = (line*)realloc(LineBuffer->Lines, LineBuffer->LineCapacity * sizeof(line));
    }
    
    line* NewLine = &LineBuffer->Lines[LineBuffer->NumLines];
    NewLine->String = CurrentBlock->NextFreeLocation;
    NewLine->Size   = Length;

    LineBuffer->NumLines++;

    memcpy(CurrentBlock->NextFreeLocation, CStr, Length);

    CurrentBlock->NextFreeLocation += Length;
}

void
LineBufferAppendToCurrentLine(line_buffer* LineBuffer, const char* Str, int Size)
{
    memory_block* CurrentBlock = LineBuffer->FirstBlock;
    while (CurrentBlock->NextBlock)
        CurrentBlock = CurrentBlock->NextBlock;

    line* CurrentLine = &LineBuffer->Lines[LineBuffer->NumLines-1];

    int CapacityLeft = LINE_BUFFER_BLOCK_SIZE - (CurrentBlock->NextFreeLocation - (char*)CurrentBlock);
    if (CapacityLeft > Size)
    {

        memcpy(CurrentLine->String + CurrentLine->Size, Str, Size);
        CurrentLine->Size += Size;

        CurrentBlock->NextFreeLocation += Size;
    }
    else
    {
        int NewSize = CurrentLine->Size + Size;

        char* NewBlock = (char*)malloc(LINE_BUFFER_BLOCK_SIZE);
        CurrentBlock->NextBlock = (memory_block*)NewBlock;
        CurrentBlock = CurrentBlock->NextBlock;
        CurrentBlock->NextFreeLocation = NewBlock + offsetof(memory_block, Data);
        CurrentBlock->NextBlock = 0;

        memcpy(CurrentBlock->NextFreeLocation, CurrentLine->String, CurrentLine->Size);
        CurrentLine->String = CurrentBlock->NextFreeLocation;
        CurrentBlock->NextFreeLocation += CurrentLine->Size;
        
        memcpy(CurrentBlock->NextFreeLocation, Str, Size);
        CurrentBlock->NextFreeLocation += Size;

        CurrentLine->Size = NewSize;
    }
}

#if 0
int main(int argc, char** argv)
{
    line_buffer LineBuffer = {};
    LineBufferInit(&LineBuffer);

    for (int i = 0; i < 100000; i++) 
    {
        LineBufferAddLine(&LineBuffer, "Hello, world");
        LineBufferAddLine(&LineBuffer, "My name is David");
        LineBufferAddLine(&LineBuffer, "Ich bin ein Berliner");
        LineBufferAddLine(&LineBuffer, "I have a dream");
        LineBufferAddLine(&LineBuffer, "Yes we can");
        LineBufferAddLine(&LineBuffer, "Niemand will eine Mauer bauen");
    }

    for (int I = 0; I < LineBuffer.NumLines; I += 1)
    {
        printf("Line(%d): %.*s\n", I, LineBuffer.Lines[I].Size, LineBuffer.Lines[I].String);
    }

    int NumBlocks = 1;
    memory_block* CurrentBlock = LineBuffer.FirstBlock;
    while (CurrentBlock)
    {
        CurrentBlock = CurrentBlock->NextBlock;
        NumBlocks++;
    }

    printf("%d blocks allocated\n", NumBlocks);

    char c = fgetc(stdin);
}
#endif
