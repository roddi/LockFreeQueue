//
//  LockFreeQueue.cpp
//
//  Created by Ruotger Deecke on 17.01.13.
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

#include "LockFreeQueue.h"

#include <libkern/OSAtomic.h>

LockFreeQueue::LockFreeQueue()
{
    // Don't do any work here but use init
}

LockFreeQueue::~LockFreeQueue()
{
    free(mDataRing);
}

#pragma mark - public


/**
 \brief Initialiser.
 \param maxBytes length of bytes of the ring buffer. You can't store more than this at the same time
 \param doOverwrite if true, empty areas of the buffer are overwritten. For debugging purposes.
 */
void LockFreeQueue::InitWithMaxBytesDoOverwrite(unsigned long maxBytes, bool doOverwrite)
{
    mDataRingLength = maxBytes;
    mDoOverwrite = doOverwrite;
    
    mDataRing = (unsigned char*)malloc(maxBytes+2);
    memset(mDataRing, '-', maxBytes);
    
    // for debugging these two byte are written
    mDataRing[maxBytes] = '|';
    mDataRing[maxBytes+1] = 0;

    memset(&mInternalRangeList, 0, sizeof(RangeList));
    mRangeList = &mInternalRangeList;
}



/**
 \brief Store a blob of data
 \param inBufferToStore buffer to store
 \param inBufferLength length of supplied buffer in inBufferToStore
 \param inReservedList RangeList used to reserve space for this store
 \param inOutRangeList RangeList to hold new state
 
 This method should only be called from the storing thread
 */
LockFreeQueueReturnCode     LockFreeQueue::Store(const char *inBufferToStore, unsigned long inBufferLength, RangeList* inReservedList, RangeList* inOutRangeList)
{
    if (inReservedList == inOutRangeList)
    {
        printf("can't use same RangeList!\n");
        return LockFreeQueue_sameRangeList;
    }
    
    if (inReservedList->mReservedRange.mLength != inBufferLength)
    {
        printf("sorry but you reserved a different length!\n");
        return LockFreeQueue_differentByteCountThanReserved;
    }
    
    OSMemoryBarrier();
    RangeList *oldRangeList = (RangeList *)mRangeList;
    
    if (oldRangeList)
    {
        if (!oldRangeList->mHasReserved
            || oldRangeList->mReservedRange.mLength != inReservedList->mReservedRange.mLength
            || oldRangeList->mReservedRange.mPosition != inReservedList->mReservedRange.mPosition)

        {
            printf("something is strange! (1)\n");
            return LockFreeQueue_fileABug;
        }
        
        memcpy(inOutRangeList, oldRangeList, sizeof(RangeList));
    }
    else
    {
        printf("something is strange! (We *should* have an old range list)\n");
        return LockFreeQueue_fileABug;        
    }
    
    Range firstByteRange;
    Range secondByteRange;

    RangePartsOfByteRange(&firstByteRange, &secondByteRange, &oldRangeList->mReservedRange);
    
    if (secondByteRange.mLength == 0)
    {
        memcpy(&(mDataRing[firstByteRange.mPosition]), inBufferToStore, inBufferLength);
    }
    else
    {
        memcpy(&(mDataRing[firstByteRange.mPosition]), inBufferToStore, firstByteRange.mLength);
        memcpy(&(mDataRing[secondByteRange.mPosition]), &(inBufferToStore[firstByteRange.mLength]), inBufferLength-firstByteRange.mLength);
    }

    inOutRangeList->mFullRanges[inOutRangeList->mFullRangeCount] = oldRangeList->mReservedRange;
    inOutRangeList->mFullRangeCount++;
    inOutRangeList->mReservedRange.mLength = 0;
    inOutRangeList->mReservedRange.mPosition = 0;
    inOutRangeList->mHasReserved = false;
    
    bool result = OSAtomicCompareAndSwapPtr(oldRangeList, inOutRangeList, (void* volatile*)&mRangeList);
    
    return result ? LockFreeQueue_OK : LockFreeQueue_casUnsuccessful;
}


/**
 \brief Fetch a blob of data
 \param inOutBuffer buffer to hold the fetched data
 \param inBufferLength length of supplied buffer in inOutBuffer
 \param inOutRangeList RangeList to hold new state
 \param outReturnedBytesCount count of bytes which are returned
 
 This method should only be called from the fetching thread
 */
