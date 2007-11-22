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

#include <string>
#include <sstream>

#include "base/hybrid_pred.hh"
#include "base/statistics.hh"
#include "sim/stats.hh"

using namespace std;

HybridPredictor::HybridPredictor(const char *_p_name, const char *_z_name,
				 const char *_o_name,
				 unsigned _index_bits, unsigned _counter_bits,
				 unsigned _zero_change, unsigned _one_change,
				 unsigned _thresh,
				 unsigned _global_bits,
				 unsigned _global_thresh,
				 bool _reg_individual_stats)
{
    stringstream local_name, global_name;

    pred_name = _p_name;
    one_name  = _o_name;
    zero_name = _z_name;
    reg_individual_stats = _reg_individual_stats;

    local_name << pred_name.c_str() << ":L";
    local = new SaturatingCounterPred(local_name.str(), zero_name, one_name,
				      _index_bits, _counter_bits,
				      _zero_change, _one_change, _thresh);

    global_name << pred_name.c_str() << ":G";
    global = new SaturatingCounterPred(global_name.str(), zero_name, one_name,
				       0, _global_bits, 1, 1, _global_thresh);
}

void HybridPredictor::regStats()
{
    using namespace Stats;

    string p_name;
    stringstream description;

    if (reg_individual_stats)
	p_name = pred_name + ":A";
    else
	p_name = pred_name;


    //
    //  Number of predictions
    //
    stringstream num_zero_preds;
    num_zero_preds << p_name << ":" << zero_name << ":preds";
    description << "number of predictions of " << zero_name;
    pred_zero
	.name(num_zero_preds.str())
	.desc(description.str());
    description.str("");

    stringstream num_one_preds;
    num_one_preds << p_name << ":" << one_name << ":preds";
    description << "number of predictions of " << one_name;
    pred_one
	.name(num_one_preds.str())
	.desc(description.str())
	;
    description.str("");

    //
    //  Count the number of correct predictions
    //
    stringstream num_zero_correct;
    num_zero_correct << p_name << ":" << zero_name << ":corr_preds";
    description << "number of correct " << zero_name << " preds" ;
    correct_pred_zero
	.name(num_zero_correct.str())
	.desc(description.str())
	;
    description.str("");

    stringstream num_one_correct;
    num_one_correct << p_name << ":" << one_name << ":corr_preds";
    description << "number of correct " << one_name << " preds" ;
    correct_pred_one
	.name(num_one_correct.str())
	.desc(description.str())
	;
    description.str("");


    //
    //  Number of predictor updates
    //
    stringstream num_zero_updates;
    num_zero_updates << p_name << ":" << zero_name << ":updates" ;
    description << "number of actual " << zero_name << "s" ;
    record_zero
	.name(num_zero_updates.str())
	.desc(description.str())
	;
    description.str("");

    stringstream num_one_updates;
    num_one_updates << p_name << ":" << one_name << ":updates" ;
    description << "number of actual " << one_name << "s" ;
    record_one
	.name(num_one_updates.str())
	.desc(description.str())
	;
    description.str("");

    //
    //  Local & Global predictor stats
    //
    if (reg_individual_stats) {
	local->regStats();
	global->regStats();
    }
}

void HybridPredictor::regFormulas()
{
    using namespace Stats;

    string p_name;
    stringstream description;
    stringstream name;

    if (reg_individual_stats)
	p_name = pred_name + ":A";
    else
	p_name = pred_name;

    //
    //  Number of predictions
    //
    name << p_name << ":predictions" ;
    total_preds
	.name(name.str())
	.desc("total number of predictions made")
	;
    total_preds = pred_one + pred_zero;
    name.str("");

    //
    //  Fraction of all predictions that are one or zero
    //
    name << p_name << ":" << zero_name << ":pred_frac";
    description << "fraction of all preds that were " << zero_name ;
    frac_preds_zero
	.name(name.str())
	.desc(description.str())
	;
    frac_preds_zero = 100 * record_zero / total_preds;
    description.str("");
    name.str("");

    name << p_name << ":" << one_name << ":pred_frac";
    description << "fraction of all preds that were " << one_name ;
    frac_preds_one
	.name(name.str())
	.desc(description.str())
	;
    frac_preds_one = 100 * record_one / total_preds;
    description.str("");
    name.str("");

    //
    //  Count the number of correct predictions
    //
    name << p_name << ":correct_preds" ;
    total_correct
	.name(name.str())
	.desc("total number of correct predictions made")
	;
    total_correct = correct_pred_one + correct_pred_zero;
    name.str("");


    //
    //  Prediction accuracy rates
    //
    name << p_name << ":pred_rate";
    total_accuracy
	.name(name.str())
	.desc("fraction of all preds that were correct")
	;
    total_accuracy = 100 * total_correct / total_preds;
    name.str("");

    name << p_name << ":" << zero_name << ":pred_rate" ;
    description << "fraction of "<< zero_name <<" preds that were correct";
    zero_accuracy
	.name(name.str())
	.desc(description.str())
	;
    zero_accuracy = 100 * correct_pred_zero / pred_zero;
    description.str("");
    name.str("");

    name << p_name << ":" << one_name << ":pred_rate" ;
    description << "fraction of "<< one_name <<" preds that were correct";
    one_accuracy
	.name(name.str())
	.desc(description.str())
	;
    one_accuracy = 100 * correct_pred_one / pred_one;
    description.str("");
    name.str("");

    //
    //  Coverage
    //
    name << p_name << ":" << zero_name << ":coverage";
    description << "fraction of " << zero_name
		<< "s that were predicted correctly";
    zero_coverage
	.name(name.str())
	.desc(description.str())
	;
    zero_coverage = 100 * correct_pred_zero / record_zero;
    description.str("");
    name.str("");

    name << p_name << ":" << one_name << ":coverage";
    description << "fraction of " << one_name
		<< "s that were predicted correctly";
    one_coverage
	.name(name.str())
	.desc(description.str())
	;
    one_coverage = 100 * correct_pred_one / record_one;
    description.str("");
    name.str("");

    //
    //  Local & Global predictor stats
    //
    if (reg_individual_stats) {
	local->regFormulas();
	global->regFormulas();
    }

}

