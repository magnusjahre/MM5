/*
 * Copyright (c) 2001, 2002, 2003, 2004, 2005
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

//==========================================================================
//
//  This predictor takes the AND of a "local" and a "global" predictor
//  in order to determine its prediction.
//
//
//
//

#ifndef __HYBRID_PRED_HH__
#define __HYBRID_PRED_HH__

#include <string>

#include "base/sat_counter.hh"
#include "base/statistics.hh"

class HybridPredictor : public GenericPredictor
{
  private:
    std::string pred_name;
    std::string one_name;
    std::string zero_name;
    bool reg_individual_stats;

    SaturatingCounterPred *local;
    SaturatingCounterPred *global;

    unsigned long max_index;

    //
    //  Stats
    //
    Stats::Scalar<> pred_one; //num_one_preds
    Stats::Scalar<> pred_zero; //num_zero_preds
    Stats::Scalar<> correct_pred_one; //num_one_correct
    Stats::Scalar<> correct_pred_zero; //num_zero_correct
    Stats::Scalar<> record_one; //num_one_updates
    Stats::Scalar<> record_zero; //num_zero_updates

    Stats::Formula total_preds;
    Stats::Formula frac_preds_zero;
    Stats::Formula frac_preds_one;
    Stats::Formula total_correct;
    Stats::Formula total_accuracy;
    Stats::Formula zero_accuracy;
    Stats::Formula one_accuracy;
    Stats::Formula zero_coverage;
    Stats::Formula one_coverage;

  public:
    HybridPredictor(const char *_p_name, const char *_z_name,
		    const char *_o_name,
		    unsigned _index_bits, unsigned _counter_bits,
		    unsigned _zero_change, unsigned _one_change,
		    unsigned _thresh,
		    unsigned _global_bits, unsigned _global_thresh,
		    bool _reg_individual_stats = false);

    void clear() {
	global->clear();
	local->clear();
    }

    unsigned peek(unsigned long _index) {
	unsigned l = local->peek(_index);
	unsigned g = global->peek(_index);

	if (l && g)
	    return 1;

	return 0;
    }

    unsigned value(unsigned long _index) {
	unsigned l = local->peek(_index);
	unsigned g = global->peek(_index);

	l = l & 0xFFFF;
	g = g & 0xFFFF;

	return  (l << 16) | g;

    }

    unsigned predict(unsigned long _index) {
	unsigned l = local->predict(_index);
	unsigned g = global->predict(_index);

	if (l && g) {
	    ++pred_one;
	    return 1;
	}

	++pred_zero;
	return 0;
    }


    //
    //  This version need only be used if local/global statistics
    //  will be maintained
    //
    unsigned predict(unsigned long _index, unsigned &_pdata) {
	unsigned l = local->predict(_index);
	unsigned g = global->predict(_index);

	//
	//  bit 0 => local predictor result
	//  bit 1 => global predictor result
	//
	_pdata = 0;
	if (l)
	    _pdata |= 1;
	if (g)
	    _pdata |= 2;
	if (l && g) {
	    ++pred_one;
	    return 1;
	}

	++pred_zero;
	return 0;
    }

    void record(unsigned long _index, unsigned _val, unsigned _predicted) {

	if (_val) {
	    local->record(_index, _val, 0);
	    global->record(_index, _val, 0);
	    ++record_one;

	    if (_val == _predicted) {
		++correct_pred_one;
	    }
	} else {
	    local->record(_index, _val, 0);
	    global->record(_index, _val, 0);
	    ++record_zero;

	    if (_val == _predicted)
		++correct_pred_zero;
	}
    }

    void record(unsigned long _index, unsigned _val, unsigned _predicted,
		unsigned _pdata)
    {

	local->record(_index, _val, (_pdata & 1));
	global->record(_index, _val, ((_pdata & 2) ? 1 : 0));


	if (_val) {
	    ++record_one;

	    if (_val == _predicted)
		++correct_pred_one;
	} else {
	    ++record_zero;

	    if (_val == _predicted)
		++correct_pred_zero;
	}
    }

    void regStats();
    void regFormulas();
};


#endif  // _HYBRID_PRED_HH__

