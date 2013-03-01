//
//  LockFreeQueue.h
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

#ifndef __LockFreeQueue__
#define __LockFreeQueue__

const static unsigned long kMaxMessageCount = 100; //!< hardcoded max messge the RangeList can hold 

/// \enum LockFreeQueueReturnCode
/// \brief return code
typedef enum
{
    LockFreeQueue_OK = 0,
    LockFreeQueue_empty,                //!< can't fetch, nothing to fetch
    LockFreeQueue_bufferToSmall,        //!< can't fetch, supplied buffer too small
    LockFreeQueue_notEnoughSpaceLeft,   //!< can't reserve space, not enough space left
    LockFreeQueue_alreadyReserved,      //!< can't reserve space, space is already reserved
    LockFreeQueue_sameRangeList,        //!< can't store, the two supplied RangedLists are actually the same
    LockFreeQueue_differentByteCountThanReserved,        //!< can't store, you try to store a different count of bytes than you reserved
    LockFreeQueue_rangeListInUse,       //!< operation unsuccessful, supplied RangeList is in use
    LockFreeQueue_casUnsuccessful,      //!< operation unsuccessful, CAS operation was unsuccessful, try again.
    LockFreeQueue_fileABug              //!< operation failed in a way that might justify filing a bug report.
} LockFreeQueueReturnCode;

/// Range of elements.
typedef struct {
    unsigned long mPosition; //!< position of the first element of the range
    unsigned long mLength; //!< length of the range. position + length is first element not belonging to the range.
} Range;

/// \brief Master structure that fully describes the state of the data ring.
///
/// There is only one
/// valid RangeList at a time, and it is exchanged with cas operation. The callers
/// are responsible to supply such a structure for all calls and keep it save while
/// the LockFreeQueue is live. You can't supply the same RngeList multiple times
/// i.e. you can't supply the valid RangeList again.
typedef struct {
    bool mHasReserved;      //!< if true mReservedRange is a actually a valid range you can put data into
    Range mReservedRange;   //!< range you can put data into
    unsigned long mFullRangeCount;  //!< count of valid full ranges
    Range mFullRanges[kMaxMessageCount]; //!< full ranges
} RangeList;

class LockFreeQueue
{
private:
    RangeList  * volatile mRangeList; // CASed!
    
    RangeList mInternalRangeList;

    unsigned char *mDataRing;
    unsigned long mDataRingLength;
    bool  mDoOverwrite;
    
public:
	LockFreeQueue();
	~LockFreeQueue();
    void InitWithMaxBytesDoOverwrite(unsigned long maxBytes, bool doOverwrite);

    // list-based
    LockFreeQueueReturnCode     ReserveRange(unsigned long inCount, RangeList* inOutRangeList);
    LockFreeQueueReturnCode     Store(const char *inBufferToStore, unsigned long inBufferLength, RangeList* inReservedList, RangeList* inOutRangeList);
    LockFreeQueueReturnCode     Fetch(char *inOutBuffer, unsigned long inBufferLength, RangeList* inOutRangeList, unsigned long * outReturnedBytesCount);
    LockFreeQueueReturnCode     InternalizeRangeList(RangeList* inRangeList);
    void                        DebugPrintDataBufferList();
    
private:

    void            RangePartsOfByteRange(Range *outFirstRange, Range *outSecondRange, Range *inRange);
    unsigned long   EffectiveFirstDataByteIndexAfterRange(Range *inRange);
     
    void            FirstFreeByteRangeWithList(Range *outRange, RangeList* inRangeList);
    bool            SecondFreeByteRangeWithList(Range *outRange, RangeList* inRangeList);
    unsigned long   FreeBytesWithList(RangeList* inRangeList);
    unsigned long   FirstEmptyByteIndexWithList(RangeList* inRangeList);
    unsigned long   FirstFullByteIndexWithList(RangeList* inRangeList);
};

#endif /* defined(__LockFreeQueue__) */
