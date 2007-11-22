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

#include "arch/alpha/isa_traits.hh"
#include "config/full_system.hh"
#include "cpu/static_inst.hh"
#include "sim/serialize.hh"

// Alpha UNOP (ldq_u r31,0(r0))
const MachInst AlphaISA::NoopMachInst = 0x2ffe0000;

void
AlphaISA::RegFile::serialize(std::ostream &os)
{
    SERIALIZE_ARRAY(intRegFile, NumIntRegs);
    SERIALIZE_ARRAY(floatRegFile.q, NumFloatRegs);
    SERIALIZE_SCALAR(miscRegs.fpcr);
    SERIALIZE_SCALAR(miscRegs.uniq);
    SERIALIZE_SCALAR(miscRegs.lock_flag);
    SERIALIZE_SCALAR(miscRegs.lock_addr);
    SERIALIZE_SCALAR(pc);
    SERIALIZE_SCALAR(npc);
#if FULL_SYSTEM
    SERIALIZE_ARRAY(palregs, NumIntRegs);
    SERIALIZE_ARRAY(ipr, NumInternalProcRegs);
    SERIALIZE_SCALAR(intrflag);
    SERIALIZE_SCALAR(pal_shadow);
#endif
}


void
AlphaISA::RegFile::unserialize(Checkpoint *cp, const std::string &section)
{
    UNSERIALIZE_ARRAY(intRegFile, NumIntRegs);
    UNSERIALIZE_ARRAY(floatRegFile.q, NumFloatRegs);
    UNSERIALIZE_SCALAR(miscRegs.fpcr);
    UNSERIALIZE_SCALAR(miscRegs.uniq);
    UNSERIALIZE_SCALAR(miscRegs.lock_flag);
    UNSERIALIZE_SCALAR(miscRegs.lock_addr);
    UNSERIALIZE_SCALAR(pc);
    UNSERIALIZE_SCALAR(npc);
#if FULL_SYSTEM
    UNSERIALIZE_ARRAY(palregs, NumIntRegs);
    UNSERIALIZE_ARRAY(ipr, NumInternalProcRegs);
    UNSERIALIZE_SCALAR(intrflag);
    UNSERIALIZE_SCALAR(pal_shadow);
#endif
}


#if FULL_SYSTEM
void
AlphaISA::PTE::serialize(std::ostream &os)
{
    SERIALIZE_SCALAR(tag);
    SERIALIZE_SCALAR(ppn);
    SERIALIZE_SCALAR(xre);
    SERIALIZE_SCALAR(xwe);
    SERIALIZE_SCALAR(asn);
    SERIALIZE_SCALAR(asma);
    SERIALIZE_SCALAR(fonr);
    SERIALIZE_SCALAR(fonw);
    SERIALIZE_SCALAR(valid);
}


void
AlphaISA::PTE::unserialize(Checkpoint *cp, const std::string &section)
{
    UNSERIALIZE_SCALAR(tag);
    UNSERIALIZE_SCALAR(ppn);
    UNSERIALIZE_SCALAR(xre);
    UNSERIALIZE_SCALAR(xwe);
    UNSERIALIZE_SCALAR(asn);
    UNSERIALIZE_SCALAR(asma);
    UNSERIALIZE_SCALAR(fonr);
    UNSERIALIZE_SCALAR(fonw);
    UNSERIALIZE_SCALAR(valid);
}

#endif //FULL_SYSTEM
