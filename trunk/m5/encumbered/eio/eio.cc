/* $Id: eio.cc 1.51 05/06/04 22:33:09-04:00 binkertn@crampon.my.domain $ */

/*
 * eio.cc - external interfaces to external I/O f\iles
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

#include <unistd.h>

#include <string>

#include "base/cprintf.hh"
#include "base/endian.hh"
#include "base/misc.hh"
#include "cpu/smt.hh"
#include "encumbered/cpu/full/spec_state.hh"
#include "encumbered/eio/eio.hh"
#include "sim/builder.hh"
#include "sim/host.hh"
#include "sim/root.hh"	// for curTick

// note that libexo.h has to be last, as it does some nasty #defines;
// specifically, it #defines as_float in a way that conflicts with a
// field name in eval.h.
#include "encumbered/eio/libexo.h"

using namespace std;

static struct {
  char *type;
  char *ext;
  char *cmd;
} gzcmds[] = {
  /* type */	/* extension */		/* command */
  { "r",	".gz",			"%s -dc %s" },
  { "rb",	".gz",			"%s -dc %s" },
  { "r",	".Z",			"%s -dc %s" },
  { "rb",	".Z",			"%s -dc %s" },
  { "w",	".gz",			"%s > %s" },
  { "wb",	".gz",			"%s > %s" }
};

#ifndef GZIP_PATH
#define GZIP_PATH "gzip"
#endif

/* same semantics as fopen() except that filenames ending with a ".gz" or ".Z"
   will be automagically get compressed */
FILE *
gzopen(const char *fname, const char *type)
{
  int i;
  char *cmd = NULL;
  const char *ext;
  FILE *fd;
  char str[2048];

  /* get the extension */
  ext = strrchr(fname, '.');

  /* check if extension indicates compressed file */
  if (ext != NULL && *ext != '\0')
    {
      for (i=0; i < sizeof(gzcmds) / sizeof(gzcmds[0]); i++)
	{
	  if (!strcmp(gzcmds[i].type, type) && !strcmp(gzcmds[i].ext, ext))
	    {
	      cmd = gzcmds[i].cmd;
	      break;
	    }
	}
    }

  if (!cmd)
    {
      /* open file */
      fd = fopen(fname, type);
    }
  else
    {
      /* open pipe to compressor/decompressor */
      sprintf(str, cmd, GZIP_PATH, fname);
      fd = popen(str, type);
    }

  return fd;
}

/* close compressed stream */
void
gzclose(FILE *fd)
{
  /* attempt pipe close, otherwise file close */
  if (pclose(fd) == -1)
    fclose(fd);
}

#ifdef _MSC_VER
#define write		_write
#endif

#define EIO_FILE_HEADER							\
  "/* This is a SimpleScalar EIO file - DO NOT MOVE OR EDIT THIS LINE! */\n"
/*
   EIO transaction format:

   (inst_count, pc,
    ... reg inputs ...
    [r2, r3, r4, r5, r6, r7],
    ... mem inputs ...
    ((addr, size, blob), ...)
    ... reg outputs ...
    [r2, r3, r4, r5, r6, r7],
    ... mem outputs ...
    ((addr, size, blob), ...)
   )
*/

FILE *
eio_open(const string &fname)
{
    FILE *fd;
    struct exo_term_t *exo;
    int file_format, file_version, big_endian, target_big_endian;

    target_big_endian = HostBigEndian();

    fd = gzopen(fname.c_str(), "r");
    if (!fd)
	fatal("unable to open EIO file `%s'", fname);

    /* read and check EIO file header */
    exo = exo_read(fd);
    if (!exo
	|| exo->ec != ec_list
	|| !exo->as_list.head
	|| exo->as_list.head->ec != ec_integer
	|| !exo->as_list.head->next
	|| exo->as_list.head->next->ec != ec_integer
	|| !exo->as_list.head->next->next
	|| exo->as_list.head->next->next->ec != ec_integer
	|| exo->as_list.head->next->next->next != NULL)
	fatal("could not read EIO file header");

    file_format = exo->as_list.head->as_integer.val;
    file_version = exo->as_list.head->next->as_integer.val;
    big_endian = exo->as_list.head->next->next->as_integer.val;
    exo_delete(exo);

    if (file_format != MD_EIO_FILE_FORMAT)
	fatal("EIO file `%s' has incompatible format", fname);

    if (file_version != EIO_FILE_VERSION)
	fatal("EIO file `%s' has incompatible version", fname);

    if (!!big_endian != !!target_big_endian)
	fatal("EIO file `%s' has incompatible endian format", fname);

    return fd;
}


