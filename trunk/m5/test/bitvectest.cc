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


#include <iostream.h>

#include <vector>

int
main()
{
  vector<bool> v1(100);

  v1[0] = true;
  v1.resize(500);
  v1[100] = true;
  v1[499] = true;
  v1.resize(10000);
  v1[9999] = true;

  cout << "v1.size() = " << v1.size() << "\n";
  for (int i = 0; i < v1.size(); i++)
    if (v1[i])
      cout << "v1[" << i << "] = " << v1[i] << "\n";

  cout << "\n";

  vector<bool> v2 = v1;

  for (int i = 0; i < v2.size(); i++)
    if (v2[i])
      cout << "v2[" << i << "] = " << v2[i] << "\n";

  cout << "v1 " << ((v1 == v2) ? "==" : "!=") << " v2" << "\n";
  v2[8583] = true;
  cout << "v1 " << ((v1 == v2) ? "==" : "!=") << " v2" << "\n";
  v1[8583] = true;
  cout << "v1 " << ((v1 == v2) ? "==" : "!=") << " v2" << "\n";
  v1.resize(100000);
  cout << "v1 " << ((v1 == v2) ? "==" : "!=") << " v2" << "\n";
  cout << flush;
}
