
#include <iostream>
#include <vector>

// Template instantiation includes
#include "mem/config/cache.hh"
#include "mem/config/compression.hh"

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

#include "mem/cache/miss/blocking_buffer.hh"
#include "mem/cache/miss/miss_queue.hh"

#include "mem/cache/coherence/simple_coherence.hh"
#include "mem/cache/coherence/uni_coherence.hh"

// Busses

#include "crossbar_slave_impl.hh"

#ifndef DOXYGEN_SHOULD_SKIP_THIS

#if defined(USE_CACHE_FALRU)
template class CrossbarSlave<Cache<CacheTags<FALRU,NullCompression>, BlockingBuffer, SimpleCoherence> >;
template class CrossbarSlave<Cache<CacheTags<FALRU,NullCompression>, BlockingBuffer, UniCoherence> >;
template class CrossbarSlave<Cache<CacheTags<FALRU,NullCompression>, MissQueue, SimpleCoherence> >;
template class CrossbarSlave<Cache<CacheTags<FALRU,NullCompression>, MissQueue, UniCoherence> >;
#if defined(USE_LZSS_COMPRESSION)
template class CrossbarSlave<Cache<CacheTags<FALRU,LZSSCompression>, BlockingBuffer, SimpleCoherence> >;
template class CrossbarSlave<Cache<CacheTags<FALRU,LZSSCompression>, BlockingBuffer, UniCoherence> >;
template class CrossbarSlave<Cache<CacheTags<FALRU,LZSSCompression>, MissQueue, SimpleCoherence> >;
template class CrossbarSlave<Cache<CacheTags<FALRU,LZSSCompression>, MissQueue, UniCoherence> >;
#endif
#endif

#if defined(USE_CACHE_IIC)
template class CrossbarSlave<Cache<CacheTags<IIC,NullCompression>, BlockingBuffer, SimpleCoherence> >;
template class CrossbarSlave<Cache<CacheTags<IIC,NullCompression>, BlockingBuffer, UniCoherence> >;
template class CrossbarSlave<Cache<CacheTags<IIC,NullCompression>, MissQueue, SimpleCoherence> >;
template class CrossbarSlave<Cache<CacheTags<IIC,NullCompression>, MissQueue, UniCoherence> >;
#if defined(USE_LZSS_COMPRESSION)
template class CrossbarSlave<Cache<CacheTags<IIC,LZSSCompression>, BlockingBuffer, SimpleCoherence> >;
template class CrossbarSlave<Cache<CacheTags<IIC,LZSSCompression>, BlockingBuffer, UniCoherence> >;
template class CrossbarSlave<Cache<CacheTags<IIC,LZSSCompression>, MissQueue, SimpleCoherence> >;
template class CrossbarSlave<Cache<CacheTags<IIC,LZSSCompression>, MissQueue, UniCoherence> >;
#endif
#endif

#if defined(USE_CACHE_LRU)
template class CrossbarSlave<Cache<CacheTags<LRU,NullCompression>, BlockingBuffer, SimpleCoherence> >;
template class CrossbarSlave<Cache<CacheTags<LRU,NullCompression>, BlockingBuffer, UniCoherence> >;
template class CrossbarSlave<Cache<CacheTags<LRU,NullCompression>, MissQueue, SimpleCoherence> >;
template class CrossbarSlave<Cache<CacheTags<LRU,NullCompression>, MissQueue, UniCoherence> >;
#if defined(USE_LZSS_COMPRESSION)
template class CrossbarSlave<Cache<CacheTags<LRU,LZSSCompression>, BlockingBuffer, SimpleCoherence> >;
template class CrossbarSlave<Cache<CacheTags<LRU,LZSSCompression>, BlockingBuffer, UniCoherence> >;
template class CrossbarSlave<Cache<CacheTags<LRU,LZSSCompression>, MissQueue, SimpleCoherence> >;
template class CrossbarSlave<Cache<CacheTags<LRU,LZSSCompression>, MissQueue, UniCoherence> >;
#endif
#endif

#if defined(USE_CACHE_SPLIT)
template class CrossbarSlave<Cache<CacheTags<Split,NullCompression>, BlockingBuffer, SimpleCoherence> >;
template class CrossbarSlave<Cache<CacheTags<Split,NullCompression>, BlockingBuffer, UniCoherence> >;
template class CrossbarSlave<Cache<CacheTags<Split,NullCompression>, MissQueue, SimpleCoherence> >;
template class CrossbarSlave<Cache<CacheTags<Split,NullCompression>, MissQueue, UniCoherence> >;
#if defined(USE_LZSS_COMPRESSION)
template class CrossbarSlave<Cache<CacheTags<Split,LZSSCompression>, BlockingBuffer, SimpleCoherence> >;
template class CrossbarSlave<Cache<CacheTags<Split,LZSSCompression>, BlockingBuffer, UniCoherence> >;
template class CrossbarSlave<Cache<CacheTags<Split,LZSSCompression>, MissQueue, SimpleCoherence> >;
template class CrossbarSlave<Cache<CacheTags<Split,LZSSCompression>, MissQueue, UniCoherence> >;
#endif
#endif

#if defined(USE_CACHE_SPLIT_LIFO)
template class CrossbarSlave<Cache<CacheTags<SplitLIFO,NullCompression>, BlockingBuffer, SimpleCoherence> >;
template class CrossbarSlave<Cache<CacheTags<SplitLIFO,NullCompression>, BlockingBuffer, UniCoherence> >;
template class CrossbarSlave<Cache<CacheTags<SplitLIFO,NullCompression>, MissQueue, SimpleCoherence> >;
template class CrossbarSlave<Cache<CacheTags<SplitLIFO,NullCompression>, MissQueue, UniCoherence> >;
#if defined(USE_LZSS_COMPRESSION)
template class CrossbarSlave<Cache<CacheTags<SplitLIFO,LZSSCompression>, BlockingBuffer, SimpleCoherence> >;
template class CrossbarSlave<Cache<CacheTags<SplitLIFO,LZSSCompression>, BlockingBuffer, UniCoherence> >;
template class CrossbarSlave<Cache<CacheTags<SplitLIFO,LZSSCompression>, MissQueue, SimpleCoherence> >;
template class CrossbarSlave<Cache<CacheTags<SplitLIFO,LZSSCompression>, MissQueue, UniCoherence> >;
#endif
#endif


#endif // DOXYGEN_SHOULD_SKIP_THIS