/* returns non-zero if file FNAME has a valid EIO header */
int
eio_valid(const string &fname)
{
    FILE *fd;
    char buf[512];

    /* open possible EIO file */
    fd = gzopen(fname.c_str(), "r");
    if (!fd)
	return false;

    /* read and check EIO file header */
    fgets(buf, 512, fd);

    /* check the header */
    if (strcmp(buf, EIO_FILE_HEADER))
	return false;

    /* all done, close up file */
    gzclose(fd);

    /* else, has a valid header, go with it... */
    return true;
}


void
eio_close(FILE * fd)
{
    gzclose(fd);
}

/* read check point of architected state from stream FD, returns
   EIO transaction count (an EIO file pointer) */
exo_integer_t
EioProcess::read_chkpt(RegFile *regs,
			  FILE *fd) /* stream to read */
{
    int i, page_count;
    exo_integer_t trans_icnt;
    struct exo_term_t *exo, *elt;

    /* read the EIO file pointer */
    exo = exo_read(fd);
    if (!exo || exo->ec != ec_integer)
	fatal("could not read EIO file pointer");
    trans_icnt = exo->as_integer.val;
    exo_delete(exo);

    /* read misc regs: icnt, PC, NPC, HI, LO, FCC */
    exo = exo_read(fd);
    MD_EXO_TO_MISC_REGS(exo, chkpt_num_inst, regs);
    exo_delete(exo);

    /* read integer registers */
    exo = exo_read(fd);
    if (!exo || exo->ec != ec_list)
	fatal("could not read EIO integer regs");
    elt = exo->as_list.head;
    for (i = 0; i < NumIntRegs; i++) {
	if (!elt)
	    fatal("could not read EIO integer regs (too few)");

	if (elt->ec != ec_address)
	    fatal("could not read EIO integer regs (bad value)");

	MD_EXO_TO_IREG(elt, regs, i);
	elt = elt->next;
    }
    if (elt != NULL)
	fatal("could not read EIO integer regs (too many)");
    exo_delete(exo);

    /* read FP registers */
    exo = exo_read(fd);
    if (!exo || exo->ec != ec_list)
	fatal("could not read EIO FP regs");
    elt = exo->as_list.head;
    for (i = 0; i < NumFloatRegs; i++) {
	if (!elt)
	    fatal("could not read EIO FP regs (too few)");

	if (elt->ec != ec_address)
	    fatal("could not read EIO FP regs (bad value)");

	MD_EXO_TO_FREG(elt, regs, i);
	elt = elt->next;
    }
    if (elt != NULL)
	fatal("could not read EIO FP regs (too many)");
    exo_delete(exo);

    /* read the number of page defs, and memory config */
    exo = exo_read(fd);
    if (!exo
	|| exo->ec != ec_list
	|| !exo->as_list.head
	|| exo->as_list.head->ec != ec_integer
	|| !exo->as_list.head->next
	|| exo->as_list.head->next->ec != ec_address
	|| !exo->as_list.head->next->next
	|| exo->as_list.head->next->next->ec != ec_address
	|| exo->as_list.head->next->next->next != NULL)
	fatal("could not read EIO memory page count");
    page_count = exo->as_list.head->as_integer.val;
    brk_point = (Addr) exo->as_list.head->next->as_integer.val;
    stack_min = (Addr) exo->as_list.head->next->next->as_integer.val;
    exo_delete(exo);

    /* read text segment specifiers */
    exo = exo_read(fd);
    if (!exo
	|| exo->ec != ec_list
	|| !exo->as_list.head
	|| exo->as_list.head->ec != ec_address
	|| !exo->as_list.head->next
	|| exo->as_list.head->next->ec != ec_integer
	|| exo->as_list.head->next->next != NULL)
	fatal("count not read EIO text segment specifiers");
    text_base = (Addr) exo->as_list.head->as_integer.val;
    text_size = (unsigned int) exo->as_list.head->next->as_integer.val;
    exo_delete(exo);

    /* read data segment specifiers */
    exo = exo_read(fd);
    if (!exo
	|| exo->ec != ec_list
	|| !exo->as_list.head
	|| exo->as_list.head->ec != ec_address
	|| !exo->as_list.head->next
	|| exo->as_list.head->next->ec != ec_integer
	|| exo->as_list.head->next->next != NULL)
	fatal("count not read EIO data segment specifiers");
    data_base = (Addr) exo->as_list.head->as_integer.val;
    data_size = (unsigned int) exo->as_list.head->next->as_integer.val;
    exo_delete(exo);

    /* read stack segment specifiers */
    exo = exo_read(fd);
    if (!exo
	|| exo->ec != ec_list
	|| !exo->as_list.head
	|| exo->as_list.head->ec != ec_address
	|| !exo->as_list.head->next
	|| exo->as_list.head->next->ec != ec_integer
	|| exo->as_list.head->next->next != NULL)
	fatal("count not read EIO stack segment specifiers");
    stack_base = (Addr) exo->as_list.head->as_integer.val;
    stack_size = (unsigned int) exo->as_list.head->next->as_integer.val;
    //Make the stack size 16MB
    next_thread_stack_base = stack_base - (16 * 1024 * 1024);
    exo_delete(exo);

    for (i = 0; i < page_count; i++) {
	int j;
	Addr page_addr;
	struct exo_term_t *blob;

	/* read the page */
	exo = exo_read(fd);
	if (!exo
	    || exo->ec != ec_list
	    || !exo->as_list.head
	    || exo->as_list.head->ec != ec_address
	    || !exo->as_list.head->next
	    || exo->as_list.head->next->ec != ec_blob
	    || exo->as_list.head->next->next != NULL)
	    fatal("could not read EIO memory page");
	page_addr = (Addr) exo->as_list.head->as_integer.val;
	blob = exo->as_list.head->next;

	/* write data to simulator memory */
	for (j = 0; j < blob->as_blob.size; j++) {
	    uint8_t val;

	    val = blob->as_blob.data[j];
	    /* unchecked access... */
	    memory->access(Write, page_addr, &val, 1);
	    page_addr++;
	}
	exo_delete(exo);
    }

    return trans_icnt;
}


