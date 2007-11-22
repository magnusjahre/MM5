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


#include <iostream>
#include "bhgp.hh"

int main(void)
{
  typedef AssociativeTable<unsigned int, unsigned int> tableType;
  tableType table(10, 4);  // 40 entry table

  std::cout << "Initial state:" << std::endl;
  table.dump();

  std::cout << "Inserting (2, 1)" << std::endl;
  table[2] = 1;
  table.dump();

  std::cout << "Inserting (5, 2)" << std::endl;
  table[5] = 2;
  table.dump();

  std::cout << "Inserting (10 + 2, 3)" << std::endl;
  table[10 + 2] = 3;
  table.dump();

  tableType::const_iterator i = table.find(2);
  assert(i != table.end());
  std::cout << "Accessed 2: " << *i << std::endl;
  table.dump();

  i = table.find(10 + 2);
  assert(i != table.end());
  std::cout << "Accessed 10 + 2: " << *i << std::endl;
  table.dump();

  i = table.find(34);
  assert(i == table.end());

  std::cout << "Inserting (2 * 10 + 2, 4)" << std::endl;
  table[2 * 10 + 2] = 4;
  table.dump();

  std::cout << "Replacing (10 + 2) with 5" << std::endl;
  table[10 + 2] = 5;
  table.dump();

  std::cout << "Inserting (3 * 10 + 2, 6)" << std::endl;
  table[3 * 10 + 2] = 6;
  table.dump();

  std::cout << "Inserting (4 * 10 + 2, 7)" << std::endl;
  table[4 * 10 + 2] = 7;
  table.dump();

  return(0);
}
