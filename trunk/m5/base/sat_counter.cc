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

#include <sstream>

#include "base/sat_counter.hh"
#include "base/statistics.hh"
#include "sim/stats.hh"


using namespace std;


SaturatingCounterPred::SaturatingCounterPred(string p_name,
					     string z_name,
					     string o_name,
					     unsigned _index_bits,
					     unsigned _counter_bits,
					     unsigned _zero_change,
					     unsigned _one_change,
					     unsigned _thresh,
					     unsigned _init_value)
{
    pred_name    = p_name;
    zero_name    = z_name;
    one_name     = o_name;

    index_bits   = _index_bits;
    counter_bits = _counter_bits;
    zero_change  = _zero_change;
    one_change   = _one_change;
    thresh       = _thresh;
    init_value   = _init_value;

    max_index = (1 << index_bits) - 1;
    max_value = (1 << counter_bits) - 1;

    table = new unsigned[max_index + 1];

    //  Initialize with the right parameters & clear the counter
    for (int i = 0; i <= max_index; ++i)
	table[i] = init_value;
}

void SaturatingCounterPred::regStats()
{
    using namespace Stats;
    stringstream name, description;

    //
    //  Number of predictions
    //
    name << pred_name << ":" << zero_name << ":preds";
    description << "number of predictions of " << zero_name;
    predicted_zero
	.name(name.str())
	.desc(description.str())
	;
    description.str("");
    name.str("");

    name << pred_name << ":" << one_name << ":preds";
    description << "number of predictions of " << one_name;
    predicted_one
	.name(name.str())
	.desc(description.str())
	;
    description.str("");
    name.str("");

    //
    //  Count the number of correct predictions
    //
    name << pred_name << ":" << zero_name << ":corr_preds";
    description << "number of correct " << zero_name << " preds";
    correct_pred_zero
	.name(name.str())
	.desc(description.str())
	;
    description.str("");
    name.str("");

    name << pred_name << ":" << one_name << ":corr_preds";
    description << "number of correct " << one_name << " preds";
    correct_pred_one
	.name(name.str())
	.desc(description.str())
	;
    description.str("");
    name.str("");

    //
    //  Number of predictor updates
    //
    name << pred_name << ":" << zero_name << ":updates";
    description << "number of actual " << zero_name << "s";
    record_zero
	.name(name.str())
	.desc(description.str())
	;
    description.str("");
    name.str("");

    name << pred_name << ":" << one_name << ":updates";
    description << "number of actual " << one_name << "s";
    record_one
	.name(name.str())
	.desc(description.str())
	;
    description.str("");
    name.str("");
}

void SaturatingCounterPred::regFormulas()
{
    using namespace Stats;
    stringstream name, description;

    //
    //  Number of predictions
    //
    name << pred_name << ":predictions";
    preds_total
	.name(name.str())
	.desc("total number of predictions made")
	;
    preds_total = predicted_zero + predicted_one;
    name.str("");

    //
    //  Fraction of all predictions that are one or zero
    //
    name << pred_name << ":" << zero_name << ":pred_frac";
    description << "fraction of all preds that were " << zero_name;
    pred_frac_zero
	.name(name.str())
	.desc(description.str())
	;
    pred_frac_zero = 100 * predicted_zero / preds_total;
    description.str("");
    name.str("");

    name << pred_name << ":" << one_name << ":pred_frac";
    description << "fraction of all preds that were " << one_name;
    pred_frac_one
	.name(name.str())
	.desc(description.str())
	;
    pred_frac_one = 100 * predicted_one / preds_total;
    description.str("");
    name.str("");


    //
    //  Count the number of correct predictions
    //
    name << pred_name << ":correct_preds";
    correct_total
	.name(name.str())
	.desc("total correct predictions made")
	;
    correct_total = correct_pred_one + correct_pred_zero;
    name.str("");

    //
    //  Number of predictor updates
    //
    name << pred_name << ":updates";
    updates_total
	.name(name.str())
	.desc("total number of updates")
	;
    updates_total = record_zero + record_one;
    name.str("");

    //
    //  Prediction accuracy rates
    //
    name << pred_name << ":pred_rate";
    pred_rate
	.name(name.str())
	.desc("correct fraction of all preds")
	;
    pred_rate = correct_total / updates_total;
    name.str("");

    name << pred_name << ":" << zero_name << ":pred_rate";
    description << "fraction of " << zero_name << " preds that were correct";
    frac_correct_zero
	.name(name.str())
	.desc(description.str())
	;
    frac_correct_zero = 100 * correct_pred_zero /
	(correct_pred_zero + record_one - correct_pred_one);
    description.str("");
    name.str("");

    name << pred_name << ":" << one_name << ":pred_rate";
    description << "fraction of " << one_name << " preds that were correct";
    frac_correct_one
	.name(name.str())
	.desc(description.str())
	;
    frac_correct_one = 100 * correct_pred_one /
	(correct_pred_one + record_zero - correct_pred_zero);
    description.str("");
    name.str("");

    //
    //  Coverage
    //
    name << pred_name << ":" << zero_name << ":coverage";
    description << "fraction of " << zero_name
		<< "s that were predicted correctly";
    coverage_zero
	.name(name.str())
	.desc(description.str())
	;
    coverage_zero = 100 * correct_pred_zero / record_zero;
    description.str("");
    name.str("");

    name << pred_name << ":" << one_name << ":coverage";
    description << "fraction of " << one_name
		<< "s that were predicted correctly";
    coverage_one
	.name(name.str())
	.desc(description.str())
	;
    coverage_one = 100 * correct_pred_one / record_one;
    description.str("");
    name.str("");
}