/* syscall proxy handler from an EIO trace, architect registers
   and memory are assumed to be precise when this function is called,
   register and memory are updated with the results of the system call */

// Note that we pass in a memory object that may be different from the
// process's memory object (mem field), since we may be executing the
// syscall early in the pipeline on uncommitted memory state.  It
// might be reasonable to eliminate this parameter if we do syscalls
// at commit time, or only after flushing the pipeline, etc.

void
EioProcess::read_trace(RegFile *regs,	/* registers to update */
		       FunctionalMemory *curmem, /* memory space */
		       Counter icnt)	/* instruction count */
{
    int i;
    struct exo_term_t *exo = 0, *exo_icnt = 0, *exo_pc = 0;
    struct exo_term_t *exo_inregs = 0, *exo_inmem = 0,
	              *exo_outregs = 0, *exo_outmem = 0;
    struct exo_term_t *brkrec, *regrec, *memrec;

    /* exit() system calls get executed for real... */
    if (MD_EXIT_SYSCALL(regs)) {
	/*
	 * FIXME: Not clear what to do for a multithreaded workload
	 * where one thread may call exit before the simulation
	 * is complete.
	 */
	panic("EIO program called exit()");
    }

    /* else, read the external I/O (EIO) transaction */
    exo = exo_read(eio_fd);

    /* if we started from a checkpoint, we need to add in the number of
     * instructions executed at the chkpt to the number simulated thus
     * far to get our distance from the start of the program */
    icnt += chkpt_num_inst;

    /* pull apart the EIO transaction (EXO format) */
    if (!exo || exo->ec != ec_list || !(exo_icnt = exo->as_list.head)
	|| exo_icnt->ec != ec_integer || !(exo_pc = exo_icnt->next)
	|| exo_pc->ec != ec_address || !(exo_inregs = exo_pc->next)
	|| exo_inregs->ec != ec_list || !(exo_inmem = exo_inregs->next)
	|| exo_inmem->ec != ec_list || !(exo_outregs = exo_inmem->next)
	|| exo_outregs->ec != ec_list || !(exo_outmem = exo_outregs->next)
	|| exo_outmem->ec != ec_list || exo_outmem->next != NULL)
	fatal("%s: cannot read EIO transaction", name());

    /*
     * check the system call inputs
     */

    /* check ICNT input */
    if (icnt != (Counter) exo_icnt->as_integer.val) {
	ccprintf(cerr, "actual=%d, eio=%d\n", icnt,
		 (Counter) exo_icnt->as_integer.val);
	fatal("%s: EIO trace inconsistency: ICNT mismatch", name());
    }

    /* check PC input */
    if (regs->pc != (Addr) exo_pc->as_integer.val) {
	ccprintf(cerr, "actual=%d, eio=%d\n", regs->pc,
		 (Counter) exo_pc->as_integer.val);
	fatal("%s: EIO trace inconsistency: PC mismatch", name());
    }

    /* check integer register inputs */
    for (i = MD_FIRST_IN_REG, regrec = exo_inregs->as_list.head;
	 i <= MD_LAST_IN_REG; i++, regrec = regrec->next) {

	if (!regrec || regrec->ec != ec_address) {
	    ccprintf(cerr, "icount=%d, cycle=%d\n",
		     icnt, curTick);
	    fatal("%s: EIO trace inconsistency: missing input reg", name());
	}

	if (MD_EXO_CMP_IREG(regrec, regs, i)) {
	    ccprintf(cerr, "icount=%d, cycle=%d\n", icnt, curTick);
	    fatal("%s: EIO trace inconsistency: R[%d] input mismatch", name(),
		  i);
	}

#ifdef VERBOSE
	ccprintf(cerr, "** R[%d] checks out...\n", i);
#endif				/* VERBOSE */
    }

    if (regrec != NULL) {
	ccprintf(cerr, "icount=%d, cycle=%d\n", icnt, curTick);
	fatal("%s: EIO trace inconsistency: too many input regs", name());
    }

    /* check memory inputs */
    for (memrec = exo_inmem->as_list.head; memrec != NULL;
	 memrec = memrec->next) {
	Addr loc;
	struct exo_term_t *addr = 0, *blob = 0;

	/* check the mem transaction format */
	if (!memrec || memrec->ec != ec_list || !(addr = memrec->as_list.head)
	    || addr->ec != ec_address || !(blob = addr->next)
	    || blob->ec != ec_blob || blob->next != NULL)
	{
	    ccprintf(cerr, "icount=%d, cycle=%d\n", icnt, curTick);
	    fatal("%s: EIO trace inconsistency: bad memory transaction",
		  name());
	}

	for (loc = addr->as_integer.val, i = 0; i < blob->as_blob.size;
	     loc++, i++) {
	    unsigned char val;

	    /* was: val = MEM_READ_BYTE(loc); */
	    curmem->access(Read, loc, &val, sizeof(unsigned char));

	    if (val != blob->as_blob.data[i]) {
		ccprintf(cerr, "icount=%d, cycle=%d\n", icnt, curTick);
		fatal("%s: EIO trace inconsistency: "
		      "addr 0x%08p input mismatch", name(), loc);
	    }

#ifdef VERBOSE
	    ccprintf(cerr, "** %#08d checks out...\n", loc);
#endif				/* VERBOSE */
	}

	/* echo stdout/stderr output */
	if (MD_OUTPUT_SYSCALL(regs)) {
	    int tgt_fd = MD_STREAM_FILENO(regs);

	    if (tgt_fd == STDOUT_FILENO || tgt_fd == STDERR_FILENO) {
		int real_fd = sim_fd(tgt_fd);

		if (real_fd >= 0) {
		    write(real_fd, blob->as_blob.data, blob->as_blob.size);
		}
	    }
	}
    }

    /*
     * write system call outputs
     */

    /* adjust breakpoint */
    brkrec = exo_outregs->as_list.head;
    if (!brkrec || brkrec->ec != ec_address) {
	ccprintf(cerr, "icount=%d, cycle=%d\n", icnt, curTick);
	fatal("%s: EIO trace inconsistency: missing memory breakpoint",
	      name());
    }

    brk_point = (Addr) brkrec->as_integer.val;

    /* write integer register outputs */
    for (i = MD_FIRST_OUT_REG, regrec = exo_outregs->as_list.head->next;
	 i <= MD_LAST_OUT_REG; i++, regrec = regrec->next) {
	if (!regrec || regrec->ec != ec_address) {
	    ccprintf(cerr, "icount=%d, cycle=%d\n", icnt, curTick);
	    fatal("%s: EIO trace inconsistency: missing output reg", name());
	}

	MD_EXO_TO_IREG(regrec, regs, i);

#ifdef VERBOSE
	ccprintf(cerr, "** R[%d] written...\n", i);
#endif				/* VERBOSE */
    }
    if (regrec != NULL) {
	ccprintf(cerr, "icount=%d, cycle=%d\n", icnt, curTick);
	fatal("%s: EIO trace inconsistency: too many output regs", name());
    }

    /* write memory outputs */
    for (memrec = exo_outmem->as_list.head; memrec != NULL;
	 memrec = memrec->next) {
	Addr loc;
	struct exo_term_t *addr = 0, *blob = 0;

	/* check the mem transaction format */
	if (!memrec || memrec->ec != ec_list || !(addr = memrec->as_list.head)
	    || addr->ec != ec_address || !(blob = addr->next)
	    || blob->ec != ec_blob || blob->next != NULL)
	{
	    ccprintf(cerr, "icount=%d, cycle=%d\n", icnt, curTick);
	    fatal("%s: EIO trace icnonsistency: bad memory transaction",
		  name());
	}

	for (loc = addr->as_integer.val, i = 0; i < blob->as_blob.size;
	     loc++, i++) {
	    /* was: MEM_WRITE_BYTE(loc, blob->as_blob.data[i]); */
	    curmem->access(Write, loc, &blob->as_blob.data[i],
			   sizeof(unsigned char));

#ifdef VERBOSE
	    ccprintf(cerr, "** %#08d written...\n", loc);
#endif				/* VERBOSE */
	}
    }

    /* release the EIO EXO node */
    exo_delete(exo);
}

