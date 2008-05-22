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

/**
 * @file
 * Declaration of a common base class for cache tagstore objects.
 */

#ifndef __BASE_TAGS_HH__
#define __BASE_TAGS_HH__

#include <string>
#include "base/statistics.hh"
#include "base/callback.hh"
#include "sim/eventq.hh"

class BaseCache;

/**
 * A common base class of Cache tagstore objects.
 */
class BaseTags
{
  protected:
    /** Pointer to the parent cache. */
    BaseCache *cache;

    /** Local copy of the parent cache name. Used for DPRINTF. */
    std::string objName;

    /** 
     * The number of tags that need to be touched to meet the warmup 
     * percentage.
     */
    int warmupBound;
    /** Marked true when the cache is warmed up. */
    bool warmedUp;
 
    // Statistics
    /**
     * @addtogroup CacheStatistics
     * @{
     */

    /** Number of replacements of valid blocks per thread. */
    Stats::Vector<> replacements;
    /** Per cycle average of the number of tags that hold valid data. */
    Stats::Average<> tagsInUse;

    /** The total number of references to a block before it is replaced. */
    Stats::Scalar<> totalRefs;
    
    /**
     * The number of reference counts sampled. This is different from 
     * replacements because we sample all the valid blocks when the simulator
     * exits.
     */
    Stats::Scalar<> sampledRefs;
    
    /**
     * Average number of references to a block before is was replaced.
     * @todo This should change to an average stat once we have them.
     */
    Stats::Formula avgRefs;
    
    /** The cycle that the warmup percentage was hit. */
    Stats::Scalar<> warmupCycle;
    /**
     * @}
     */

  public:

    /**
     * Destructor.
     */
    virtual ~BaseTags() {}

    /**
     * Set the parent cache back pointer. Also copies the cache name to
     * objName.
     * @param _cache Pointer to parent cache.
     */
    void setCache(BaseCache *_cache);

    /**
     * Return the parent cache name.
     * @return the parent cache name.
     */
    const std::string &name() const
    {
	return objName;
    }

    /**
     * Register local statistics.
     * @param name The name to preceed each statistic name.
     */
    void regStats(const std::string &name);

    /**
     * Average in the reference count for valid blocks when the simulation 
     * exits.
     */
    virtual void cleanupRefs() {}
    
    virtual std::vector<int> perCoreOccupancy() {
        std::vector<int> tmp;
        return tmp; 
    }
    
    virtual int getNumSets(){
        return 0;
    }
    
    virtual int getAssoc(){
        return 0;
    }
        
    
    virtual void handleSwitchEvent(){
    }
};

class BaseTagsCallback : public Callback
{
    BaseTags *tags;
  public:
    BaseTagsCallback(BaseTags *t) : tags(t) {}
    virtual void process() { tags->cleanupRefs(); };
};

class BaseTagsSwitchEvent : public Event
{

    public:
        
        BaseTags* bt;
        
        BaseTagsSwitchEvent(BaseTags* _bt)
            : Event(&mainEventQueue, CPU_Switch_Pri), bt(_bt)
        {
        }
        
        void process(){
            bt->handleSwitchEvent();
            delete this;
        }

        virtual const char *description(){
            return "BaseTags Switch Event";
        }
};
#endif //__BASE_TAGS_HH__
