//
//  LockFreeQueueCocoa.m
//
//  Created by Ruotger Deecke on 01.03.13.
//
// Copyright (c) 2013, Ruotger Deecke
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// - Redistributions of source code must retain the above copyright notice, this
//   list of conditions and the following disclaimer.
// - Redistributions in binary form must reproduce the above copyright notice,
//   this list of conditions and the following disclaimer in the documentation
//   and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//

#import "LockFreeQueueCocoa.h"
#import "LockFreeQueueCocoa+CPP.h"
#include "LockFreeQueue.h"


/// \brief class extension for private stuff
///
@interface LockFreeQueueCocoa ()

@property (assign, readonly) RangeList *reserveRangeList;
@property (assign, readonly) RangeList *storeRangeList;
@property (assign, readonly) RangeList *fetchRangeListA;
@property (assign, readonly) RangeList *fetchRangeListB;
@property (assign, readonly) LockFreeQueue *lockFreeQueue;
@property (assign, readonly) char *fetchBuffer;
@property (assign, readonly) unsigned long size;

@property (assign) BOOL useANext;

@end

@implementation LockFreeQueueCocoa

- (id) initWithSize:(unsigned long)inSize;
{
    if (!(self=[super init]))
        return nil;

    _size = inSize;
    
    _lockFreeQueue = new LockFreeQueue();
    self.lockFreeQueue->InitWithMaxBytesDoOverwrite(inSize, false);
    
    _reserveRangeList = (RangeList*)malloc(sizeof(RangeList));
    _storeRangeList = (RangeList*)malloc(sizeof(RangeList));
    _fetchRangeListA = (RangeList*)malloc(sizeof(RangeList));
    _fetchRangeListB = (RangeList*)malloc(sizeof(RangeList));

    _fetchBuffer = (char*)malloc(inSize);
    
    return self;
}

- (void) dealloc
{
    while (self.lockFreeQueue->InternalizeRangeList(_reserveRangeList) == LockFreeQueue_casUnsuccessful)
    {
        [NSThread sleepForTimeInterval:0.01];
    }
    while (self.lockFreeQueue->InternalizeRangeList(_storeRangeList) == LockFreeQueue_casUnsuccessful)
    {
        [NSThread sleepForTimeInterval:0.01];
    }
    while (self.lockFreeQueue->InternalizeRangeList(_fetchRangeListA) == LockFreeQueue_casUnsuccessful)
    {
        [NSThread sleepForTimeInterval:0.01];
    }
    while (self.lockFreeQueue->InternalizeRangeList(_fetchRangeListB) == LockFreeQueue_casUnsuccessful)
    {
        [NSThread sleepForTimeInterval:0.01];
    }
    
    free(_reserveRangeList);
    free(_storeRangeList);
    free(_fetchRangeListA);
    free(_fetchRangeListB);
    free(_fetchBuffer);
    
    free(_lockFreeQueue);
    
    [super dealloc];
}

/**
 \brief fetch one packet of data
 
 */
- (NSData*) fetchData;
{
    unsigned long returnedBytesCount = 0;
    
    LockFreeQueueReturnCode returnCode =
    self.lockFreeQueue->Fetch(self.fetchBuffer,
                              self.size,
                              self.useANext ? self.fetchRangeListA : self.fetchRangeListB,
                              &returnedBytesCount);
    
    if (returnCode != LockFreeQueue_OK)
    {
        return nil;
    }

    self.useANext = !self.useANext;
    
    NSData *data = [NSData dataWithBytes:self.fetchBuffer length:returnedBytesCount];
    
    return data;
}

/**
 \brief store one packet of data
 
 **CAUTION** this can block!
 */
- (BOOL) storeData:(NSData*)inData;
{
    LockFreeQueueReturnCode returnCode = self.lockFreeQueue->ReserveRange(inData.length, self.reserveRangeList);
    
    NSAssert(returnCode != LockFreeQueue_alreadyReserved, @"reserved twice!");
    NSAssert(returnCode != LockFreeQueue_fileABug, @"file a bug!");
    NSAssert(returnCode != LockFreeQueue_rangeListInUse, @"range list in use!");
    
    if (returnCode == LockFreeQueue_notEnoughSpaceLeft ||
        returnCode == LockFreeQueue_casUnsuccessful)
    {
        return NO;
    }

    while ((returnCode = self.lockFreeQueue->Store((const char*)[inData bytes], [inData length], self.reserveRangeList, self.storeRangeList)) != LockFreeQueue_OK)
    {
        NSAssert(returnCode != LockFreeQueue_alreadyReserved, @"reserved twice!");
        NSAssert(returnCode != LockFreeQueue_fileABug, @"file a bug!");
        NSAssert(returnCode != LockFreeQueue_rangeListInUse, @"range list in use!");
    }

    return YES;
}

/**
 \brief return the LockFreeQueue object as a void pointer
 
 **CAUTION** void pointer! I hope you know what that means. But you can still access the
 LockFreeQueue object from a .m file with this method if needed.
 */
- (void*)lockFreeQueueVoid;
{
    return (void*)self.lockFreeQueue;
}

@end
