/*
 * Copyright (c) 2005
 * The Regents of The University of Michigan
 * All Rights Reserved
 *
 * This code is part of the M5 simulator, developed by Nathan Binkert,
 * Erik Hallnor, Steve Raasch, and Steve Reinhardt, with contributions
 * from Ron Dreslinski, Dave Greene, Lisa Hsu, Kevin Lim, Ali Saidi,
 * and Andrew Schultz.
 *
 * Permission is granted to use, copy, create derivative works and
 * redistribute this software and such derivative works for any
 * purpose, so long as the copyright notice above, this grant of
 * permission, and the disclaimer below appear in all copies made; and
 * so long as the name of The University of Michigan is not used in
 * any advertising or publicity pertaining to the use or distribution
 * of this software without specific, written prior authorization.
 *
 * THIS SOFTWARE IS PROVIDED AS IS, WITHOUT REPRESENTATION FROM THE
 * UNIVERSITY OF MICHIGAN AS TO ITS FITNESS FOR ANY PURPOSE, AND
 * WITHOUT WARRANTY BY THE UNIVERSITY OF MICHIGAN OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE. THE REGENTS OF THE UNIVERSITY OF MICHIGAN SHALL NOT BE
 * LIABLE FOR ANY DAMAGES, INCLUDING DIRECT, SPECIAL, INDIRECT,
 * INCIDENTAL, OR CONSEQUENTIAL DAMAGES, WITH RESPECT TO ANY CLAIM
 * ARISING OUT OF OR IN CONNECTION WITH THE USE OF THE SOFTWARE, EVEN
 * IF IT HAS BEEN OR IS HEREAFTER ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGES.
 */

/**
 * @file
 * Describes a tagged prefetcher based on template policies.
 */

#ifndef __MEM_CACHE_PREFETCH_RPT_PREFETCHER_HH__
#define __MEM_CACHE_PREFETCH_RPT_PREFETCHER_HH__

#include "base/misc.hh" // fatal, panic, and warn
#include "sim/host.hh" // Tick

#include "mem/cache/prefetch/prefetcher.hh"

/**
 * A template-policy based cache. The behavior of the cache can be altered by
 * supplying different template policies. TagStore handles all tag and data
 * storage @sa TagStore. Buffering handles all misses and writes/writebacks
 * @sa MissQueue. Coherence handles all coherence policy details @sa
 * UniCoherence, SimpleMultiCoherence.
 */

typedef enum {
  Initial,
  Transient,
  Steady,
  No_prediction
} rpt_state_t;


typedef struct {
  Addr pc;             /* Adress of loading instruction */
  Addr prev_addr;      /* Previous adress loaded by instruction */
  int stride;               /* Recorded stride */
  rpt_state_t state;        /* Current state */
  Tick atime;             /* Time of last access (in ticks) */
} rpt_entry_t;


template <class TagStore, class Buffering>
class RPTPrefetcher : public Prefetcher<TagStore, Buffering>
{
  protected:

    Buffering* mq;
    TagStore* tags;
    
    Tick latency;
    int degree;

    rpt_entry_t rpt[64];

  public:

    RPTPrefetcher(int size, bool pageStop, bool serialSquash,
		     bool cacheCheckPush, bool onlyData, 
		     Tick latency, int degree)
	:Prefetcher<TagStore, Buffering>(size, pageStop, serialSquash, 
					 cacheCheckPush, onlyData), 
	 latency(latency), degree(degree)
    {
         degree = 8;
    }

    ~RPTPrefetcher() {}
    
    void calculatePrefetch(MemReqPtr &req, std::list<Addr> &addresses, 
			   std::list<Tick> &delays)
    {
      int i;
      int table_size = 64;
      int correct = 0;
      Tick oldest;
      Addr newAddr;
	Addr blkAddr = req->paddr & ~(Addr)(this->blkSize-1);
      
      int index = -1;
      
      if (req->pc) {

      for (i = 0; i < table_size; i++) {
        if (req->pc == rpt[i].pc) {
          index = i;
          break;
        }
      }
       
      if (index > -1) {
        /* Entry is in table */
        /* Calculate if prediction is correct */
        if (req->pc == rpt[index].prev_addr + rpt[index].stride) {
          correct = 1;
        } else {
          correct = 0;
        }
        switch(rpt[index].state) {
          case Initial:
            if (!correct) {
              rpt[index].stride = req->pc - rpt[index].prev_addr;
              rpt[index].state = Transient;
            } else {
              rpt[index].state = Steady;
            }
            break;
          case Transient:
            if (correct) {
              rpt[index].state = Steady;
            } else {
              rpt[index].stride = req->pc - rpt[index].prev_addr;
              rpt[index].state = No_prediction;
            }
            break;
          case Steady:
            if (correct) {
              rpt[index].state = Steady;
            } else {
              rpt[index].state = Initial;
            }
            break;
          case No_prediction:
            if (correct) {
              rpt[index].state = Transient;
            } else {
              rpt[index].stride = req->pc - rpt[index].prev_addr;
              rpt[index].state = No_prediction;
            }
            break;
          default:
            panic("Something weird happened, shouldn't be in this state");
           
        }
        rpt[index].prev_addr = req->pc;
        /* Update access time */
        rpt[index].atime = req->time;
        /* If we now are in the steady state; issue prefetches! */
        if (rpt[index].state == Steady) {
          for (i=1; i<=degree; i++) {
            newAddr = req->pc + i*rpt[index].stride;
            if (this->pageStop && 
              (blkAddr & ~(TheISA::VMPageSize - 1)) !=
              (newAddr & ~(TheISA::VMPageSize - 1)))
            {
              //Spanned the page, so now stop
              this->pfSpanPage += degree - i + 1;
              return;
            }
            else
            {
              addresses.push_back(newAddr);
              delays.push_back(latency);
              DPRINTF(HWPrefetch, "Issued RPT prefetch request for %x, degree %d\n", newAddr, i);
            }
          }
        }
     } else {
      /* This entry is not in the table, so we insert it. */
      /* Find the oldest entry through linear search */
      oldest = rpt[0].atime;
      index = 0;
      for (i = 0; i < table_size; i++) {
        if (oldest > rpt[i].atime) {
          oldest = rpt[i].atime;
          index = i;
        }
      }
      /* Replace the oldest */
      rpt[index].pc = req->pc;
      rpt[index].prev_addr = blkAddr;
      rpt[index].stride = 0;
      rpt[index].atime = req->time;
      rpt[index].state = Initial;
    }
  }

      

      /* 

	for (int d=1; d <= degree; d++) {
	    Addr newAddr = blkAddr + d*(this->blkSize);
	    }
	}
      */
    }
};

#endif // __MEM_CACHE_PREFETCH_RPT_PREFETCHER_HH__