/* fast forward EIO trace EIO_FD to the transaction just after ICNT */
void
eio_fast_forward(FILE * eio_fd, Counter icnt)
{
    struct exo_term_t *exo;
    Counter this_icnt;

    do {

	/* read the next external I/O (EIO) transaction */
	exo = exo_read(eio_fd);

	/* pull apart the EIO transaction (EXO format) */
	if (!exo
	    || exo->ec != ec_list
	    || !exo->as_list.head || exo->as_list.head->ec != ec_integer)
	    fatal("cannot read EIO transaction (during fast forward)");

	this_icnt = exo->as_list.head->as_integer.val;

	exo_delete(exo);

    } while (this_icnt < icnt);

    /* instruction counts should match exactly */
    if (this_icnt > icnt)
	fatal("EIO transaction icnt mismatch during fast forward");
}


EioProcess::EioProcess(const string &name, int stdout_fd, int stderr_fd,
		       const string &eio_file, const string &chkpt_file)
    : Process(name,
	      -1, // stdin_fd unused: all input redirecte from EIO trace
	      stdout_fd, stderr_fd)
{
    /* open the EIO file stream */
    eio_fd = eio_open(eio_file);

    /* load initial state checkpoint */
    if (read_chkpt(init_regs, eio_fd) != -1)
	fatal("bad initial checkpoint in EIO file");

    /* load checkpoint? */
    if (chkpt_file != "")
    {
	Counter restore_icnt;
	FILE *chkpt_fd;

	cerr << "sim: loading checkpoint file: " << chkpt_file << "\n";

	if (!eio_valid(chkpt_file)) {
	    cerr << "file `" << chkpt_file
		 << "' does not appear to be a checkpoint file" << endl;
	    cerr << " ==> Running without checkpoint!" << endl;
	}
	else {
	    /* open the checkpoint file */
	    chkpt_fd = eio_open(chkpt_file);

	    /* load the state image */
	    restore_icnt = read_chkpt(init_regs, chkpt_fd);

	    /* fast forward the baseline EIO trace to checkpoint location */
	    ccprintf(cerr, "sim: fast forwarding to instruction %d\n",
		     restore_icnt);
	    eio_fast_forward(eio_fd, restore_icnt);

	    eio_close(chkpt_fd);
	}
    }

    /* computed state... */
    prog_entry = init_regs->pc;
}


