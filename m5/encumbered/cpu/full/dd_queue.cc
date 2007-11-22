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

#include "base/statistics.hh"
#include "encumbered/cpu/full/cpu.hh"
#include "encumbered/cpu/full/dd_queue.hh"
#include "sim/stats.hh"

using namespace std;

//============================================================================
//
//  The Decode-Dispatch Queue places instructions one at a time into a
//  packetizing instruction pipeline. If the entire queue is N cycles long,
//  the instruction pipeline is N-1 stages.
//
//  If the queue is BLOCKING, the instructions leave the pipeline for a single
//  skid buffer from which they can be pulled one at a time.
//
//  If the queue is NON-BLOCKING, the instructions leave the pipeline for a
//  small skid buffer (only holds one packet) during tick(). The packet is
//  de-composed into individual fetch records at this time.
//   -> Instructions can be dispatched from this buffer
//   -> If the instructions are there during the next cycle, they are moved
//      into the appropriate per-thread skid buffer.
//      NOTE: instructions in the per-thread skid buffers are ALWAYS OLDER
//            than the instructions in the small skid buffer!
//

//
//  Constructor
//
DecodeDispatchQueue::DecodeDispatchQueue(FullCPU *_cpu,
					 unsigned length, unsigned bw,
					 bool nonblocking)
{
    bandwidth = bw;
    nonBlocking = nonblocking;
    maxInst = bw * length;

    cpu = _cpu;

    //  We need a 2-stage length, at minimum...
    assert(length > 1);

    pipeline = new InstructionPipe(length - 1, bw);

    dispatchBuffer = new InstructionFifo(bw);


    if (nonBlocking) {
	for (int i = 0; i < SMT_MAX_THREADS; ++i) {
	    // each skid buffer must be able to hold all the instructions
	    // in the pipeline
	    skidBuffers[i] = new InstructionFifo(maxInst);
	}
    }

    instsTotal = 0;
    memset(instsThread, 0, sizeof(instsThread));
}

unsigned
DecodeDispatchQueue::addBW(unsigned thread)
{
    int bw;

    if (!nonBlocking) {
	bw = pipeline->addBW();
    } else {
	// the number of slots available for this thread in the ENTIRE queue
	bw = maxInst - pipeline->count(thread) - dispatchBuffer->count(thread)
	    - skidBuffers[thread]->count();

	// ... limited by the pipe bw
	if (bw > pipeline->addBW())
	    bw = pipeline->addBW();
    }

    return bw;
}

void
DecodeDispatchQueue::add(DynInst *inst)
{
    // instructions added to the queue must NOT be squashed!
    assert(addBW(inst->thread_number) > 0);

    pipeline->add(inst);

    ++instsTotal;
    ++instsThread[inst->thread_number];

#if 0
    if (pipeline->count() > 112 || dispatchBuffer->count() > 8
	|| skidBuffers[0]->count() > 120)
    {
	cerr << "OOPS! total is " << instsTotal << ", pipe="
	     << pipeline->count() << ", dbuf=" << dispatchBuffer->count()
	     << ", skid=" << skidBuffers[0]->count() << endl;
    }

    if (pipeline->count() + dispatchBuffer->count() + skidBuffers[0]->count()
	> 120)
    {
	cerr << "OOPS 2! total is " << instsTotal << ", pipe="
	     << pipeline->count() << ", dbuf=" << dispatchBuffer->count()
	     << ", skid=" << skidBuffers[0]->count() << endl;
    }

    if (pipeline->count() + dispatchBuffer->count() + skidBuffers[0]->count()
	!= instsTotal)
    {
	cerr << "OOPS 3! total is " << instsTotal << ", pipe="
	     << pipeline->count() << ", dbuf=" << dispatchBuffer->count()
	     << ", skid=" << skidBuffers[0]->count() << endl;
    }
#endif
}


bool
DecodeDispatchQueue::loadable()
{
    return pipeline->stageOccupancy(0) == 0;
}


DynInst *
DecodeDispatchQueue::peek(unsigned thread)
{
    DynInst *inst = 0;

    if (!nonBlocking) {
	//
	//  No skid-buffers, just grab the next instruction from the end of
	//  the pipeline (which is actually the dispatchBuffer)
	//  ==> Ignore the thread arg!!!
	//
	inst = dispatchBuffer->peek();
    } else {
	//
	//  First, look to the skid buffer associated with this thread. If
	//  that is empty, look to the dispatchBuffer
	//
	if (skidBuffers[thread]->count()) {
	    inst = skidBuffers[thread]->peek();
	} else {
	    inst = dispatchBuffer->peek();

	    // the dispatch buffer should only contain instructions from
	    // one thread...
	    // we shouldn't be peeking unless we know that there are
	    // instructions from the thread available!
	    // (squashed instructions have inst == NULL)
	    assert(inst == 0 || inst->thread_number == thread);
	}
    }

    return inst;
}


