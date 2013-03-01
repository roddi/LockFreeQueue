#### Goal

LockFreeQueue is a queue that doesn't lock the reading or the writing thread. You can put arbitrarily sized objects into the queue. My main focus is to have a way of passing data from and to the Core Audio render thread.


#### Typical usage

This example is single threaded and I don't check the return values. But you are a serious grownup programmer and know what to do, aren't you?

    LockFreeQueue *queue = new LockFreeQueue();
    queue->InitWithMaxBytesDoOverwrite(27, true);
    
    char testBuffer[15] = {'>','H','e','l','l','o',' ','W','o','r','l','d','!','<',0};
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
    
    queue->Fetch(fetchBuffer, 20, &fetchRangeList, &fetchedByteCount);
    queue->DebugPrintDataBufferList();
        
#### Note

This C++ code is OS X only (maybe iOS, I haven't tried) but the only os specific function call is the CAS for the RangeList. Should be easy to port.

Run
    doxygen
for a bit of documentation.


#### Pull requests welcome!