void
EioProcess::syscall(ExecContext *xc)
{
    num_syscalls++;

    read_trace(&xc->regs, xc->mem, xc->func_exe_inst);
}



BEGIN_DECLARE_SIM_OBJECT_PARAMS(EioProcess)

    Param<string> file;
    Param<string> chkpt;
    Param<string> output;

END_DECLARE_SIM_OBJECT_PARAMS(EioProcess)


BEGIN_INIT_SIM_OBJECT_PARAMS(EioProcess)

    INIT_PARAM(file, "EIO trace file name"),
    INIT_PARAM(chkpt, "EIO checkpoint file name (optional)"),
    INIT_PARAM(output, "filename for stdout/stderr (dflt: use sim stdout)")

END_INIT_SIM_OBJECT_PARAMS(EioProcess)


CREATE_SIM_OBJECT(EioProcess)
{
    // initialize file descriptors to default: same as simulator
    int stdout_fd, stderr_fd;

    string out = output;

    if (out == "stdout" || out == "cout")
	stdout_fd = STDOUT_FILENO;
    else if (out == "stderr" || out == "cerr")
	stdout_fd = STDERR_FILENO;
    else
	stdout_fd = Process::openOutputFile(out);

    stderr_fd = (stdout_fd != STDOUT_FILENO) ? stdout_fd : STDERR_FILENO;

    return new EioProcess(getInstanceName(), stdout_fd, stderr_fd, file,
			  chkpt);
}


REGISTER_SIM_OBJECT("EioProcess", EioProcess)
