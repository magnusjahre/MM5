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
 * Definition of a simple main memory.
 */

#include <sstream>
#include <string>
#include <vector>

#include "config/full_system.hh"
#include "cpu/base.hh"
#include "cpu/exec_context.hh" // for reading from memory
#include "cpu/smt.hh"
#include "mem/bus/bus.hh"
#include "mem/bus/bus_interface.hh"
#include "mem/bus/slave_interface.hh"
#include "mem/functional/functional.hh"
#include "mem/memory_interface.hh"
#include "mem/mem_req.hh"
#include "mem/timing/simple_mem_bank.hh"
#include "sim/builder.hh"

//#define DO_HIT_TRACE
#define STATIC_LATENCY 120

using namespace std;

template <class Compression>
SimpleMemBank<Compression>::SimpleMemBank(const string &name, HierParams *hier,
					  BaseMemory::Params params)
    : BaseMemory(name, hier, params)
{

    active_bank_count = 0;
    num_banks = params.num_banks;
    RAS_latency = params.RAS_latency;
    CAS_latency = params.CAS_latency;
    precharge_latency = params.precharge_latency;
    min_activate_to_precharge_latency = params.min_activate_to_precharge_latency;
    write_latency = params.write_latency;

    write_recovery_time = params.write_recovery_time;
    internal_write_to_read = params.internal_write_to_read;
    pagesize = params.pagesize;
    internal_read_to_precharge = params.internal_read_to_precharge;
    data_time = params.data_time;
    read_to_write_turnaround = params.read_to_write_turnaround;
    internal_row_to_row = params.internal_row_to_row;

    maximum_active_banks = params.max_active_bank_cnt;

    returnStaticLatencies = params.static_memory_latency;

    /* Build bank state */
    Bankstate.resize(num_banks, DDR2Idle);
    openpage.resize(num_banks, 0);
    activateTime.resize(num_banks, 0);
    closeTime.resize(num_banks, 0);
    readyTime.resize(num_banks, 0);
    lastCmdFinish.resize(num_banks, 0);

    bankInConflict.resize(num_banks, false);

    //CPU frequency is 4GHz
    bus_to_cpu_factor = 4000 / params.bus_frequency; // Multiply by this to get cpu - cycles :p

    // Convert to CPU cycles :-)
    RAS_latency *= bus_to_cpu_factor;
    CAS_latency *= bus_to_cpu_factor;
    precharge_latency *= bus_to_cpu_factor;
    min_activate_to_precharge_latency *= bus_to_cpu_factor;
    write_latency *= bus_to_cpu_factor;
    write_recovery_time *= bus_to_cpu_factor;
    internal_write_to_read *= bus_to_cpu_factor;
    internal_read_to_precharge *= bus_to_cpu_factor;
    read_to_write_turnaround *= bus_to_cpu_factor;
    data_time *= bus_to_cpu_factor;
    internal_row_to_row *= bus_to_cpu_factor;

#ifdef DO_HIT_TRACE

    pageTrace = RequestTrace(string(""), "dram_access_trace");

    vector<string> traceparams;
    traceparams.push_back("Address");
    traceparams.push_back("Bank");
    traceparams.push_back("Result");
    traceparams.push_back("Inserted At");
    traceparams.push_back("Old Address");
    traceparams.push_back("Sequence Number");
    traceparams.push_back("Command");

    pageTrace.initalizeTrace(traceparams);

#endif
}

