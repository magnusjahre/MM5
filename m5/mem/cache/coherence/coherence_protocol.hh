/*
 * Copyright (c) 2002, 2003, 2004, 2005
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
 * Declaration of CoherenceProcotol a basic coherence policy.
 */
#ifndef __COHERENCE_PROTOCOL_HH__
#define __COHERENCE_PROTOCOL_HH__

#include <string>

#include "sim/sim_object.hh"
#include "mem/mem_req.hh"
#include "mem/mem_cmd.hh"
#include "mem/cache/cache_blk.hh"
#include "base/statistics.hh"

class BaseCache;
class MSHR;

/**
 * A simple coherence policy for the memory hierarchy. Currently implements
 * MSI, MESI, and MOESI protocols.
 */
class CoherenceProtocol : public SimObject
{
  public:
    /**
     * Contruct and initialize this policy.
     * @param name The name of this policy.
     * @param protocol The string representation of the protocol to use.
     * @param doUpgrades True if bus upgrades should be used.
     */
    CoherenceProtocol(const std::string &name, const std::string &protocol,
		      const bool doUpgrades);

    /**
     * Destructor.
     */
    virtual ~CoherenceProtocol() {};

    /**
     * Register statistics
     */
    virtual void regStats();

    /**
     * Get the proper bus command for the given command and status.
     * @param cmd The request's command.
     * @param status The current state of the cache block.
     * @param mshr The MSHR matching the request.
     * @return The proper bus command, as determined by the protocol.
     */
    MemCmd getBusCmd(MemCmd cmd, CacheBlk::State status,
		     MSHR *mshr = NULL);

    /**
     * Return the proper state given the current state and the bus response.
     * @param req The bus response.
     * @param oldState The current block state.
     * @return The new state.
     */
    CacheBlk::State getNewState(const MemReqPtr &req, 
				CacheBlk::State oldState);

    /**
     * Handle snooped bus requests.
     * @param cache The cache that snooped the request.
     * @param req The snooped bus request.
     * @param blk The cache block corresponding to the request, if any.
     * @param mshr The MSHR corresponding to the request, if any.
     * @param new_state The new coherence state of the block.
     * @return True if the request should be satisfied locally.
     */
    bool handleBusRequest(BaseCache *cache, MemReqPtr &req, CacheBlk *blk,
			  MSHR *mshr, CacheBlk::State &new_state);

  protected:
    /** Snoop function type. */
    typedef bool (*SnoopFuncType)(BaseCache *, MemReqPtr&, CacheBlk *,
				  MSHR *, CacheBlk::State&);

    //
    // Standard snoop transition functions
    //

    /**
     * Do nothing transition.
     */
    static bool nullTransition(BaseCache *, MemReqPtr&, CacheBlk *,
			       MSHR *, CacheBlk::State&);

    /**
     * Invalid transition, basically panic.
     */
    static bool invalidTransition(BaseCache *, MemReqPtr&, CacheBlk *,
				  MSHR *, CacheBlk::State&);

    /**
     * Invalidate block, move to Invalid state.
     */
    static bool invalidateTrans(BaseCache *, MemReqPtr&, CacheBlk *,
				MSHR *, CacheBlk::State&);

    /**
     * Supply data, no state transition.
     */
    static bool supplyTrans(BaseCache *, MemReqPtr&, CacheBlk *,
			    MSHR *, CacheBlk::State&);

    /**
     * Supply data and go to Shared state.
     */
    static bool supplyAndGotoSharedTrans(BaseCache *, MemReqPtr&, CacheBlk *,
					 MSHR *, CacheBlk::State&);

    /**
     * Supply data and go to Owned state.
     */
    static bool supplyAndGotoOwnedTrans(BaseCache *, MemReqPtr&, CacheBlk *,
					MSHR *, CacheBlk::State&);

    /**
     * Invalidate block, supply data, and go to Invalid state.
     */
    static bool supplyAndInvalidateTrans(BaseCache *, MemReqPtr&, CacheBlk *,
					 MSHR *, CacheBlk::State&);

    /**
     * Assert the shared line for a block that is shared/exclusive.
     */
    static bool assertShared(BaseCache *, MemReqPtr&, CacheBlk *,
					 MSHR *, CacheBlk::State&);

    /**
     * Definition of protocol state transitions.
     */
    class StateTransition
    {
	friend class CoherenceProtocol;

	/** The bus command of this transition. */
	MemCmd busCmd;
	/** The state to transition to. */
	int newState;
	/** The snoop function for this transition. */
	SnoopFuncType snoopFunc;

	/**
	 * Constructor, defaults to invalid transition.
	 */
	StateTransition();

	/**
	 * Initialize bus command.
	 * @param cmd The bus command to use.
	 */
	void onRequest(MemCmd cmd)
	{
	    busCmd = cmd;
	}

	/**
	 * Set the transition state.
	 * @param s The new state.
	 */
	void onResponse(CacheBlk::State s)
	{
	    newState = s;
	}

	/**
	 * Initialize the snoop function.
	 * @param f The new snoop function.
	 */
	void onSnoop(SnoopFuncType f)
	{
	    snoopFunc = f;
	}
    };

    friend class CoherenceProtocol::StateTransition;

    /** Mask to select status bits relevant to coherence protocol. */
    const static CacheBlk::State
	stateMask = BlkValid | BlkWritable | BlkDirty;

    /** The Modified (M) state. */
    const static CacheBlk::State
	Modified = BlkValid | BlkWritable | BlkDirty;
    /** The Owned (O) state. */
    const static CacheBlk::State
	Owned = BlkValid | BlkDirty;
    /** The Exclusive (E) state. */
    const static CacheBlk::State
	Exclusive = BlkValid | BlkWritable;
    /** The Shared (S) state. */
    const static CacheBlk::State
	Shared = BlkValid;
    /** The Invalid (I) state. */
    const static CacheBlk::State
	Invalid = 0;

    /**
     * Maximum state encoding value (used to size transition lookup
     * table).  Could be more than number of states, depends on
     * encoding of status bits.
     */
    const static int stateMax = stateMask;

    /**
     * The table of all possible transitions, organized by starting state and
     * request command.
     */
    StateTransition transitionTable[stateMax+1][NUM_MEM_CMDS];

    /**
     * @addtogroup CoherenceStatistics
     * @{
     */
    /**
     * State accesses from parent cache.
     */
    Stats::Scalar<> requestCount[stateMax+1][NUM_MEM_CMDS];
    /**
     * State accesses from snooped requests.
     */
    Stats::Scalar<> snoopCount[stateMax+1][NUM_MEM_CMDS];
    /**
     * @}
     */
};

#endif // __COHERENCE_PROTOCOL_HH__
