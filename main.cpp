#include "LockFreeQueue.h"
#include <stdio.h>

void testSome()
{
    LockFreeQueue *queue = new LockFreeQueue();
    queue->InitWithMaxBytesDoOverwrite(27, true);
    
    char testBuffer[15] = {'>','H','e','l','l','o',' ','W','o','r','l','d','!','<',0};
    char testBuffer2[13] = {'>','K','r','e','u','z','b','e','r','g','!','<',0};
    char fetchBuffer[20] = {'X','X','X','X','X','X','X','X','X','X','X','X','X','X','X','X','X','X','X',0};
    unsigned long fetchedByteCount = 0;
    
    RangeList firstRangeListReserved;
    RangeList firstRangeList;
    RangeList fetchRangeList;
    
    printf("\n\n");
    queue->ReserveRange(14, &firstRangeListReserved);
    queue->DebugPrintDataBufferList();
    queue->Store(testBuffer, 14, &firstRangeListReserved, &firstRangeList);
    queue->DebugPrintDataBufferList();
    
    queue->ReserveRange(12, &firstRangeListReserved);
    queue->DebugPrintDataBufferList();
    queue->Store(testBuffer2, 12, &firstRangeListReserved, &firstRangeList);
    queue->DebugPrintDataBufferList();
    
    queue->Fetch(fetchBuffer, 20, &fetchRangeList, &fetchedByteCount);
    queue->DebugPrintDataBufferList();
    
    queue->ReserveRange(14, &firstRangeListReserved);
    queue->DebugPrintDataBufferList();
    queue->Store(testBuffer, 14, &firstRangeListReserved, &firstRangeList);
    queue->DebugPrintDataBufferList();
    
    queue->Fetch(fetchBuffer, 20, &fetchRangeList, &fetchedByteCount);
    queue->DebugPrintDataBufferList();
    
    queue->ReserveRange(12, &firstRangeListReserved);
    queue->DebugPrintDataBufferList();
    // --
    queue->Fetch(fetchBuffer, 20, &fetchRangeList, &fetchedByteCount);
    queue->DebugPrintDataBufferList();
    // --
    queue->Store(testBuffer2, 12, &firstRangeListReserved, &firstRangeList);
    queue->DebugPrintDataBufferList();
    
    queue->Fetch(fetchBuffer, 20, &fetchRangeList, &fetchedByteCount);
    queue->DebugPrintDataBufferList();
    
    // fetch from empty
    RangeList secondFetchRangeList;
    queue->Fetch(fetchBuffer, 20, &secondFetchRangeList, &fetchedByteCount);
    queue->DebugPrintDataBufferList();
    
    while(queue->InternalizeRangeList(&firstRangeList) != LockFreeQueue_OK)
        ;
    while(queue->InternalizeRangeList(&firstRangeListReserved) != LockFreeQueue_OK)
        ;
    while(queue->InternalizeRangeList(&fetchRangeList) != LockFreeQueue_OK)
        ;
    while(queue->InternalizeRangeList(&secondFetchRangeList) != LockFreeQueue_OK)
        ;
}


int main(int argc,const char *argv[])
{
    testSome();
}
