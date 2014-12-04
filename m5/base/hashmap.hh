/*
 * Copyright (c) 2003, 2004, 2005
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

#ifndef __HASHMAP_HH__
#define __HASHMAP_HH__

#if defined(__GNUC__) && __GNUC__ >= 3
#include <ext/hash_map>
#else
#include <hash_map>
#endif

#include <string>

#include "sim/host.hh"
#include "arch/alpha/isa_traits.hh"

#if defined(__GNUC__) && __GNUC__ >= 3
    #define __hash_namespace __gnu_cxx
#else
    #define __hash_namespace std
#endif

namespace m5 {
    using ::__hash_namespace::hash_multimap;
    using ::__hash_namespace::hash_map;
    using ::__hash_namespace::hash;
}


///////////////////////////////////
// Some default Hashing Functions
//

namespace __hash_namespace {
#if !defined(__LP64__) && !defined(__alpha__)
    template<>
    struct hash<uint64_t> {
	size_t operator()(uint64_t r) const {
	    return r;
	}
    };

    template<>
    struct hash<int64_t> {
	size_t operator()(int64_t r) const {
	    return r;
	};
    };
#endif

    template<>
    struct hash<std::string> {
	size_t operator()(const std::string &s) const {
	    return(__stl_hash_string(s.c_str()));
	}
    };

#ifdef __APPLE__
    template<>
    struct hash<Addr> {
    	size_t operator()(Addr a) const {
    		return hash<Addr>()(a);
    	}
    };
#endif
}


#endif // __HASHMAP_HH__
