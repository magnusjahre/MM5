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

// #define DO_HIT_TRACE 1

#ifdef DO_HIT_TRACE
#include <fstream>
#endif

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
    
    /* Constants */
    write_recovery_time = 6;
    internal_write_to_read = 3;

    /* Build bank state */
    Bankstate.resize(num_banks, DDR2Idle);
    openpage.resize(num_banks, 0);
    activateTime.resize(num_banks, 0);
    closeTime.resize(num_banks, 0);
    readyTime.resize(num_banks, 0);
    lastCmdFinish.resize(num_banks, 0);

    bankInConflict.resize(num_banks, false);

    pagesize = 10; // 1kB = 2**10 from standard :-)
    internal_read_to_precharge = 3;
    data_time = 4; // Single channel = 4 (i.e. burst lenght 8), dual channel = 2
    read_to_write_turnaround = 6; // for burstlength = 8 
    internal_row_to_row = 3;

    // internal read to precharge can be hidden by the data transfer if it is less than 2 cc
    assert(internal_read_to_precharge >= 2); // assumed by the implementation
    
    //Clock frequency is 4GHz, bus freq 400MHz
    bus_to_cpu_factor = 10; // Multiply by this to get cpu - cycles :p

    // Convert to CPU cycles :-)
    RAS_latency *= bus_to_cpu_factor;
    CAS_latency *= bus_to_cpu_factor;
    precharge_latency *= bus_to_cpu_factor;
    min_activate_to_precharge_latency *= bus_to_cpu_factor;
    write_recovery_time *= bus_to_cpu_factor;
    internal_write_to_read *= bus_to_cpu_factor;
    internal_read_to_precharge *= bus_to_cpu_factor;
    read_to_write_turnaround *= bus_to_cpu_factor;
    data_time *= bus_to_cpu_factor;
    internal_row_to_row *= bus_to_cpu_factor;
    
#ifdef DO_HIT_TRACE
    ofstream ofile("dram_access_trace.txt");
    ofile << "";
    ofile.flush();
    ofile.close();
#endif
}

