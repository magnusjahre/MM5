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



#include <iostream>
#include <string>
#include <vector>

#include "base/str.hh"

int
main(int argc, char *argv[])
{
    using namespace std;

    if (argc != 3) {
        cout << "Usage: " << argv[0] << " <string> <token>\n";
        exit(1);
    }

    int i;
    string test = argv[1];
    vector<string> tokens1;
    vector<string> tokens2;
    char token = argv[2][0];

    cout << "string = \"" << test << "\", token = \'" << token << "\'\n";
    cout << "testing without ignore\n";
    tokenize(tokens1, test, token, false);

    if (tokens1.size()) {
        int size = tokens1.size();
        cout << "size = " << size << "\n";
        for (i = 0; i < size; i++) {
            cout << "'" << tokens1[i] << "' (" << tokens1[i].size()
                 << ")" << ((i == size - 1) ? "\n" : ", ");
        }        
    } else {
        cout << "no tokens" << endl;
    }

    cout << "testing with ignore\n";
    tokenize(tokens2, test, token, true);

    if (tokens2.size()) {
        int size = tokens2.size();
        cout << "size = " << size << "\n";
        for (i = 0; i < size; i++) {
            cout << "'" << tokens2[i] << "' (" << tokens2[i].size()
                 << ")" << ((i == size - 1) ? "\n" : ", ");
        }        
    } else {
        cout << "no tokens" << endl;
    }

    return 0;
}
