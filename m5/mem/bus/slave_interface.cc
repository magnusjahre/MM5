/*
 * Copyright (c) 2004, 2005
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
 * template instantiations of the slave interface.
 */

#include "mem/config/cache.hh"
#include "mem/config/compression.hh"

// Memories
#include "mem/timing/simple_mem_bank.hh"
#include "mem/cache/cache.hh"

#include "mem/cache/tags/cache_tags.hh"

#if defined(USE_CACHE_LRU)
#include "mem/cache/tags/lru.hh"
#endif

#if defined(USE_CACHE_FALRU)
#include "mem/cache/tags/fa_lru.hh"
#endif

#if defined(USE_CACHE_IIC)
#include "mem/cache/tags/iic.hh"
#endif

#if defined(USE_CACHE_SPLIT)
#include "mem/cache/tags/split.hh"
#endif

#if defined(USE_CACHE_SPLIT_LIFO)
#include "mem/cache/tags/split_lifo.hh"
#endif

#include "base/compression/null_compression.hh"
#if defined(USE_LZSS_COMPRESSION)
#include "base/compression/lzss_compression.hh"
#endif

#include "mem/cache/miss/miss_queue.hh"
#include "mem/cache/miss/blocking_buffer.hh"

#include "mem/cache/coherence/uni_coherence.hh"
#include "mem/cache/coherence/simple_coherence.hh"

// Busses
#include "mem/bus/bus.hh"

#include "mem/bus/slave_interface_impl.hh"

#ifndef DOXYGEN_SHOULD_SKIP_THIS

template class SlaveInterface<SimpleMemBank<NullCompression>, Bus>;
#if defined(USE_LZSS_COMPRESSION)
template class SlaveInterface<SimpleMemBank<LZSSCompression>, Bus>;
#endif

#if defined(USE_CACHE_FALRU)
template class SlaveInterface<Cache<CacheTags<FALRU,NullCompression>, BlockingBuffer, SimpleCoherence>, Bus>;
template class SlaveInterface<Cache<CacheTags<FALRU,NullCompression>, BlockingBuffer, UniCoherence>, Bus>;
template class SlaveInterface<Cache<CacheTags<FALRU,NullCompression>, MissQueue, SimpleCoherence>, Bus>;
template class SlaveInterface<Cache<CacheTags<FALRU,NullCompression>, MissQueue, UniCoherence>, Bus>;
#if defined(USE_LZSS_COMPRESSION)
template class SlaveInterface<Cache<CacheTags<FALRU,LZSSCompression>, BlockingBuffer, SimpleCoherence>, Bus>;
template class SlaveInterface<Cache<CacheTags<FALRU,LZSSCompression>, BlockingBuffer, UniCoherence>, Bus>;
template class SlaveInterface<Cache<CacheTags<FALRU,LZSSCompression>, MissQueue, SimpleCoherence>, Bus>;
template class SlaveInterface<Cache<CacheTags<FALRU,LZSSCompression>, MissQueue, UniCoherence>, Bus>;
#endif
#endif

#if defined(USE_CACHE_IIC)
template class SlaveInterface<Cache<CacheTags<IIC,NullCompression>, BlockingBuffer, SimpleCoherence>, Bus>;
template class SlaveInterface<Cache<CacheTags<IIC,NullCompression>, BlockingBuffer, UniCoherence>, Bus>;
template class SlaveInterface<Cache<CacheTags<IIC,NullCompression>, MissQueue, SimpleCoherence>, Bus>;
template class SlaveInterface<Cache<CacheTags<IIC,NullCompression>, MissQueue, UniCoherence>, Bus>;
#if defined(USE_LZSS_COMPRESSION)
template class SlaveInterface<Cache<CacheTags<IIC,LZSSCompression>, BlockingBuffer, SimpleCoherence>, Bus>;
template class SlaveInterface<Cache<CacheTags<IIC,LZSSCompression>, BlockingBuffer, UniCoherence>, Bus>;
template class SlaveInterface<Cache<CacheTags<IIC,LZSSCompression>, MissQueue, SimpleCoherence>, Bus>;
template class SlaveInterface<Cache<CacheTags<IIC,LZSSCompression>, MissQueue, UniCoherence>, Bus>;
#endif
#endif

