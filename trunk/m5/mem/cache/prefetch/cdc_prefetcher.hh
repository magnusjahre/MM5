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
 * Describes a CDC prefetcher based on template policies.
 */

#ifndef __MEM_CACHE_PREFETCH_CDC_PREFETCHER_HH__
#define __MEM_CACHE_PREFETCH_CDC_PREFETCHER_HH__

#include "base/misc.hh" // fatal, panic, and warn

#include "mem/cache/prefetch/prefetcher.hh"

/**
 * A template-policy based cache. The behavior of the cache can be altered by
 * supplying different template policies. TagStore handles all tag and data
 * storage @sa TagStore. Buffering handles all misses and writes/writebacks
 * @sa MissQueue. Coherence handles all coherence policy details @sa
 * UniCoherence, SimpleMultiCoherence.
 */
template <class TagStore, class Buffering>
class CDCPrefetcher : public Prefetcher<TagStore, Buffering>
{
  protected:

    Buffering* mq;
    TagStore* tags;
    
    Tick latency;
    int degree;

  /* CZone size in bits */
  int czone_size;

  /* Table size in entries (GHB, RPT etc) */
  int table_size;

  /* Global History buffer */
  Addr ghb[1000];

  /* Delta buffer */
  Addr delta_buffer[1000];

  /* Global History buffer top */
  int ghb_top;

  /* Global History buffer fill */
  int ghb_is_full;


  public:

    CDCPrefetcher(int size, bool pageStop, bool serialSquash,
		     bool cacheCheckPush, bool onlyData, 
		     Tick latency, int degree)
	:Prefetcher<TagStore, Buffering>(size, pageStop, serialSquash, 
					 cacheCheckPush, onlyData), 
	 latency(latency), degree(degree)
    {
        ghb_top = 0;
        ghb_is_full = 0;
        czone_size = 16;
        table_size = 1000;
        degree = 8;
    }

    ~CDCPrefetcher() {}
    
    void calculatePrefetch(MemReqPtr &req, std::list<Addr> &addresses, 
			   std::list<Tick> &delays)
    {
      int i;
      int j;
      int delta_count = 0;
      int delta_1;
      int delta_2;
      //int delta;
      Addr newAddr;

	Addr blkAddr = req->paddr & ~(Addr)(this->blkSize-1);
      // Insert refernce into GHB 
      ghb_top = (ghb_top + 1) % table_size;
      ghb[ghb_top] = blkAddr;
      if (ghb_top == 0) {
          ghb_is_full = 1;
      }

      if (ghb_is_full) {
      // Construct delta buffer 
      // GHB is a circular buffer, need two separate for loops 
      for (i = ghb_top; i >= 0; i--) {
        // If this entry matches the czone size, put it into the buffer 
        if (ghb[i] >> czone_size ==  blkAddr >> czone_size) {
          delta_buffer[delta_count] = ghb[i];
          delta_count++;
        }
      }
      for (i = table_size -1; i > ghb_top; i--) {
        // If this entry matches the czone size, put it into the buffer 
        if (ghb[i] >> czone_size ==  blkAddr >> czone_size) {
          delta_buffer[delta_count] = ghb[i];
          delta_count++;
          }
        }
       //We can only prefetch if there is enough data available 
        if (delta_count>3) {
          // Correlate deltas - Two deltas are used as Nesbit found optimal 
          delta_1 = delta_buffer[0] - delta_buffer[1];
          delta_2 = delta_buffer[1] - delta_buffer[2];
          // Search for first delta 
          for (i = 2; i < delta_count-2 ; i++) {
            if (delta_buffer[i] - delta_buffer[i+1] == delta_1) {
              if (delta_buffer[i+1] - delta_buffer[i+2] == delta_2) {
                // Pattern found 
                // Start prefetching 
                newAddr = blkAddr;
                for (j=1; j<=degree; j++) {
                   // Find next delta 
                   i--;
                   if (i < 0) {
                     break;
                   }
                   if (delta_buffer[i] > delta_buffer[i+1]) {
                     newAddr += delta_buffer[i] - delta_buffer[i+1];
                     if (this->pageStop && 
                        (blkAddr & ~(TheISA::VMPageSize - 1)) !=
                        (newAddr & ~(TheISA::VMPageSize - 1)))
                     {
                        //Spanned the page, so now stop
                        this->pfSpanPage += degree - j + 1;
                        return;
                     }
                     else
                     {
                       addresses.push_back(newAddr);
                       delays.push_back(latency);
                       DPRINTF(HWPrefetch, "Issued a CDC prefetch request for %x, degree %d\n", newAddr, j);
                     }
                     }
                 }
                 // break out of the loop 
                 break;
        }
      }
     }
    }
  }
    }
};

#endif // __MEM_CACHE_PREFETCH_CDC_PREFETCHER_HH__
