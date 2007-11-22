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
 * Declaration of a Intel ITX memory trace format reader.
 */
#include <sstream>

#include "cpu/trace/reader/itx_reader.hh"
#include "sim/builder.hh"
#include "base/misc.hh" // for fatal

using namespace std;

ITXReader::ITXReader(const string &name, const string &filename)
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
    traceFormat = 0;
    int c;
    for (int i = 0; i < 4; ++i) {
	c = getc(trace);
	if (c == EOF) {
	    fatal("Unexpected end of trace file.");
	}
	traceFormat |= (c & 0xff) << (8 * i);
    }
    if (traceFormat > 2)
	fatal("Invalid trace format.");
}

Tick
ITXReader::getNextReq(MemReqPtr &req)
{   
    MemReqPtr tmp_req = new MemReq();
    bool phys_val;
    do {
	int c = getc(trace);
	if (c != EOF) {
	    // Decode first byte
	    // phys_val<1> | type <2:0> | size <3:0>
	    phys_val = c & 0x80;
	    tmp_req->size = (c & 0x0f) + 1;
	    int type = (c & 0x70) >> 4;
	    
	    // Could be a compressed instruction entry, expand if necessary
	    if (type == ITXCodeComp) {
		if (traceFormat != 2) {
		    fatal("Compressed code entry in non CompCode trace.");
		}
		if (!codeVirtValid) {
		    fatal("Corrupt CodeComp entry.");
		}
		
		tmp_req->vaddr = codeVirtAddr;
		codeVirtAddr += tmp_req->size;
		if (phys_val) {
		    if (!codePhysValid) {
			fatal("Corrupt CodeComp entry.");
		    }
		    tmp_req->paddr = codePhysAddr;
		    if (((tmp_req->paddr & 0xfff) + tmp_req->size) & ~0xfff) {
			// Crossed page boundary, next physical address is
			// invalid
			codePhysValid = false;
		    } else {
			codePhysAddr += tmp_req->size;
		    }
		    assert(tmp_req->paddr >> 36 == 0);
		} else {
		    codePhysValid = false;
		}
		type = ITXCode;
		tmp_req->cmd = Read;
	    } else {
		// Normal entry
		tmp_req->vaddr = 0;
		for (int i = 0; i < 4; ++i) {
		    c = getc(trace);
		    if (c == EOF) {
			fatal("Unexpected end of trace file.");
		    }
		    tmp_req->vaddr |= (c & 0xff) << (8 * i);
		}
		if (type == ITXCode) {
		    codeVirtAddr = tmp_req->vaddr + tmp_req->size;
		    codeVirtValid = true;
		}
		tmp_req->paddr = 0;
		if (phys_val) {
		    c = getc(trace);
		    if (c == EOF) {
			fatal("Unexpected end of trace file.");
		    }
		    // Get the page offset from the virtual address.
		    tmp_req->paddr = tmp_req->vaddr & 0xfff;
		    tmp_req->paddr |= (c & 0xf0) << 8;
		    tmp_req->paddr |= (Addr)(c & 0x0f) << 32;
		    for (int i = 2; i < 4; ++i) {
			c = getc(trace);
			if (c == EOF) {
			    fatal("Unexpected end of trace file.");
			}
			tmp_req->paddr |= (Addr)(c & 0xff) << (8 * i);
		    }
		    if (type == ITXCode) {
			if (((tmp_req->paddr & 0xfff) + tmp_req->size) 
			    & ~0xfff) {
			    // Crossing the page boundary, next physical 
			    // address isn't valid
			    codePhysValid = false;
			} else {
			    codePhysAddr = tmp_req->paddr + tmp_req->size;
			    codePhysValid = true;
			}
		    }
		    assert(tmp_req->paddr >> 36 == 0);
		} else if (type == ITXCode) {
		    codePhysValid = false;
		}
		switch(type) {
		  case ITXRead:
		    tmp_req->cmd = Read;
		    break;
		  case ITXWrite:
		    tmp_req->cmd = Write;
		    break;
		  case ITXWriteback:
		    tmp_req->cmd = Writeback;
		    break;
		  case ITXCode:
		    tmp_req->cmd = Read;
		    tmp_req->flags |= INST_READ;
		    break;
		  default:
		    fatal("Unknown ITX type");
		}
	    }
	} else {
	    // EOF need to return a null request
	    MemReqPtr null_req;
	    req = null_req;
	    return 0;
	}
    } while (!phys_val);
    req = tmp_req;
    assert(!req || (req->paddr >> 36) == 0);
    return 0;
}
	    
BEGIN_DECLARE_SIM_OBJECT_PARAMS(ITXReader)
  
    Param<string> filename;

END_DECLARE_SIM_OBJECT_PARAMS(ITXReader)


BEGIN_INIT_SIM_OBJECT_PARAMS(ITXReader)

    INIT_PARAM(filename, "trace file")
    
END_INIT_SIM_OBJECT_PARAMS(ITXReader)


CREATE_SIM_OBJECT(ITXReader)
{
    return new ITXReader(getInstanceName(), filename);
}

REGISTER_SIM_OBJECT("ITXReader", ITXReader)