LockFreeQueueReturnCode LockFreeQueue::Fetch(char *inOutBuffer, unsigned long inBufferLength, RangeList* inOutRangeList, unsigned long * outReturnedBytesCount)
{
    OSMemoryBarrier();
    RangeList *oldRangeList = (RangeList*)mRangeList;
    
    if (oldRangeList == inOutRangeList)
    {
        printf("fetch: RangeList in use!\n");
        *outReturnedBytesCount = 0;
        return LockFreeQueue_rangeListInUse;
    }
    
    if (oldRangeList->mFullRangeCount == 0)
    {
        // nothing to fetch!
        *outReturnedBytesCount = 0;
        return LockFreeQueue_empty;
    }

    if (oldRangeList->mFullRanges[0].mLength > inBufferLength)
    {
        printf("inBuffer not large enough!\n");
        *outReturnedBytesCount = 0;
        return LockFreeQueue_bufferToSmall;
    }

    Range firstRange;
    Range secondRange;
    
    RangePartsOfByteRange(&firstRange, &secondRange, &oldRangeList->mFullRanges[0]);
    
    const bool doClearBuffer = true;
    
    memcpy(inOutBuffer, &mDataRing[firstRange.mPosition], firstRange.mLength);
    
    if (secondRange.mLength)
    {
        memcpy(&inOutBuffer[firstRange.mLength], &mDataRing[secondRange.mPosition], secondRange.mLength);
    }

    inOutRangeList->mHasReserved = oldRangeList->mHasReserved;
    inOutRangeList->mReservedRange = oldRangeList->mReservedRange;
    for (unsigned long i=0; i+1<oldRangeList->mFullRangeCount ; i++)
    {
        inOutRangeList->mFullRanges[i] = oldRangeList->mFullRanges[i+1];
    }
    inOutRangeList->mFullRangeCount = oldRangeList->mFullRangeCount-1;
    
    bool result = OSAtomicCompareAndSwapPtr(oldRangeList, inOutRangeList, (void* volatile*)&mRangeList);
    
    if (result && doClearBuffer)
    {
        memset(&mDataRing[firstRange.mPosition], '-', firstRange.mLength);
        if (secondRange.mLength) memset(&mDataRing[secondRange.mPosition], '-', secondRange.mLength);
    }
    
    *outReturnedBytesCount = result ? oldRangeList->mFullRanges[0].mLength : 0;
    return result ? LockFreeQueue_OK : LockFreeQueue_casUnsuccessful;
}


/**
 \brief Copy supplied RangeList to the internal RangeList and make that the valid one.
 \param inOutBuffer buffer to hold the fetched data
 \param inBufferLength length of supplied buffer in inOutBuffer
 \param inOutRangeList RangeList to hold new state
 \param outReturnedBytesCount count of bytes which are returned
 
 This method can be used to free a locally used RangeList. It can be called from any thread
 */
LockFreeQueueReturnCode LockFreeQueue::InternalizeRangeList(RangeList* inRangeList)
{
    OSMemoryBarrier();
    RangeList *oldRangeList = (RangeList*)mRangeList;
    
    if (inRangeList != oldRangeList)
    {
        // supplied range list is not the valid one --> nothing to do
        return LockFreeQueue_OK;
    }

    memcpy(&mInternalRangeList, oldRangeList, sizeof(RangeList));
    
    bool result = OSAtomicCompareAndSwapPtr(oldRangeList, &mInternalRangeList, (void* volatile*)&mRangeList);
    
    return result ? LockFreeQueue_OK : LockFreeQueue_casUnsuccessful;
}

/**
 \brief use this method to reserve a blob of data to fill.
 \param inCount count of bytes you need to reserve
 \param inOutRangeList RangeList to hold new state
 
 This method should only be called from the storing thread
 */
LockFreeQueueReturnCode    LockFreeQueue::ReserveRange(unsigned long inCount, RangeList* inOutRangeList)
{
    OSMemoryBarrier();

    RangeList *oldRangeList = (RangeList*)mRangeList;
    
    if (oldRangeList == inOutRangeList)
    {
        printf("reserve: RangeList in use!\n");
        return LockFreeQueue_rangeListInUse;
    }
    
    if (oldRangeList)
    {
        if (oldRangeList->mHasReserved)
        {
            // someone has already reserved space, try again later!
            return LockFreeQueue_alreadyReserved;
        }
        
        if (FreeBytesWithList(oldRangeList) < inCount)
        {
            // not enough space!
            return LockFreeQueue_notEnoughSpaceLeft;
        }
        
        memcpy(inOutRangeList, oldRangeList, sizeof(RangeList));
    }
    else
    {
        memset(inOutRangeList, 0, sizeof(RangeList));
    }

    unsigned long firstReserved = FirstEmptyByteIndexWithList(inOutRangeList);
    inOutRangeList->mReservedRange.mPosition = firstReserved;
    inOutRangeList->mReservedRange.mLength = inCount;
    inOutRangeList->mHasReserved = true;
    
    bool result = OSAtomicCompareAndSwapPtr(oldRangeList, inOutRangeList, (void* volatile*)&mRangeList);
    
    if (result && true)
    {
        Range firstByteRange;
        Range secondByteRange;
        
        RangePartsOfByteRange(&firstByteRange, &secondByteRange, &inOutRangeList->mReservedRange);
        
        if (secondByteRange.mLength == 0)
        {
            memset(&(mDataRing[firstByteRange.mPosition]), 'r', inCount);
        }
        else
        {
            memset(&(mDataRing[firstByteRange.mPosition]), 'r', firstByteRange.mLength);
            memset(&(mDataRing[secondByteRange.mPosition]), 'r', inCount-firstByteRange.mLength);
        }        
    }
    
    return result ? LockFreeQueue_OK : LockFreeQueue_casUnsuccessful;
}

