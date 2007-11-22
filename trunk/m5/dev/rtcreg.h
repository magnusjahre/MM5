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

#define RTC_SEC                 0x00
#define RTC_SEC_ALRM            0x01
#define RTC_MIN                 0x02
#define RTC_MIN_ALRM            0x03
#define RTC_HR                  0x04
#define RTC_HR_ALRM             0x05
#define RTC_DOW                 0x06
#define RTC_DOM                 0x07
#define RTC_MON                 0x08
#define RTC_YEAR                0x09

#define RTC_STAT_REGA           0x0A
#define  RTCA_1024HZ            0x06  /* 1024Hz periodic interrupt frequency */
#define  RTCA_32768HZ           0x20  /* 22-stage divider, 32.768KHz timebase */
#define  RTCA_UIP               0x80  /* 1 = date and time update in progress */

#define RTC_STAT_REGB           0x0B
#define  RTCB_DST               0x01  /* USA Daylight Savings Time enable */
#define  RTCB_24HR              0x02  /* 0 = 12 hours, 1 = 24 hours */
#define  RTCB_BIN               0x04  /* 0 = BCD, 1 = Binary coded time */
#define  RTCB_SQWE              0x08  /* 1 = output sqare wave at SQW pin */
#define  RTCB_UPDT_IE           0x10  /* 1 = enable update-ended interrupt */
#define  RTCB_ALRM_IE           0x20  /* 1 = enable alarm interrupt */
#define  RTCB_PRDC_IE           0x40  /* 1 = enable periodic clock interrupt */
#define  RTCB_NO_UPDT           0x80  /* stop clock updates */

#define RTC_STAT_REGC           0x0C
#define RTC_STAT_REGD           0x0D

