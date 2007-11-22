/*
 * Alpha-specific EXO definitions extracted from SimpleScalar's alpha.h
 */

/*
 * alpha.h - Alpha ISA definitions
 *
 * This file is a part of the SimpleScalar tool suite written by
 * Todd M. Austin as a part of the Multiscalar Research Project.
 *
 * The tool suite is currently maintained by Doug Burger and Todd M. Austin.
 *
 * Copyright (C) 1994, 1995, 1996, 1997, 1998 by Todd M. Austin
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

#ifndef __MACHINE_EXO_H_
#define __MACHINE_EXO_H_

/*
 * EIO package configuration/macros
 */

/* expected EIO file format */
#define MD_EIO_FILE_FORMAT		EIO_ALPHA_FORMAT

#define MD_MISC_REGS_TO_EXO(REGS)					\
  exo_new(ec_list,							\
	  /*icnt*/exo_new(ec_integer, (exo_integer_t)sim_num_inst),	\
	  /*PC*/exo_new(ec_address, (exo_integer_t)(REGS)->pc),	\
	  /*NPC*/exo_new(ec_address, (exo_integer_t)(REGS)->npc),	\
	  /*FPCR*/exo_new(ec_integer, (exo_integer_t)(REGS)->miscRegs.fpcr),\
	  /*UNIQ*/exo_new(ec_integer, (exo_integer_t)(REGS)->miscRegs.uniq),\
	  NULL)

#define MD_IREG_TO_EXO(REGS, IDX)					\
  exo_new(ec_address, (exo_integer_t)(REGS)->intRegFile[IDX])

#define MD_FREG_TO_EXO(REGS, IDX)					\
  exo_new(ec_address, (exo_integer_t)(REGS)->floatRegFile.q[IDX])

#define MD_EXO_TO_MISC_REGS(EXO, ICNT, REGS)				\
  /* check EXO format for errors... */					\
  if (!exo								\
      || exo->ec != ec_list						\
      || !exo->as_list.head						\
      || exo->as_list.head->ec != ec_integer				\
      || !exo->as_list.head->next					\
      || exo->as_list.head->next->ec != ec_address			\
      || !exo->as_list.head->next->next					\
      || exo->as_list.head->next->next->ec != ec_address		\
      || !exo->as_list.head->next->next->next				\
      || exo->as_list.head->next->next->next->ec != ec_integer		\
      || !exo->as_list.head->next->next->next->next			\
      || exo->as_list.head->next->next->next->next->ec != ec_integer	\
      || exo->as_list.head->next->next->next->next->next != NULL)	\
    fatal("could not read EIO misc regs");				\
  (ICNT) = (Counter)exo->as_list.head->as_integer.val;		\
  (REGS)->pc = (Addr)exo->as_list.head->next->as_integer.val;	\
  (REGS)->npc =							\
    (Addr)exo->as_list.head->next->next->as_integer.val;		\
  (REGS)->miscRegs.fpcr =							\
    (uint64_t)exo->as_list.head->next->next->next->as_integer.val;	\
  (REGS)->miscRegs.uniq =							\
    (uint64_t)exo->as_list.head->next->next->next->next->as_integer.val;

#define MD_EXO_TO_IREG(EXO, REGS, IDX)					\
  ((REGS)->intRegFile[IDX] = (uint64_t)(EXO)->as_integer.val)

#define MD_EXO_TO_FREG(EXO, REGS, IDX)					\
  ((REGS)->floatRegFile.q[IDX] = (uint64_t)(EXO)->as_integer.val)

#define MD_EXO_CMP_IREG(EXO, REGS, IDX)					\
  ((REGS)->intRegFile[IDX] != (uint64_t)(EXO)->as_integer.val)

#define MD_FIRST_IN_REG			0
#define MD_LAST_IN_REG			21

#define MD_FIRST_OUT_REG		0
#define MD_LAST_OUT_REG			21


/* non-zero if system call is an exit() */
#define OSF_SYS_exit			1
#define MD_EXIT_SYSCALL(REGS)						\
  ((REGS)->intRegFile[ReturnValueReg] == OSF_SYS_exit)

/* non-zero if system call is a write to stdout/stderr */
#define OSF_SYS_write			4
#define MD_OUTPUT_SYSCALL(REGS)						\
  ((REGS)->intRegFile[ReturnValueReg] == OSF_SYS_write				\
   && ((REGS)->intRegFile[ArgumentReg0] == /* stdout */1			\
       || (REGS)->intRegFile[ArgumentReg0] == /* stderr */2))

/* returns stream of an output system call, translated to host */
#define MD_STREAM_FILENO(REGS)		((REGS)->intRegFile[ArgumentReg0])

/*
 * configure the EXO package
 */

/* EXO pointer class */
typedef uint64_t exo_address_t;

/* EXO integer class, 64-bit encoding */
typedef uint64_t exo_integer_t;

/* EXO floating point class, 64-bit encoding */
typedef double exo_float_t;


#endif // __MACHINE_EXO_H_
