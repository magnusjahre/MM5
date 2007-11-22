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

/** @file
 * Base class for UART
 */

#ifndef __UART_HH__
#define __UART_HH__

#include "base/range.hh"
#include "dev/io_device.hh"

class SimConsole;
class Platform;

const int RX_INT = 0x1;
const int TX_INT = 0x2;


class Uart : public PioDevice 
{

  protected:
    int status;
    Addr addr;
    Addr size;
    SimConsole *cons;
    
  public:
    Uart(const std::string &name, SimConsole *c, MemoryController *mmu, 
	 Addr a, Addr s, HierParams *hier, Bus *bus, Tick pio_latency,
	 Platform *p);

    virtual Fault read(MemReqPtr &req, uint8_t *data) = 0;
    virtual Fault write(MemReqPtr &req, const uint8_t *data) = 0;


    /**
     * Inform the uart that there is data available.
     */
    virtual void dataAvailable() = 0;
        
   
    /**
     * Return if we have an interrupt pending
     * @return interrupt status
     */
    bool intStatus() { return status ? true : false; }
    
    /**
     * Return how long this access will take.
     * @param req the memory request to calcuate
     * @return Tick when the request is done
     */
    Tick cacheAccess(MemReqPtr &req);
};

#endif // __UART_HH__
