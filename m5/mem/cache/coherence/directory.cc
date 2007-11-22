
#include "directory.hh"
#include "sim/builder.hh"
        
#include <fstream>
        
using namespace std;
// using namespace __gnu_cxx;
        

template<class TagStore>
DirectoryProtocol<TagStore>::DirectoryProtocol(const std::string &_cacheName,
                                               const std::string &_protocol,
                                               bool _doTrace,
                                               int _dumpInterval,
                                               int _traceStart){
            
    protocol = _protocol;
    cacheName = _cacheName;
    
    doTrace = _doTrace;
    traceStart = _traceStart;
    
    if(_dumpInterval != 0){
        dumpEvent = 
                new DirectoryProtocolDumpEvent<TagStore>(this, _dumpInterval);
        dumpEvent->schedule(curTick + _dumpInterval);
        
        dumpFileName = "coherencedump." + _cacheName + ".txt";
        
        ofstream dumpfile(dumpFileName.c_str());
        dumpfile << "Clock Cycle;Redirected Reads; Transfer Owner Request; "
                 << "Owner Writebacks; Sharer Writebacks; NACKs\n";
        dumpfile.flush();
        dumpfile.close();
    }
    else{
        dumpEvent = NULL;
    }
    lastNumRedirectedReads = 0;
    lastNumOwnerRequests = 0;
    lastNumOwnerWritebacks = 0;
    lastNumSharerWritebacks = 0;
    lastNumNACKs = 0;
    
    if(doTrace){
        ofstream tracefile(OUTFILENAME);
        tracefile << "M5 coherence trace:\n\n";
        tracefile.flush();
        tracefile.close();
    }
}

template<class TagStore>
void
DirectoryProtocol<TagStore>::regStats(){
    
    using namespace Stats;
    
    numRedirectedReads
            .name(name() + ".num_redirected_reads")
            .desc("total number of reads redirected to a different cache")
            ;
    
    numOwnerRequests
            .name(name() + ".num_owner_requests")
            .desc("total number of owner requests issued to allready owned"
                  " blocks")
            ;
    
    numOwnerWritebacks
            .name(name() + ".num_owner_writebacks")
            .desc("total number of times this cache has written back an owned"
                  " block")
            ;
    
    numSharerWritebacks
            .name(name() + ".num_sharer_writebacks")
            .desc("total number of times this cache has written back a block"
                  " owned by a different cache")
            ;
    
    numNACKs
            .name(name() + ".num_nacks")
            .desc("total number of negative acknowledgements sent from this"
                  " cache")
            ;
}

template<class TagStore>
void
DirectoryProtocol<TagStore>::writeTraceLine(const std::string cachename,
                                            const std::string message,
                                            const int owner,
                                            const DirectoryState state,
                                            const Addr paddr,
                                            const Addr blkSize,
                                            bool* presentFlags){
    if(doTrace && curTick >= traceStart){
        
        Addr blkAddr = (paddr & ~((Addr)blkSize - 1));
        
        ofstream tracefile(OUTFILENAME, ofstream::app);
        tracefile << curTick 
                  << "; " << cachename 
                  << "; " << blkAddr << ", " << hex 
                  << showbase << blkAddr << dec
                  << "; Owner " << owner 
                  << "; " << state
                  << "; " << message;
        
        if(presentFlags != NULL){
            tracefile << "; [";
            for(int i=0;i<directoryCpuCount;i++){
                tracefile << (presentFlags[i] ? "1" : "0");
                if(i != (directoryCpuCount-1)) tracefile << ",";
            }
            tracefile << "]";
        }
        
        tracefile << "\n";
        tracefile.flush();
        tracefile.close();
    }
}

template<class TagStore>
void
DirectoryProtocol<TagStore>::dumpStats(){
    
    ofstream dumpfile(dumpFileName.c_str(), ofstream::app);
    
    dumpfile << curTick << "; "
             << (numRedirectedReads.value() - lastNumRedirectedReads) << "; "
             << (numOwnerRequests.value() - lastNumOwnerRequests) << "; "
             << (numOwnerWritebacks.value() - lastNumOwnerWritebacks) << "; "
             << (numSharerWritebacks.value() - lastNumSharerWritebacks) << "; "
             << (numNACKs.value() - lastNumNACKs) << "\n";
    
    lastNumRedirectedReads = numRedirectedReads.value();
    lastNumOwnerRequests = numOwnerRequests.value();
    lastNumOwnerWritebacks = numOwnerWritebacks.value();
    lastNumSharerWritebacks = numSharerWritebacks.value();
    lastNumNACKs = numNACKs.value();
    
    dumpfile.flush();
    dumpfile.close();
}

template<class TagStore>
bool
DirectoryProtocol<TagStore>::isOwned(Addr address){
    if(blockStore.find(address) == blockStore.end()){
        return false;
    }
    return true;
}

template<class TagStore>
int
DirectoryProtocol<TagStore>::getOwner(Addr address){
    map<Addr, int>::iterator found = blockStore.find(address);
    if(found != blockStore.end()){
        return found->second;
    }
    return -1;
}

template<class TagStore>
void
DirectoryProtocol<TagStore>::setOwner(Addr address, int newOwner){
    blockStore[address] = newOwner;
}

template<class TagStore>
void
DirectoryProtocol<TagStore>::removeOwner(Addr address){
    map<Addr, int>::iterator eraseIt = blockStore.find(address);
    assert(eraseIt != blockStore.end());
    blockStore.erase(eraseIt);
}

#ifndef DOXYGEN_SHOULD_SKIP_THIS

// Include config files
// Must be included first to determine which caches we want
#include "mem/config/cache.hh"
#include "mem/config/compression.hh"


// Tag Templates
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
                 
// Compression Templates
#include "base/compression/null_compression.hh"
#if defined(USE_LZSS_COMPRESSION)
#include "base/compression/lzss_compression.hh"
#endif
                 
#if defined(USE_CACHE_FALRU)
    template class DirectoryProtocol<CacheTags<FALRU,NullCompression> >;
#if defined(USE_LZSS_COMPRESSION)
    template class DirectoryProtocol<CacheTags<FALRU,LZSSCompression> >;
#endif
#endif

#if defined(USE_CACHE_IIC)
    template class DirectoryProtocol<CacheTags<IIC,NullCompression> >;
#if defined(USE_LZSS_COMPRESSION)
    template class DirectoryProtocol<CacheTags<IIC,LZSSCompression> >;
#endif
#endif

#if defined(USE_CACHE_LRU)
    template class DirectoryProtocol<CacheTags<LRU,NullCompression> >;
#if defined(USE_LZSS_COMPRESSION)
    template class DirectoryProtocol<CacheTags<LRU,LZSSCompression> >;
#endif
#endif

#if defined(USE_CACHE_SPLIT)
    template class DirectoryProtocol<CacheTags<Split,NullCompression> >;
#if defined(USE_LZSS_COMPRESSION)
    template class DirectoryProtocol<CacheTags<Split,LZSSCompression> >;
#endif
#endif

#if defined(USE_CACHE_SPLIT_LIFO)
    template class DirectoryProtocol<CacheTags<SplitLIFO,NullCompression> >;
#if defined(USE_LZSS_COMPRESSION)
    template class DirectoryProtocol<CacheTags<SplitLIFO,LZSSCompression> >;
#endif
#endif
        
#endif // DOXYGEN_SHOULD_SKIP_THIS