/* Calculate latency for request */
template <class Compression>
Tick
SimpleMemBank<Compression>::calculateLatency(MemReqPtr &req)
{
    // Sanity checks
    assert (req->cmd == Read || req->cmd == Writeback || req->cmd == Close || req->cmd == Activate);

    if(returnStaticLatencies){
    	if(req->cmd == Read || req->cmd == Writeback){
    		return STATIC_LATENCY;
    	}

    	assert(req->cmd == Close || req->cmd == Activate);
    	return 0;
    }

    int bank = getMemoryBankID(req->paddr);
    Addr page = (req->paddr >> pagesize);
    DDR2State oldState = Bankstate[bank];

    accessesPerBank[bank]++;

    DPRINTF(DRAM, "Calculating latency for req %d, cmd %s, page %d, bank %d\n",
            req->paddr,
            req->cmd,
            page,
            bank);

    if (req->cmd == Close) {
        active_bank_count--;
        Tick closelatency = 0;
        assert (Bankstate[bank] != DDR2Idle);

        Tick prechCmdTick = 0;
        if (Bankstate[bank] == DDR2Read) {
            if(readyTime[bank] > curTick){
                prechCmdTick = readyTime[bank] + internal_read_to_precharge;
            }
            else{
                prechCmdTick = curTick + internal_read_to_precharge;
            }
            DPRINTF(DRAM, "Bank %d is read, ready at %d, precharge command can issue at %d\n", bank, readyTime[bank], prechCmdTick);
        }
        if (Bankstate[bank] == DDR2Written) {
            if(readyTime[bank] > curTick){
                prechCmdTick = readyTime[bank] + data_time + write_recovery_time;
            }
            else{
                prechCmdTick = curTick + data_time + write_recovery_time;
            }
            DPRINTF(DRAM, "Bank %d is written, ready at %d, precharge command can issue at %d\n", bank, readyTime[bank], prechCmdTick);
        }
        if (Bankstate[bank] == DDR2Active) {
        	if(activateTime[bank] > curTick){
        		prechCmdTick = activateTime[bank];
        	}
        	else{
        		prechCmdTick = curTick;
        	}
        	DPRINTF(DRAM, "Bank %d is active, activated at %d, precharge command can issue at %d\n", bank, activateTime[bank], prechCmdTick);
        }
        assert(prechCmdTick != 0);

        Tick actToPrechLat = prechCmdTick - activateTime[bank];
        if (actToPrechLat < min_activate_to_precharge_latency) {
           closelatency =  min_activate_to_precharge_latency - actToPrechLat;
           DPRINTF(DRAM, "Bank %d: Computed close latency %d with min activate to precharge %d and activate time %d\n", bank, closelatency, min_activate_to_precharge_latency, activateTime[bank]);
        }

        closelatency += precharge_latency;
        closeTime[bank] = closelatency + prechCmdTick;

        DPRINTF(DRAM, "Closing bank %d, will reach idle state at %d\n", bank, closeTime[bank]);

        Bankstate[bank] = DDR2Idle;
        return 0;
    }

    if (req->cmd == Activate) {

        if(closeTime[bank] >= curTick && closeTime[bank] != 0){
            bankInConflict[bank] = true;
        }

        active_bank_count++;
        assert(active_bank_count <= maximum_active_banks);
        Tick extra_latency = 0;
        if (curTick < closeTime[bank]) {
            extra_latency = closeTime[bank] - curTick;

        }
        assert(Bankstate[bank] == DDR2Idle);
        Tick last_activate = 0;
        for (int i=0; i < num_banks; i++) {
            if (last_activate < activateTime[bank]) {
                last_activate = activateTime[bank];
            }
        }

        if (last_activate + internal_row_to_row > curTick && last_activate > 0) {
            activateTime[bank] = (curTick - last_activate) + RAS_latency + curTick;
        } else {
            activateTime[bank] = RAS_latency + curTick;
        }
        activateTime[bank] += extra_latency;
        Bankstate[bank] = DDR2Active;
        openpage[bank] = page;

        DPRINTF(DRAM, "Activating bank %d, closed at %d, last activate at %d, will reach the active state at %d\n",
        		bank, closeTime[bank], last_activate, activateTime[bank]);

        return 0;
    }

    Tick latency = 0;
    bool isHit = false;
    if (req->cmd == Read) {
        number_of_reads++;
        assert (page == openpage[bank]);
        switch(Bankstate[bank]) {

            case DDR2Read:
                latency = data_time;
                number_of_reads_hit++;
                isHit = true;
                break;
            case DDR2Active :
                Bankstate[bank] = DDR2Read;
                latency = data_time;
                readyTime[bank] = activateTime[bank] + CAS_latency;
                DPRINTF(DRAM, "Reading bank %d, reaching ready state at %d\n", bank, readyTime[bank]);
                break;
            case DDR2Written:
                Bankstate[bank] = DDR2Read;
                if (curTick - lastCmdFinish[bank]  <= internal_write_to_read + CAS_latency) {
                    latency = data_time + (internal_write_to_read + CAS_latency - (curTick - lastCmdFinish[bank]));
                } else {
                    latency = data_time;
                }

                number_of_reads_hit++;
                number_of_slow_read_hits++;

                break;
            default:
                fatal("Unknown state!");
        }
    }

    if (req->cmd == Writeback) {
        assert (page == openpage[bank]);
        number_of_writes++;
        switch (Bankstate[bank]) {
            case DDR2Read:
            {
                Bankstate[bank] = DDR2Written;
                int readCmdToWriteStartLat = read_to_write_turnaround + write_latency;
                int curOffset = curTick - readyTime[bank];
                if (curOffset <= readCmdToWriteStartLat) {
                    latency = data_time + (readCmdToWriteStartLat - curOffset);
                } else {
                    latency = data_time;
                }

                number_of_writes_hit++;
                number_of_slow_write_hits++;
                break;
            }
            case DDR2Active:
            {
                Bankstate[bank] = DDR2Written;
                readyTime[bank] = activateTime[bank] + write_latency;
                DPRINTF(DRAM, "Writing bank %d, reaching ready state at %d\n", bank, readyTime[bank]);
                latency = data_time;
                break;
            }
            case DDR2Written:
            {
                latency = data_time;
                number_of_writes_hit++;
                isHit = true;
                break;
            }

            default:
                fatal("Unknown state!");
        }
    }

    if(req->adaptiveMHASenderID > -1 && req->adaptiveMHASenderID < bmCPUCount) perCPURequests[req->adaptiveMHASenderID]++;

    assert(req->cmd == Writeback || req->cmd == Read);
    if (curTick < readyTime[bank]) {
    	assert(!isHit);

    	// Wait until activation completes;
        latency += readyTime[bank] - curTick;
        number_of_non_overlap_activate++;
    }

    if(req->adaptiveMHASenderID > -1 && req->adaptiveMHASenderID < bmCPUCount){
#ifdef DO_HIT_TRACE
    bool isConfict =
#endif
    	updateLatencyDistribution(isHit, latency, bank, req);
    }

    bankInConflict[bank] = false;

#ifdef DO_HIT_TRACE

    vector<RequestTraceEntry> vals;
    vals.push_back(RequestTraceEntry(req->paddr));
    vals.push_back(RequestTraceEntry(bank));

    if(isConfict) vals.push_back(RequestTraceEntry("conflict"));
    else if(isHit) vals.push_back(RequestTraceEntry("hit"));
    else vals.push_back(RequestTraceEntry("miss"));

    vals.push_back(RequestTraceEntry(req->inserted_into_memory_controller));
    vals.push_back(RequestTraceEntry(req->oldAddr == MemReq::inval_addr ? 0 : req->oldAddr ));
    vals.push_back(RequestTraceEntry(req->memCtrlSequenceNumber));
    vals.push_back(RequestTraceEntry(req->cmd.toString()));

    pageTrace.addTrace(vals);
#endif

    DDR2State curState = Bankstate[bank];
    if((oldState == DDR2Read && curState == DDR2Read)
        || (oldState == DDR2Written && curState == DDR2Written)){

        if(readyTime[bank] >= curTick){
            readyTime[bank] += data_time;
        }
        else{
            readyTime[bank] = curTick + data_time;
        }
        DPRINTF(DRAM, "Bank %d hit, new ready time is %d\n", bank, readyTime[bank]);
    }
    else if((oldState == DDR2Read && curState == DDR2Written)
            || (oldState == DDR2Written && curState == DDR2Read)){

        readyTime[bank] = curTick + (latency - data_time);
        DPRINTF(DRAM, "Bank %d r2w or w2r, new ready time is %d\n", bank, readyTime[bank]);
    }


    total_latency += latency;
    lastCmdFinish[bank] = latency + curTick;

    DPRINTF(DRAM, "Returning latency %d, setting last command finished to %d\n", latency, lastCmdFinish[bank]);
    return latency;
}