/**
 \brief print the content of the data buffer and range list
 
 **REALLY** only for debugging. Prints content as ascii, so it will most probably fail to do something useful with real data. This method is probably not very thread safe
 */
void  LockFreeQueue::DebugPrintDataBufferList()
{
    printf("data buffer: [%s]\n", this->mDataRing);
    printf("range list: [");
    for (unsigned long i=0; i<mRangeList->mFullRangeCount; i++)
    {
        printf("%d,%d  ", (int)mRangeList->mFullRanges[i].mPosition, (int)mRangeList->mFullRanges[i].mLength);
    }
    printf("] reserved: %s (%d,%d)\n", mRangeList->mHasReserved?"YES":"no", (int)mRangeList->mReservedRange.mPosition, (int)mRangeList->mReservedRange.mLength);
}

#pragma mark - private

void LockFreeQueue::RangePartsOfByteRange(Range *outFirstRange, Range *outSecondRange, Range *inRange)
{
    outFirstRange->mPosition = inRange->mPosition;
    
    outSecondRange->mPosition = 0;
    outSecondRange->mLength = 0;
    
    if (inRange->mPosition + inRange->mLength > mDataRingLength)
    {
        outFirstRange->mLength = mDataRingLength - inRange->mPosition;
        outSecondRange->mLength = inRange->mLength - outFirstRange->mLength;
    }
    else
    {
        outFirstRange->mLength = inRange->mLength;
    }
}

unsigned long   LockFreeQueue::EffectiveFirstDataByteIndexAfterRange(Range *inRange)
{
    Range firstRange;
    Range secondRange;
    
    RangePartsOfByteRange(&firstRange, &secondRange, inRange);
    
    unsigned long result = 0;
    
    if (secondRange.mLength)
    {
        result = secondRange.mPosition + secondRange.mLength;
    }
    else
    {
        result = firstRange.mPosition + firstRange.mLength;
    }
    
    if (result > mDataRingLength)
    {
        result -= mDataRingLength;
    }
    
    return result;
}


void LockFreeQueue::FirstFreeByteRangeWithList(Range *outRange, RangeList* inRangeList)
{
    if (inRangeList->mFullRangeCount == 0)
    {
        outRange->mPosition = 0;
        outRange->mLength = mDataRingLength;
        return;
    }
    
    outRange->mPosition =  FirstEmptyByteIndexWithList(inRangeList);
    
    if (outRange->mPosition < FirstFullByteIndexWithList(inRangeList))
    {
        outRange->mLength = FirstFullByteIndexWithList(inRangeList) - outRange->mPosition;
    }
    else
    {
        outRange->mLength = mDataRingLength - outRange->mPosition;
    }    
}

bool LockFreeQueue::SecondFreeByteRangeWithList(Range *outRange, RangeList* inRangeList)
{
    if (inRangeList->mFullRangeCount == 0)
    {
        outRange->mPosition = 0;
        outRange->mLength = 0;
        return false;
    }
    
    if (FirstEmptyByteIndexWithList(inRangeList) < FirstFullByteIndexWithList(inRangeList))
    {
        return false;
    }
    
    outRange->mPosition = 0;
    outRange->mLength = FirstFullByteIndexWithList(inRangeList);
    return true;

}

unsigned long   LockFreeQueue::FreeBytesWithList(RangeList* inRangeList)
{
    unsigned long result = 0;
    
    Range firstRange;
    Range secondRange;
    
    FirstFreeByteRangeWithList(&firstRange, inRangeList);
    
    result = firstRange.mLength;
    
    if (SecondFreeByteRangeWithList(&secondRange, inRangeList))
    {
        result += secondRange.mLength;
    }
    
    printf("free: [%d,%d][%d,%d] - %d\n", (int)firstRange.mPosition, (int)firstRange.mLength, (int)secondRange.mPosition, (int)secondRange.mLength, (int)result);
    
    return result;

}

unsigned long   LockFreeQueue::FirstEmptyByteIndexWithList(RangeList* inRangeList)
{
    if (inRangeList->mFullRangeCount == 0)
    {
        return 0;
    }
    
    Range *lastRange = &(inRangeList->mFullRanges[inRangeList->mFullRangeCount-1]);
    
    return EffectiveFirstDataByteIndexAfterRange(lastRange);
}

unsigned long   LockFreeQueue::FirstFullByteIndexWithList(RangeList* inRangeList)
{
    if (inRangeList->mFullRangeCount == 0)
    {
        return 0;
    }
    
    return inRangeList->mFullRanges[0].mPosition;
}

