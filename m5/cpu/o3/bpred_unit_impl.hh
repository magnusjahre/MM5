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

#include "base/trace.hh"
#include "base/traceflags.hh"
#include "cpu/o3/bpred_unit.hh"

template<class Impl>
TwobitBPredUnit<Impl>::TwobitBPredUnit(Params &params)
  : BP(params.local_predictor_size,
       params.local_ctr_bits,
       params.instShiftAmt),
    BTB(params.BTBEntries,
        params.BTBTagSize,
        params.instShiftAmt),
    RAS(params.RASSize)
{
}

template <class Impl>
void
TwobitBPredUnit<Impl>::regStats()
{
    lookups
        .name(name() + ".BPredUnit.lookups")
        .desc("Number of BP lookups")
        ;

    condPredicted
        .name(name() + ".BPredUnit.condPredicted")
        .desc("Number of conditional branches predicted")
        ;

    condIncorrect
        .name(name() + ".BPredUnit.condIncorrect")
        .desc("Number of conditional branches incorrect")
        ;

    BTBLookups
        .name(name() + ".BPredUnit.BTBLookups")
        .desc("Number of BTB lookups")
        ;

    BTBHits
        .name(name() + ".BPredUnit.BTBHits")
        .desc("Number of BTB hits")
        ;

    BTBCorrect
        .name(name() + ".BPredUnit.BTBCorrect")
        .desc("Number of correct BTB predictions (this stat may not "
              "work properly.")
        ;

    usedRAS
        .name(name() + ".BPredUnit.usedRAS")
        .desc("Number of times the RAS was used.")
        ;

    RASIncorrect
        .name(name() + ".BPredUnit.RASInCorrect")
        .desc("Number of incorrect RAS predictions.")
        ;
}

template <class Impl>
bool
TwobitBPredUnit<Impl>::predict(DynInstPtr &inst, Addr &PC)
{
    // See if branch predictor predicts taken.
    // If so, get its target addr either from the BTB or the RAS.
    // Once that's done, speculatively update the predictor?
    // Save off record of branch stuff so the RAS can be fixed
    // up once it's done.

    bool pred_taken = false;
    Addr target;

    ++lookups;

    if (inst->isUncondCtrl()) {
        DPRINTF(Fetch, "BranchPred: Unconditional control.\n");
        pred_taken = true;
    } else {
        ++condPredicted;

        pred_taken = BPLookup(PC);

        DPRINTF(Fetch, "BranchPred: Branch predictor predicted %i for PC %#x"
                "\n", pred_taken, inst->readPC());
    }

    PredictorHistory predict_record(inst->seqNum, PC, pred_taken);

    // Now lookup in the BTB or RAS.
    if (pred_taken) {
        if (inst->isReturn()) {
            ++usedRAS;

            // If it's a function return call, then look up the address 
            // in the RAS.
            target = RAS.top();

            // Record the top entry of the RAS, and its index.
            predict_record.usedRAS = true;
            predict_record.RASIndex = RAS.topIdx();
            predict_record.RASTarget = target;

            RAS.pop();

            DPRINTF(Fetch, "BranchPred: Instruction %#x is a return, RAS "
                    "predicted target: %#x, RAS index: %i.\n",
                    inst->readPC(), target, predict_record.RASIndex);
        } else {
            ++BTBLookups;

            if (inst->isCall()) {
                RAS.push(PC+sizeof(MachInst));
                
                // Record that it was a call so that the top RAS entry can
                // be popped off if the speculation is incorrect.
                predict_record.wasCall = true;
                    
                DPRINTF(Fetch, "BranchPred: Instruction %#x was a call, "
                        "adding %#x to the RAS.\n",
                        inst->readPC(), PC+sizeof(MachInst));
            }

            if (BTB.valid(PC)) {
                ++BTBHits;

                //If it's anything else, use the BTB to get the target addr.
                target = BTB.lookup(PC);

                DPRINTF(Fetch, "BranchPred: Instruction %#x predicted target "
                        "is %#x.\n", inst->readPC(), target);

            } else {
                DPRINTF(Fetch, "BranchPred: BTB doesn't have a valid entry."
                        "\n");
                pred_taken = false;
            }

        }
    }

    if (pred_taken) {
        // Set the PC and the instruction's predicted target.
        PC = target;
        inst->setPredTarg(target);
    } else {
        PC = PC + sizeof(MachInst);
        inst->setPredTarg(PC);
    }

    predHist.push_front(predict_record);

    assert(!predHist.empty());

    return pred_taken;
}

template <class Impl>
void
TwobitBPredUnit<Impl>::update(const InstSeqNum &done_sn)
{
    DPRINTF(Fetch, "BranchPred: Commiting branches until sequence number "
            "%i.\n", done_sn);

    while (!predHist.empty() && predHist.back().seqNum <= done_sn) {
        assert(!predHist.empty());

        // Update the branch predictor with the correct results of branches.
        BP.update(predHist.back().PC, predHist.back().predTaken);

        predHist.pop_back();
    }
}

template <class Impl>
void
TwobitBPredUnit<Impl>::squash(const InstSeqNum &squashed_sn)
{
    while (!predHist.empty() && predHist.front().seqNum > squashed_sn) {
        if (predHist.front().usedRAS) {
            DPRINTF(Fetch, "BranchPred: Restoring top of RAS to: %i, "
                    "target: %#x.\n",
                    predHist.front().RASIndex, 
                    predHist.front().RASTarget);

            RAS.restore(predHist.front().RASIndex, 
                        predHist.front().RASTarget);
        } else if (predHist.front().wasCall) {
            DPRINTF(Fetch, "BranchPred: Removing speculative entry added "
                    "to the RAS.\n");

            RAS.pop();
        }

        predHist.pop_front();
    }
}

template <class Impl>
void
TwobitBPredUnit<Impl>::squash(const InstSeqNum &squashed_sn,
                              const Addr &corr_target,
                              const bool actually_taken)
{
    // Now that we know that a branch was mispredicted, we need to undo
    // all the branches that have been seen up until this branch and
    // fix up everything.

    ++condIncorrect;

    DPRINTF(Fetch, "BranchPred: Squashing from sequence number %i, "
            "setting target to %#x.\n",
            squashed_sn, corr_target);

    while (!predHist.empty() && predHist.front().seqNum > squashed_sn) {

        if (predHist.front().usedRAS) {
            DPRINTF(Fetch, "BranchPred: Restoring top of RAS to: %i, "
                    "target: %#x.\n",
                    predHist.front().RASIndex, 
                    predHist.front().RASTarget);

            RAS.restore(predHist.front().RASIndex, 
                        predHist.front().RASTarget);
        } else if (predHist.front().wasCall) {
            DPRINTF(Fetch, "BranchPred: Removing speculative entry added "
                    "to the RAS.\n");

            RAS.pop();
        }

        predHist.pop_front();
    }

    predHist.front().predTaken = actually_taken;

    if (predHist.front().usedRAS) {
        ++RASIncorrect;
    }
    
    BP.update(predHist.front().PC, actually_taken);

    BTB.update(predHist.front().PC, corr_target);
}
