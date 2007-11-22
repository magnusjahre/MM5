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

/**
 * @file
 * Declaration of a IBM memory trace format reader.
 */
#include <sstream>

#include "cpu/trace/reader/ibm_reader.hh"
#include "sim/builder.hh"
#include "base/misc.hh" // for fatal

using namespace std;

IBMReader::IBMReader(const string &name, const string &filename)
    : MemTraceReader(name)
{
    if (strcmp((filename.c_str() + filename.length() -3), ".gz") == 0) {
	// Compressed file, need to use a pipe to gzip.
	stringstream buf;
	buf << "gzip -d -c " << filename << endl;
	trace = popen(buf.str().c_str(), "r");
    } else {
	trace = fopen(filename.c_str(), "rb");
    }
    if (!trace) {
	fatal("Can't open file %s", filename);
    }
}

Tick
IBMReader::getNextReq(MemReqPtr &req)
{
    MemReqPtr tmp_req;
   
    int c = getc(trace);
    if (c != EOF) {
	tmp_req = new MemReq();
	//int cpu_id = (c & 0xf0) >> 4;
	int type = c & 0x0f;
	// We have L1 miss traces, so all accesses are 128 bytes
	tmp_req->size = 128;

	tmp_req->paddr = 0;
	for (int i = 2; i >= 0; --i) {
	    c = getc(trace);
	    if (c == EOF) {
		fatal("Unexpected end of file");
	    }
	    tmp_req->paddr |= ((c & 0xff) << (8 * i));
	}
	tmp_req->paddr = tmp_req->paddr << 7;
	
	switch(type) {
	  case IBM_COND_EXCLUSIVE_FETCH:
	  case IBM_READ_ONLY_FETCH:
	    tmp_req->cmd = Read;
	    break;
	  case IBM_EXCLUSIVE_FETCH:
	  case IBM_FETCH_NO_DATA:
	    tmp_req->cmd = Write;
	    break;
	  case IBM_INST_FETCH:
	    tmp_req->cmd = Read;
	    break;
	  default:
	    fatal("Unknown trace entry type.");
	}
	   
    }
    req = tmp_req;
    return 0;
}
	    
BEGIN_DECLARE_SIM_OBJECT_PARAMS(IBMReader)
  
    Param<string> filename;

END_DECLARE_SIM_OBJECT_PARAMS(IBMReader)


BEGIN_INIT_SIM_OBJECT_PARAMS(IBMReader)

    INIT_PARAM(filename, "trace file")
    
END_INIT_SIM_OBJECT_PARAMS(IBMReader)


CREATE_SIM_OBJECT(IBMReader)
{
    return new IBMReader(getInstanceName(), filename);
}

REGISTER_SIM_OBJECT("IBMReader", IBMReader)
