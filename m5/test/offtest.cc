/*
 * Copyright (c) 2002, 2003, 2004, 2005
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


#include <sys/types.h>
#include <stddef.h>
#include <stdio.h>
#include "dev/pcireg.h"

int
main()
{
#define POFFSET(x) \
  printf("offsetof(PCIConfig, hdr."#x") = %d\n", \
          offsetof(PCIConfig, hdr.x))

  POFFSET(vendor);
  POFFSET(device);
  POFFSET(command);
  POFFSET(status);
  POFFSET(revision);
  POFFSET(progIF);
  POFFSET(subClassCode);
  POFFSET(classCode);
  POFFSET(cacheLineSize);
  POFFSET(latencyTimer);
  POFFSET(headerType);
  POFFSET(bist);
  POFFSET(pci0.baseAddr0);
  POFFSET(pci0.baseAddr1);
  POFFSET(pci0.baseAddr2);
  POFFSET(pci0.baseAddr3);
  POFFSET(pci0.baseAddr4);
  POFFSET(pci0.baseAddr5);
  POFFSET(pci0.cardbusCIS);
  POFFSET(pci0.subsystemVendorID);
  POFFSET(pci0.expansionROM);
  POFFSET(pci0.reserved0);
  POFFSET(pci0.reserved1);
  POFFSET(pci0.interruptLine);
  POFFSET(pci0.interruptPin);
  POFFSET(pci0.minimumGrant);
  POFFSET(pci0.minimumLatency);

  return 0;
}