template <class Compression>
bool
SimpleMemBank<Compression>::updateLatencyDistribution(bool isHit, int latency, int bank, MemReqPtr& req){

	bool isConflict = false;

    if(bankInConflict[bank]){
        assert(!isHit);

        isConflict = true;

        req->dramResult = DRAM_RESULT_CONFLICT;

        if(req->adaptiveMHASenderID != -1) perCPUPageConflicts[req->adaptiveMHASenderID]++;

        pageConflicts[(req->cmd == Read ? DRAM_READ  : DRAM_WRITE)]++;
        pageConflictLatency[(req->cmd == Read ? DRAM_READ  : DRAM_WRITE)] += latency;
        pageConflictLatencyDistribution[(req->cmd == Read ? DRAM_READ  : DRAM_WRITE)].sample(latency);
    }
    else if(isHit){

        req->dramResult = DRAM_RESULT_HIT;

        if(req->adaptiveMHASenderID != -1) perCPUPageHits[req->adaptiveMHASenderID]++;

        pageHits[(req->cmd == Read ? DRAM_READ  : DRAM_WRITE)]++;
        pageHitLatency[(req->cmd == Read ? DRAM_READ  : DRAM_WRITE)] += latency;
    }
    else{

        req->dramResult = DRAM_RESULT_MISS;

        if(req->adaptiveMHASenderID != -1) perCPUPageMisses[req->adaptiveMHASenderID]++;

        pageMisses[(req->cmd == Read ? DRAM_READ  : DRAM_WRITE)]++;
        pageMissLatency[(req->cmd == Read ? DRAM_READ  : DRAM_WRITE)] += latency;
        pageMissLatencyDistribution[(req->cmd == Read ? DRAM_READ  : DRAM_WRITE)].sample(latency);
    }

    return isConflict;
}

