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

/** @file
 * Template instantiations of Cache TagStore template policy.
 */

//Template includes
//Tags
#include "mem/config/cache.hh"
#include "mem/config/compression.hh"

#include "mem/cache/tags/lru.hh"
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


//Compression
#include "base/compression/null_compression.hh"
#if defined(USE_LZSS_COMPRESSION)
#include "base/compression/lzss_compression.hh"
#endif

#include "mem/cache/tags/cache_tags_impl.hh"

#ifndef DOXYGEN_SHOULD_SKIP_THIS

#if defined(USE_CACHE_FALRU)
template class CacheTags<FALRU, NullCompression>;
#if defined(USE_LZSS_COMPRESSION)
template class CacheTags<FALRU, LZSSCompression>;
#endif
#endif

#if defined(USE_CACHE_IIC)
template class CacheTags<IIC, NullCompression>;
#if defined(USE_LZSS_COMPRESSION)
template class CacheTags<IIC, LZSSCompression>;
#endif
#endif

#if defined(USE_CACHE_LRU)
template class CacheTags<LRU, NullCompression>;
#if defined(USE_LZSS_COMPRESSION)
template class CacheTags<LRU, LZSSCompression>;
#endif
#endif

#if defined(USE_CACHE_SPLIT)
template class CacheTags<Split, NullCompression>;
#if defined(USE_LZSS_COMPRESSION)
template class CacheTags<Split, LZSSCompression>;
#endif
#endif

#if defined(USE_CACHE_SPLIT_LIFO)
template class CacheTags<SplitLIFO, NullCompression>;
#if defined(USE_LZSS_COMPRESSION)
template class CacheTags<SplitLIFO, LZSSCompression>;
#endif
#endif

#endif // DOXYGEN_SHOULD_SKIP_THIS
