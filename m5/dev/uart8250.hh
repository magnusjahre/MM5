/*
 * Copyright (c) 2005
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
 * Defines a 8250 UART
 */

#ifndef __TSUNAMI_UART_HH__
#define __TSUNAMI_UART_HH__

#include "dev/tsunamireg.h"
#include "base/range.hh"
#include "dev/io_device.hh"
#include "dev/uart.hh"


/* UART8250 Interrupt ID Register
 *  bit 0    Interrupt Pending 0 = true, 1 = false
 *  bit 2:1  ID of highest priority interrupt
 *  bit 7:3  zeroes
 */
#define IIR_NOPEND 0x1

// Interrupt IDs
#define IIR_MODEM 0x00 /* Modem Status (lowest priority) */
#define IIR_TXID  0x02 /* Tx Data */
#define IIR_RXID  0x04 /* Rx Data */
#define IIR_LINE  0x06 /* Rx Line Status (highest priority)*/

class SimConsole;
class Platform;

class Uart8250 : public Uart 
{


  protected:
    uint8_t IER, DLAB, LCR, MCR;

    class IntrEvent : public Event
    {
        protected:
            Uart8250 *uart;
            int intrBit;
        public:
            IntrEvent(Uart8250 *u, int bit);
            virtual void process();
            virtual const char *description();
            void scheduleIntr();
    };
    
    IntrEvent txIntrEvent;
    IntrEvent rxIntrEvent;
    
  public:
    Uart8250(const std::string &name, SimConsole *c, MemoryController *mmu, 
	 Addr a, Addr s, HierParams *hier, Bus *bus, Tick pio_latency,
	 Platform *p);

    virtual Fault read(MemReqPtr &req, uint8_t *data);
    virtual Fault write(MemReqPtr &req, const uint8_t *data);


    /**
     * Inform the uart that there is data available.
     */
    virtual void dataAvailable();
        
   
    /**
     * Return if we have an interrupt pending
     * @return interrupt status
     */
    virtual bool intStatus() { return status ? true : false; }
    
    virtual void serialize(std::ostream &os);
    virtual void unserialize(Checkpoint *cp, const std::string &section);

};

#endif // __TSUNAMI_UART_HH__