void
DecodeDispatchQueue::remove(unsigned thread)
{
    if (!nonBlocking) {
	//
	//  No skid-buffers, remove the next instruction from the end of
	//  the pipeline (which is actually the dispatchBuffer)
	//  ==> Ignore the thread arg!!!
	//
	dispatchBuffer->remove();
    } else {
	//
	//  First, look to the skid buffer associated with this thread. If
	//  that is empty, look to the dispatchBuffer
	//
	if (skidBuffers[thread]->count())
	    skidBuffers[thread]->remove();
	else
	    dispatchBuffer->remove();
    }

    --instsTotal;
    --instsThread[thread];

#if 0
    if (pipeline->count() > 112 || dispatchBuffer->count() > 8
	|| skidBuffers[0]->count() > 120)
    {
	cerr << "OOPS! total is " << instsTotal << ", pipe="
	     << pipeline->count() << ", dbuf=" << dispatchBuffer->count()
	     << ", skid=" << skidBuffers[0]->count() << endl;
    }

    if (pipeline->count() + dispatchBuffer->count() + skidBuffers[0]->count()
	> 120)
    {
	cerr << "OOPS 2! total is " << instsTotal << ", pipe="
	     << pipeline->count() << ", dbuf=" << dispatchBuffer->count()
	     << ", skid=" << skidBuffers[0]->count() << endl;
    }

    if (pipeline->count() + dispatchBuffer->count() + skidBuffers[0]->count()
	!= instsTotal)
    {
	cerr << "OOPS 3! total is " << instsTotal << ", pipe="
	     << pipeline->count() << ", dbuf=" << dispatchBuffer->count()
	     << ", skid=" << skidBuffers[0]->count() << endl;
    }
#endif
}


unsigned
DecodeDispatchQueue::squash()
{
    unsigned cnt = pipeline->squash();

    cnt += dispatchBuffer->squash();

    if (nonBlocking)
	for (int i = 0; i < SMT_MAX_THREADS; ++i)
	    cnt += skidBuffers[i]->squash();

    return cnt;
}


unsigned
DecodeDispatchQueue::squash(unsigned thread)
{
    unsigned cnt = pipeline->squash(thread);

    cnt += dispatchBuffer->squash(thread);

    if (nonBlocking)
	cnt += skidBuffers[thread]->squash(thread);

    return cnt;
}


//
//  This should NOT be called between dispatch() and decode()
//
void
DecodeDispatchQueue::tick_stats()
{
    for (int i = 0; i < cpu->number_of_threads; ++i)
	cum_insts[i] += instsThread[i];

#if 0
    if (pipeline->count() > 112 || dispatchBuffer->count() > 8
	|| skidBuffers[0]->count() > 120)
    {
	cerr << "OOPS! total is " << instsTotal << ", pipe="
	     << pipeline->count() << ", dbuf=" << dispatchBuffer->count()
	     << ", skid=" << skidBuffers[0]->count() << endl;
    }

    if (pipeline->count() + dispatchBuffer->count() + skidBuffers[0]->count()
	> 120)
    {
	cerr << "OOPS 2! total is " << instsTotal << ", pipe="
	     << pipeline->count() << ", dbuf=" << dispatchBuffer->count()
	     << ", skid=" << skidBuffers[0]->count() << endl;
    }

    if (pipeline->count() + dispatchBuffer->count() + skidBuffers[0]->count()
	!= instsTotal)
    {
	cerr << "OOPS 3! total is " << instsTotal << ", pipe="
	     << pipeline->count() << ", dbuf=" << dispatchBuffer->count()
	     << ", skid=" << skidBuffers[0]->count() << endl;
    }
#endif
}


