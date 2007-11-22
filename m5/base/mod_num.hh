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

template<class T, T MV>
class ModNum {
  private:
    T value;

    //  Compiler should optimize this
    void setValue(T n) { value = n % MV; }

  public:
    ModNum() {}
    ModNum(T n) { setValue(n); }
    ModNum(const ModNum<T, MV> &n) : value(n.value) {}

    ModNum operator=(T n) {
	setValue(n);
	return *this;
    }

    const ModNum operator=(ModNum n) {
	value = n.value;
	return *this;
    }

    //  Return the value if object used as RHS
    operator T() const { return value; }

    //
    //  Operator "+="
    //
    const ModNum<T, MV> operator+=(ModNum<T, MV> r) {
	setValue(value + r.value);
	return *this;
    }

    const ModNum<T, MV> operator+=(T r) {
	setValue(value + r);
	return *this;
    }

    //
    //  Operator "-="
    //
    const ModNum<T, MV> operator-=(ModNum<T, MV> r) {
	setValue(value - r.value);
	return *this;
    }

    const ModNum<T, MV> operator-=(T r) {
	setValue(value - r);
	return *this;
    }

    //
    //  Operator "++"
    //
    //  PREFIX (like ++a)
    const ModNum<T, MV> operator++() {
	*this += 1;
	return *this;
    }

    //  POSTFIX (like a++)
    const ModNum<T, MV> operator++(int) {
	ModNum<T, MV> rv = *this;

	*this += 1;

	return rv;
    }

    //
    //  Operator "--"
    //
    //  PREFIX (like --a)
    const ModNum<T, MV> operator--() {
	*this -= 1;
	return *this;
    }

    //  POSTFIX (like a--)
    const ModNum<T, MV> operator--(int) {
	ModNum<T, MV> rv = *this;
	*this -= 1;
	return rv;
    }
};


//
//  Define operator "+" like this to avoid creating a temporary
//
template<class T, T MV>
inline ModNum<T, MV>
operator+(ModNum<T, MV> l, ModNum<T, MV> r) {
    l += r;
    return l;
}

template<class T, T MV>
inline ModNum<T, MV>
operator+(ModNum<T, MV> l, T r) {
    l += r;
    return l;
}

template<class T, T MV>
inline ModNum<T, MV>
operator+(T l, ModNum<T, MV> r) {
    r += l;
    return r;
}


//
//  Define operator "-" like this to avoid creating a temporary
//
template<class T, T MV>
inline ModNum<T, MV>
operator-(ModNum<T, MV> l, ModNum<T, MV> r) {
    l -= r;
    return l;
}

template<class T, T MV>
inline ModNum<T, MV>
operator-(ModNum<T, MV> l, T r) {
    l -= r;
    return l;
}

template<class T, T MV>
inline ModNum<T, MV>
operator-(T l, ModNum<T, MV> r) {
    r -= l;
    return r;
}


//
//  Comparison operators
//  (all other cases are handled with conversons)
//
template<class T, T MV>
inline bool
operator<(ModNum<T, MV> l, ModNum<T, MV> r) {
    return l.value < r.value;
}

template<class T, T MV>
inline bool
operator>(ModNum<T, MV> l, ModNum<T, MV> r) {
    return l.value > r.value;
}

template<class T, T MV>
inline bool
operator==(ModNum<T, MV> l, ModNum<T, MV> r) {
    return l.value == r.value;
}

template<class T, T MV>
inline bool
operator<=(ModNum<T, MV> l, ModNum<T, MV> r) {
    return l.value <= r.value;
}

template<class T, T MV>
inline bool
operator>=(ModNum<T, MV> l, ModNum<T, MV> r) {
    return l.value >= r.value;
}


