/*
 * eio.hh - external interfaces to external I/O files
 *
 * This file is a part of the SimpleScalar tool suite written by
 * Todd M. Austin as a part of the Multiscalar Research Project.
 *
 * The tool suite is currently maintained by Doug Burger and Todd M. Austin.
 *
 * Copyright (C) 1997, 1998 by Todd M. Austin
 *
 * This source file is distributed "as is" in the hope that it will be
 * useful.  The tool set comes with no warranty, and no author or
 * distributor accepts any responsibility for the consequences of its
 * use.
 *
 * Everyone is granted permission to copy, modify and redistribute
 * this tool set under the following conditions:
 *
 *    This source code is distributed for non-commercial use only.
 *    Please contact the maintainer for restrictions applying to
 *    commercial use.
 *
 *    Permission is granted to anyone to make or distribute copies
 *    of this source code, either as received or modified, in any
 *    medium, provided that all copyright notices, permission and
 *    nonwarranty notices are preserved, and that the distributor
 *    grants the recipient permission for further redistribution as
 *    permitted by this document.
 *
 *    Permission is granted to distribute this file in compiled
 *    or executable form under the same conditions that apply for
 *    source code, provided that either:
 *
 *    A. it is accompanied by the corresponding machine-readable
 *       source code,
 *    B. it is accompanied by a written offer, with no time limit,
 *       to give anyone a machine-readable copy of the corresponding
 *       source code in return for reimbursement of the cost of
 *       distribution.  This written offer must permit verbatim
 *       duplication by anyone, or
 *    C. it is distributed by someone who received only the
 *       executable form, and is accompanied by a copy of the
 *       written offer of source code that they received concurrently.
 *
 * In other words, you are welcome to use, share and improve this

 * source file.  You are forbidden to forbid anyone else to use, share
 * and improve what you give them.
 *
 * INTERNET: dburger@cs.wisc.edu
 * US Mail:  1210 W. Dayton Street, Madison, WI 53706
 *
 */

#ifndef __EIO_HH__
#define __EIO_HH__

#include <cstdio>

#include "cpu/smt.hh"
#include "encumbered/eio/alpha_exo.h"
#include "sim/process.hh"

/* EIO file formats */
#define EIO_PISA_FORMAT			1
#define EIO_ALPHA_FORMAT		2

/* EIO file version */
#define EIO_FILE_VERSION		3


class EioProcess : public Process
{
  private:
    FILE *eio_fd;
    Counter chkpt_num_inst;

    // read process checkpoint from specified file
    exo_integer_t
    read_chkpt(RegFile *regs, FILE *fd);

    // read syscall from EIO trace in eio_fd
    void read_trace(RegFile *regs,		// registers to update
		    FunctionalMemory *curmem,	// memory space
		    Counter icnt);		// instruction count

  public:
    EioProcess(const std::string &name, int stdout_fd, int stderr_fd,
	       const std::string &eio_file, const std::string &chkpt_file);

    // override syscall handler to read from EIO trace
    virtual void syscall(ExecContext *xc);
};

/* returns non-zero if file FNAME has a valid EIO header */
int eio_valid(char *fname);

#endif // __EIO_HH__