//
//  This should be called between dispatch() and decode()
//
void
DecodeDispatchQueue::tick()
{
    //
    //  First: For a non-blocking queue, move any instructions remaining in the
    //         dispatch buffer to the appropriate skid buffer
    //
    if (nonBlocking && dispatchBuffer->count()) {
	do {
	    unsigned thread;

	    //  Returns the thread number via arg list...
	    DynInst * inst = dispatchBuffer->remove(&thread);

	    if (inst != 0) {
		skidBuffers[thread]->add(inst);
	    } else {
		// we'll drop it here... not really different from dropping it
		// at dispatch or before it would go into skid buffers...
		--instsTotal;
		--instsThread[thread];
	    }

	} while (dispatchBuffer->count());
    }

    //
    //  Second: Move any instructions in the bottom of the pipeline into the
    //          dispatch buffer
    //
    //          This is where we de-packetize the instructions
    //
    if (pipeline->instsAvailable()) {
	InstructionPacket &pkt = pipeline->peek();
	for (int idx = 0; idx < pipeline->instsAvailable(); ++idx) {
	    if (pkt.inst(idx) != 0) {
		dispatchBuffer->add( pkt.inst(idx) );
	    } else {
		// ==> Instruction was squashed
		//   Since we don't put squashed instructions into
		//   the dispatch buffer, we need to decrement the
		//   instruction count here
		--instsThread[pkt.threadNumber()];
		--instsTotal;
	    }
	}

	// clear the end of the pipeline
	pipeline->clearTail();
    }

    //
    //  Third: Advance the pipeline
    //
    pipeline->tick();
}


unsigned
DecodeDispatchQueue::count()
{
    unsigned cnt = pipeline->count() + dispatchBuffer->count();

    if (nonBlocking)
	for (int i = 0; i < SMT_MAX_THREADS; ++i)
	    cnt += skidBuffers[i]->count();

    return cnt;
}


unsigned
DecodeDispatchQueue::count(unsigned thread)
{
    unsigned cnt = pipeline->count(thread) + dispatchBuffer->count(thread);

    if (nonBlocking)
	cnt += skidBuffers[thread]->count();

    return cnt;
}


unsigned
DecodeDispatchQueue::instsAvailable()
{
    unsigned cnt = dispatchBuffer->count();

    if (nonBlocking)
	for (int i = 0; i < SMT_MAX_THREADS; ++i)
	    cnt += skidBuffers[i]->count();

    return cnt;
}


unsigned
DecodeDispatchQueue::instsAvailable(unsigned thread)
{
    unsigned cnt = dispatchBuffer->count(thread);

    if (nonBlocking)
	cnt += skidBuffers[thread]->count();

    return cnt;
}


void
DecodeDispatchQueue::dump()
{
    cout << "===========================================" << endl;
    cout << "Decode-Dispatch Queue (" << count() << ") elements" << endl;
    for (int i = 0; i < SMT_MAX_THREADS; ++i) {
	cout << "  Thread " << i << " = " << count(i) << endl;
    }
    cout << "-------------------------------------------" << endl;

    pipeline->dump();

    cout << "-------------------------------------------" << endl;

    dispatchBuffer->dump();

    cout << "-------------------------------------------" << endl;

    if (nonBlocking) {
	for (int i = 0; i < SMT_MAX_THREADS; ++i) {
	    cout << "-------------------------------------------" << endl;
	    cout << "  Thread " << i << " Skid Buffer:" << endl;
	    skidBuffers[i]->dump();
	}
    }

    cout << "===========================================" << endl << endl;
}


void
DecodeDispatchQueue::dump(unsigned thread)
{
    cout << "===========================================" << endl;
    cout << "Decode-Dispatch Queue (" << count() << ") elements" << endl;
    cout << "  Thread " << thread << " = " << count(thread) << endl;
    cout << "-------------------------------------------" << endl;

    pipeline->dumpThread(thread);

    cout << "-------------------------------------------" << endl;

    dispatchBuffer->dump(thread);

    if (nonBlocking) {
	cout << "-------------------------------------------" << endl;
	cout << "  Thread " << thread << " Skid Buffer:" << endl;
	skidBuffers[thread]->dump();
    }

    cout << "===========================================" << endl << endl;
}


//
//  This should be called between dispatch() and decode()
//
void
DecodeDispatchQueue::regStats()
{
    using namespace Stats;

    cum_insts
	.init(cpu->number_of_threads)
	.name(cpu->name() + ".DDQ:count")
	.desc("cum count of instructions")
	.flags(total)
	;
}

void
DecodeDispatchQueue::regFormulas()
{
    using namespace Stats;

    ddq_rate
	.name(cpu->name() + ".DDQ:rate")
	.desc("average number of instructions")
	.flags(total)
	.precision(0)
	;
    ddq_rate = cum_insts / cpu->numCycles;
}

