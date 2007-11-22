/*
 * Copyright (c) 2000, 2001, 2003, 2004, 2005
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

#ifndef __DBL_LIST_HH__
#define __DBL_LIST_HH__

class DblListEl {
    DblListEl *next;
    DblListEl *prev;

    // remove this from list
    void remove() {
	prev->next = next;
	next->prev = prev;
    }

    // insert this before old_el
    void insertBefore(DblListEl *old_el) {
	prev = old_el->prev;
	next = old_el;
	prev->next = this;
	next->prev = this;
    }

    // insert this after old_el
    void insertAfter(DblListEl *old_el) {
	next = old_el->next;
	prev = old_el;
	next->prev = this;
	prev->next = this;
    }

    friend class DblListBase;
};


//
// doubly-linked list of DblListEl objects
//
class DblListBase {
    // dummy list head element: dummy.next is head, dummy.prev is tail
    DblListEl dummy;

    // length counter
    unsigned length;

    DblListEl *valid_or_null(DblListEl *el) {
	// make sure users never see the dummy element
	return (el == &dummy) ? NULL : el;
    }

  public:

    DblListEl *head() {
	return valid_or_null(dummy.next);
    }

    DblListEl *tail() {
	return valid_or_null(dummy.prev);
    }

    DblListEl *next(DblListEl *el) {
	return valid_or_null(el->next);
    }

    DblListEl *prev(DblListEl *el) {
	return valid_or_null(el->prev);
    }

    bool is_empty() {
	return (dummy.next == &dummy);
    }

    void remove(DblListEl *el) {
	el->remove();
	--length;
    }

    void insertBefore(DblListEl *new_el, DblListEl *old_el) {
	new_el->insertBefore(old_el);
	++length;
    }

    void insertAfter(DblListEl *new_el, DblListEl *old_el) {
	new_el->insertAfter(old_el);
	++length;
    }

    // append to end of list, i.e. as dummy.prev
    void append(DblListEl *el) {
	insertBefore(el, &dummy);
    }

    // prepend to front of list (push), i.e. as dummy.next
    void prepend(DblListEl *el) {
	insertAfter(el, &dummy);
    }

    DblListEl *pop() {
	DblListEl *hd = head();
	if (hd != NULL)
	    remove(hd);
	return hd;
    }

    // constructor
    DblListBase() {
	dummy.next = dummy.prev = &dummy;
	length = 0;
    }
};


//
// Template class serves solely to cast args & return values
// to appropriate type (T *)
//
template<class T> class DblList : private DblListBase {

  public:

    T *head() { return (T *)DblListBase::head(); }
    T *tail() { return (T *)DblListBase::tail(); }

    T *next(T *el) { return (T *)DblListBase::next(el); }
    T *prev(T *el) { return (T *)DblListBase::prev(el); }

    bool is_empty() { return DblListBase::is_empty(); }

    void remove(T *el) { DblListBase::remove(el); }

    void append(T *el) { DblListBase::append(el); }
    void prepend(T *el) { DblListBase::prepend(el); }

    T *pop() { return (T *)DblListBase::pop(); }

    DblList<T>() { }
};

#endif // __DBL_LIST_HH__