/* Calculate latency for request */
template <class Compression>
Tick
SimpleMemBank<Compression>::calculateLatency(MemReqPtr &req)
{
    // Sanity checks
    assert (req->cmd == Read || req->cmd == Writeback || req->cmd == Close || req->cmd == Activate);
    
    int bank = getMemoryBankID(req->paddr);
    Addr page = (req->paddr >> pagesize);
    DDR2State oldState = Bankstate[bank];
    
    accessesPerBank[bank]++;
    
    DPRINTF(DRAM, "Calculating latency for req %x, cmd %s, page %x, bank %d\n",
            req->paddr,
            req->cmd,
            page,
            bank);
    
    if (req->cmd == Close) {
        active_bank_count--;
        Tick closelatency = 0;
        assert (Bankstate[bank] != DDR2Idle);
        
        Tick prechCmdTick = 0;
        if(Bankstate[bank] == DDR2Active){
            // can happen on controller switch, but should be avoided
            warn("Transition directly from Active to Idle (bad memory scheduling), memory read assumed");
            // Assume that request is a read as this gives the lowest delay
            prechCmdTick = activateTime[bank] + CAS_latency + (internal_read_to_precharge - 2*bus_to_cpu_factor);
        }
        if (Bankstate[bank] == DDR2Read) {
            prechCmdTick = readyTime[bank] + (internal_read_to_precharge - 2*bus_to_cpu_factor);
        } 
        if (Bankstate[bank] == DDR2Written) {
            prechCmdTick = readyTime[bank] + data_time + write_recovery_time;
        }
        assert(prechCmdTick != 0);
        
        if (prechCmdTick - activateTime[bank] < min_activate_to_precharge_latency) {
           closelatency = min_activate_to_precharge_latency - (prechCmdTick - activateTime[bank]);
        }
        closelatency += precharge_latency;
        closeTime[bank] = closelatency + prechCmdTick;
        Bankstate[bank] = DDR2Idle;
        
        return 0;
    }

    if (req->cmd == Activate) {
        
        if(closeTime[bank] >= curTick && closeTime[bank] != 0){
            bankInConflict[bank] = true;
        }
        
        active_bank_count++;
        // Max 4 banks can be active at any time according to DDR2 spec.
        assert(active_bank_count < 5);
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
                int readCmdToWriteStartLat = read_to_write_turnaround + CAS_latency - 1*bus_to_cpu_factor;
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
                int writeLatency = CAS_latency - 1*bus_to_cpu_factor;
                readyTime[bank] = activateTime[bank] + writeLatency;
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

#ifdef DO_HIT_TRACE
    bool isConfict = false;
#endif
    
    assert(req->adaptiveMHASenderID != -1);
    perCPURequests[req->adaptiveMHASenderID]++;
    
    assert(req->cmd == Writeback || req->cmd == Read);
    if (curTick < readyTime[bank]) {
        // Wait until activation completes;
        latency += readyTime[bank] - curTick;
        number_of_non_overlap_activate++;
        
        assert(!isHit);
        if(bankInConflict[bank]){
            
#ifdef DO_HIT_TRACE
            isConfict = true;
#endif
            
            perCPUPageConflicts[req->adaptiveMHASenderID]++;
            
            pageConflicts[(req->cmd == Read ? DRAM_READ  : DRAM_WRITE)]++;
            pageConflictLatency[(req->cmd == Read ? DRAM_READ  : DRAM_WRITE)] += latency;
            pageConflictLatencyDistribution[(req->cmd == Read ? DRAM_READ  : DRAM_WRITE)].sample(latency);
        }
        else{
            
            perCPUPageMisses[req->adaptiveMHASenderID]++;
            
            pageMisses[(req->cmd == Read ? DRAM_READ  : DRAM_WRITE)]++;
            pageMissLatency[(req->cmd == Read ? DRAM_READ  : DRAM_WRITE)] += latency;
            pageMissLatencyDistribution[(req->cmd == Read ? DRAM_READ  : DRAM_WRITE)].sample(latency);
        }
    }
    else{
        if(bankInConflict[bank]){
            assert(!isHit);
            
#ifdef DO_HIT_TRACE
            isConfict = true;
#endif
            
            perCPUPageConflicts[req->adaptiveMHASenderID]++;
            
            pageConflicts[(req->cmd == Read ? DRAM_READ  : DRAM_WRITE)]++;
            pageConflictLatency[(req->cmd == Read ? DRAM_READ  : DRAM_WRITE)] += latency;
            pageConflictLatencyDistribution[(req->cmd == Read ? DRAM_READ  : DRAM_WRITE)].sample(latency);
        }
        else if(isHit){
            
            perCPUPageHits[req->adaptiveMHASenderID]++;
            
            pageHits[(req->cmd == Read ? DRAM_READ  : DRAM_WRITE)]++;
            pageHitLatency[(req->cmd == Read ? DRAM_READ  : DRAM_WRITE)] += latency;
        }
        else{
            
            perCPUPageMisses[req->adaptiveMHASenderID]++;
            
            pageMisses[(req->cmd == Read ? DRAM_READ  : DRAM_WRITE)]++;
            pageMissLatency[(req->cmd == Read ? DRAM_READ  : DRAM_WRITE)] += latency;
            pageMissLatencyDistribution[(req->cmd == Read ? DRAM_READ  : DRAM_WRITE)].sample(latency);
        }
    }
    bankInConflict[bank] = false;
    
#ifdef DO_HIT_TRACE
    ofstream ofile("dram_access_trace.txt", ofstream::app);
    ofile << curTick << ";" << req->paddr << ";" << bank << ";";
    if(isConfict) ofile << "conflict";
    else if(isHit) ofile << "hit";
    else ofile << "miss";
    ofile << ";" << req->inserted_into_memory_controller << ";" << req->oldAddr << "\n";
    ofile.flush();
    ofile.close();
#endif
    
    DDR2State curState = Bankstate[bank];
    if((oldState == DDR2Read && curState == DDR2Read)
        || (oldState == DDR2Written && curState == DDR2Written)){
        readyTime[bank] += data_time;
    }
    else if((oldState == DDR2Read && curState == DDR2Written)
            || (oldState == DDR2Written && curState == DDR2Read)){
        readyTime[bank] = curTick + (latency - data_time);
    }


    total_latency += latency;
    lastCmdFinish[bank] = latency + curTick;
    
    return latency;
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
    if (readyTime[bank] < curTick) {
      return true;
    }
  }
  return false;
}