/* Handle memory bank access latencies */
template <class Compression>
MemAccessResult
SimpleMemBank<Compression>::access(MemReqPtr &req)
{
    Tick response_time;

    response_time = calculateLatency(req) + curTick;

    req->flags |= SATISFIED;
    si->respond(req, response_time);

    return MA_HIT;
}

template <class Compression>
Tick
SimpleMemBank<Compression>::probe(MemReqPtr &req, bool update)
{
    fatal("PROBE CALLED! YOU ARE NOT HOME!\n");
    return 0;
}


// This function returns true if the request can be issued. Eg. The bank
// is active and contains the right page.
//
template <class Compression>
bool
SimpleMemBank<Compression>::isActive(MemReqPtr &req)
{
  int bank = (req->paddr >> pagesize) % num_banks;
  Addr page = (req->paddr >> pagesize);
  if (Bankstate[bank] == DDR2Idle) {
    return false;
  }
  if (page == openpage[bank]) {
    return true;
  }
  return false;
}

// This function checks if a bank is closed
template <class Compression>
bool
SimpleMemBank<Compression>::bankIsClosed(MemReqPtr &req)
{
  int bank = (req->paddr >> pagesize) % num_banks;
  if (Bankstate[bank] == DDR2Idle) {
    return true;
  }
  return false;
}

template <class Compression>
bool
SimpleMemBank<Compression>::isReady(MemReqPtr &req)
{
	int bank = (req->paddr >> pagesize) % num_banks;
	Addr page = (req->paddr >> pagesize);

	if (Bankstate[bank] == DDR2Idle) {
		return false;
	}

	if (page == openpage[bank]) {
		if (readyTime[bank] <= curTick) {
			return true;
		}
	}
	return false;
}
