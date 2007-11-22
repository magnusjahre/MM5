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

#ifndef __CALLBACK_HH__
#define __CALLBACK_HH__

#include <list>

/**
 * Generic callback class.  This base class provides a virtual process
 * function that gets called when the callback queue is processed.
 */
class Callback
{
  public:
    /**
     * virtualize the destructor to make sure that the correct one
     * gets called.
     */
    virtual ~Callback() {}

    /**
     * virtual process function that is invoked when the callback
     * queue is executed.
     */
    virtual void process() = 0;
};

class CallbackQueue
{
  protected:
    /**
     * Simple typedef for the data structure that stores all of the
     * callbacks.
     */
    typedef std::list<Callback *> queue;

    /**
     * List of all callbacks.  To be called in fifo order.
     */
    queue callbacks;

  public:
    /**
     * Add a callback to the end of the queue
     * @param callback the callback to be added to the queue
     */
    void add(Callback *callback)
    {
	callbacks.push_back(callback);
    }

    /**
     * Find out if there are any callbacks in the queue
     */
    bool empty() const { return callbacks.empty(); }

    /**
     * process all callbacks
     */
    void process()
    {
	queue::iterator i = callbacks.begin();
	queue::iterator end = callbacks.end();

	while (i != end) {
	    (*i)->process();
	    ++i;
	}
    }

    /**
     * clear the callback queue
     */
    void clear()
    {
	callbacks.clear();
    }
};

/// Helper template class to turn a simple class member function into
/// a callback.
template <class T, void (T::* F)()>
class MakeCallback : public Callback
{
  private:
    T *object;

  public:
    MakeCallback(T *o)
	: object(o)
    { }

    void process() { (object->*F)(); }
};

#endif // __CALLBACK_HH__