#if defined(USE_CACHE_LRU)
template class SlaveInterface<Cache<CacheTags<LRU,NullCompression>, BlockingBuffer, SimpleCoherence>, Bus>;
template class SlaveInterface<Cache<CacheTags<LRU,NullCompression>, BlockingBuffer, UniCoherence>, Bus>;
template class SlaveInterface<Cache<CacheTags<LRU,NullCompression>, MissQueue, SimpleCoherence>, Bus>;
template class SlaveInterface<Cache<CacheTags<LRU,NullCompression>, MissQueue, UniCoherence>, Bus>;
#if defined(USE_LZSS_COMPRESSION)
template class SlaveInterface<Cache<CacheTags<LRU,LZSSCompression>, BlockingBuffer, SimpleCoherence>, Bus>;
template class SlaveInterface<Cache<CacheTags<LRU,LZSSCompression>, BlockingBuffer, UniCoherence>, Bus>;
template class SlaveInterface<Cache<CacheTags<LRU,LZSSCompression>, MissQueue, SimpleCoherence>, Bus>;
template class SlaveInterface<Cache<CacheTags<LRU,LZSSCompression>, MissQueue, UniCoherence>, Bus>;
#endif
#endif

#if defined(USE_CACHE_SPLIT)
template class SlaveInterface<Cache<CacheTags<Split,NullCompression>, BlockingBuffer, SimpleCoherence>, Bus>;
template class SlaveInterface<Cache<CacheTags<Split,NullCompression>, BlockingBuffer, UniCoherence>, Bus>;
template class SlaveInterface<Cache<CacheTags<Split,NullCompression>, MissQueue, SimpleCoherence>, Bus>;
template class SlaveInterface<Cache<CacheTags<Split,NullCompression>, MissQueue, UniCoherence>, Bus>;
#if defined(USE_LZSS_COMPRESSION)
template class SlaveInterface<Cache<CacheTags<Split,LZSSCompression>, BlockingBuffer, SimpleCoherence>, Bus>;
template class SlaveInterface<Cache<CacheTags<Split,LZSSCompression>, BlockingBuffer, UniCoherence>, Bus>;
template class SlaveInterface<Cache<CacheTags<Split,LZSSCompression>, MissQueue, SimpleCoherence>, Bus>;
template class SlaveInterface<Cache<CacheTags<Split,LZSSCompression>, MissQueue, UniCoherence>, Bus>;
#endif
#endif

#if defined(USE_CACHE_SPLIT_LIFO)
template class SlaveInterface<Cache<CacheTags<SplitLIFO,NullCompression>, BlockingBuffer, SimpleCoherence>, Bus>;
template class SlaveInterface<Cache<CacheTags<SplitLIFO,NullCompression>, BlockingBuffer, UniCoherence>, Bus>;
template class SlaveInterface<Cache<CacheTags<SplitLIFO,NullCompression>, MissQueue, SimpleCoherence>, Bus>;
template class SlaveInterface<Cache<CacheTags<SplitLIFO,NullCompression>, MissQueue, UniCoherence>, Bus>;
#if defined(USE_LZSS_COMPRESSION)
template class SlaveInterface<Cache<CacheTags<SplitLIFO,LZSSCompression>, BlockingBuffer, SimpleCoherence>, Bus>;
template class SlaveInterface<Cache<CacheTags<SplitLIFO,LZSSCompression>, BlockingBuffer, UniCoherence>, Bus>;
template class SlaveInterface<Cache<CacheTags<SplitLIFO,LZSSCompression>, MissQueue, SimpleCoherence>, Bus>;
template class SlaveInterface<Cache<CacheTags<SplitLIFO,LZSSCompression>, MissQueue, UniCoherence>, Bus>;
#endif
#endif


#endif // DOXYGEN_SHOULD_SKIP_THIS

