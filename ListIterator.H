
#ifndef BL_LIST_H
#define BL_LIST_H

//
// $Id: ListIterator.H,v 1.2 2001-11-16 01:48:08 vince Exp $
//

#include <BLassert.H>
#include <BoxLib.H>

#ifdef BL_NAMESPACE
namespace BL_NAMESPACE
{
#endif

template <class T> class ListLink;
template <class T> class ListIterator;
template <class T> class List;

template <class T>
class ListLink
{
private:
    friend class List<T>;
    friend class ListIterator<T>;

    ListLink (const T&     _val,
              ListLink<T>* _pre,
              ListLink<T>* _suc)
        :
        val(_val),
        pre(_pre),
        suc(_suc) {}

private:
    T            val;
    ListLink<T>* pre;
    ListLink<T>* suc;
};

template <class T>
class ListIterator
{
public:
    explicit ListIterator (const List<T>& aList);
    ListIterator (const ListIterator<T>& rhs);
    void rewind ();
    const T& operator() () const;
    const T& operator* () const;
    operator void* ();
    bool operator! () const;
    const T& value () const;
    ListIterator<T>& operator++ ();
    ListIterator<T>& operator-- ();
    ListIterator<T> operator-- (int);
    ListIterator<T> operator++ (int);
    bool operator== (const ListIterator<T>&) const;
    bool operator!= (const ListIterator<T>&) const;

protected:
    ListIterator (const List<T>& _list, ListLink<T>*   _p);
    const List<T>& list;
    ListLink<T>* p;

private:
    friend class List<T>;
    ListIterator ();
    ListIterator<T>& operator= (const ListIterator<T>&);
};

template <class T>
class List
{
public:
    List ();
    List (const List<T>& rhs);
    List<T>& operator= (const List<T>& rhs);
    ~List();
    void prepend (const T& value);
    void append (const T& value);
    void add (const T& value);
    void join (const List<T>& src);
    void catenate (List<T>& src);
    void clear ();
    T& firstElement () const;
    T& lastElement () const;
    bool includes (const T& value) const;
    bool operator== (const List<T>& rhs) const;
    bool operator!= (const List<T>& rhs) const;
    bool isEmpty () const;
    bool isNotEmpty () const;
    int length () const;
    void removeFirst ();
    void removeLast ();
    const T& operator[] (const ListIterator<T>& li) const;
    T& operator[] (const ListIterator<T>& li);
    void remove (const T& value);
    void remove (const List<T>& lst);
    void remove (ListIterator<T>& lit);
    void replace (ListIterator<T>& li, const T&         val);
    void addAfter (ListIterator<T>& lit, const T&         val);
    void addBefore (ListIterator<T>& lit, const T&         val);
    ListIterator<T> first () const;
    ListIterator<T> last () const;

protected:
    void remove (ListLink<T> *ln);
    ListLink<T>* addBefore (ListLink<T>* ln, const T&     val);
    ListLink<T>* addAfter (ListLink<T>* ln, const T&     val);
    ListLink<T>* head;
    ListLink<T>* tail;
    friend class ListIterator<T>;
};

template <class T>
inline
ListIterator<T>::ListIterator (const List<T>& _list,
                               ListLink<T>*   _p)
    :
    list(_list),
    p(_p)
{}

template <class T>
inline
ListIterator<T>::ListIterator (const List<T>& aList)
    :
    list(aList)
{
    p = list.head;
}

template <class T>
inline
ListIterator<T>::ListIterator (const ListIterator<T>& li)
    :
    list(li.list),
    p(li.p)
{}

template <class T>
inline
void
ListIterator<T>::rewind ()
{
    p = list.head;
}

template <class T>
inline
const T&
ListIterator<T>::operator() () const
{
    BL_ASSERT(p != 0);
    return p->val;
}

template <class T>
inline
const T&
ListIterator<T>::operator* () const
{
    BL_ASSERT(p != 0);
    return p->val;
}

template <class T>
inline
ListIterator<T>::operator void* ()
{
    return p != 0 ? this : 0;
}

template <class T>
inline
bool
ListIterator<T>::operator! () const
{
    return p == 0 ? true : false;
}

template <class T>
inline
const T&
ListIterator<T>::value () const
{
    BL_ASSERT(p != 0);
    return p->val;
}

template <class T>
inline
ListIterator<T>&
ListIterator<T>::operator++ ()
{
    if (p)
        p = p->suc;
    return *this;
}

template <class T>
inline
ListIterator<T>&
ListIterator<T>::operator-- ()
{
    if (p)
        p = p->pre;
    return *this;
}

template <class T>
inline
ListIterator<T>
ListIterator<T>::operator++ (int)
{
    const ListIterator<T> li = *this;
    ++(*this);
    return li;
}

template <class T>
inline
ListIterator<T>
ListIterator<T>::operator-- (int)
{
    const ListIterator<T> li = *this;
    --(*this);
    return li;
}

template <class T>
inline
bool
ListIterator<T>::operator== (const ListIterator<T>& _li) const
{
    return (&list == &_li.list && p == _li.p) ? true : false;
}

template <class T>
inline
bool
ListIterator<T>::operator!= (const ListIterator<T>& _li) const
{
    return ! ListIterator<T>::operator==(_li);
}

template <class T>
inline
List<T>::List ()
    :
    head(0),
    tail(0)
{}

template <class T>
inline
List<T>::~List ()
{
    clear();
}

template <class T>
inline
void
List<T>::prepend (const T& value)
{
    addBefore(head, value);
}

template <class T>
inline
void
List<T>::append (const T& value)
{
    addAfter(tail, value);
}

template <class T>
inline
T&
List<T>::firstElement () const
{
    BL_ASSERT(head != 0);
    return head->val;
}

template <class T>
inline
T&
List<T>::lastElement () const
{
    BL_ASSERT(tail != 0);
    return tail->val;
}

template <class T>
inline
bool
List<T>::isEmpty () const
{
    return head == 0 && tail == 0;
}

template <class T>
inline
bool
List<T>::isNotEmpty () const
{
    return !isEmpty();
}

template <class T>
inline
void
List<T>::removeFirst ()
{
    remove(head);
}

template <class T>
inline
void
List<T>::removeLast ()
{
    remove(tail);
}

template <class T>
inline
const T&
List<T>::operator[] (const ListIterator<T>& li) const
{
    BL_ASSERT(li.p != 0);
    return li.p->val;
}

template <class T>
inline
T&
List<T>::operator[] (const ListIterator<T>& li)
{
    BL_ASSERT(li.p != 0);
    return li.p->val;
}

template <class T>
inline
void
List<T>::replace (ListIterator<T>& li,
                  const T&         _val)
{
    BL_ASSERT(li.p != 0);
    li.p->val = _val;
}

template <class T>
inline
void
List<T>::addAfter (ListIterator<T>& lit,
                   const T&         val)
{
    addAfter(lit.p, val);
}

template <class T>
inline
void
List<T>::addBefore (ListIterator<T>& lit,
                    const T&         val)
{
    addBefore(lit.p, val);
}

template <class T>
inline
ListIterator<T>
List<T>::first () const
{
    return ListIterator<T>(*this,head);
}

template <class T>
inline
ListIterator<T>
List<T>::last () const
{
    return ListIterator<T>(*this,tail);
}

template <class T>
List<T>::List (const List<T>& source)
    :
    head(0),
    tail(0)
{
    if (source.isEmpty())
        tail = head = 0;
    else
        for (ListIterator<T> li(source); li; ++li)
            append(li());
}


template <class T>
void
List<T>::add (const T& value)
{
    append(value);
}

template <class T>
int
List<T>::length () const
{
    int len = 0;
    for (ListIterator<T> li(*this); li; ++li)
        len++;
    return len;
}

template <class T>
List<T>&
List<T>::operator= (const List<T>& source)
{
    if (!(this == &source))
    {
        clear();
        for (ListIterator<T> li(source); li; ++li)
            append(li());
    }
    return *this;
}

template <class T>
ListLink<T> *
List<T>::addBefore (ListLink<T>* ln,
                    const T&     val)
{
    BL_ASSERT(ln != 0 || head == 0);

    ListLink<T>* newlink;

    if (ln == head)
    {
        head = newlink = new ListLink<T>(val, 0, head);

        if (tail == 0)
            tail = head;
        else
            head->suc->pre = newlink;
    }
    else
    {
        newlink = new ListLink<T>(val, ln->pre, ln);

        ln->pre->suc = newlink;
        ln->pre = newlink;
    }

    return newlink;
}

template <class T>
ListLink<T>*
List<T>::addAfter (ListLink<T>* ln,
                   const T&     val)
{
    BL_ASSERT(ln != 0 || tail == 0);

    ListLink<T>* newlink;

    if (ln == tail)
    {
        tail = newlink = new ListLink<T>(val,tail,0);

        if (head == 0)
            head = tail;
        else
            tail->pre->suc = newlink;
    }
    else
    {
        newlink = new ListLink<T>(val, ln, ln->suc);

        ln->suc->pre = newlink;
        ln->suc = newlink;
    }

    return newlink;
}

template <class T>
void
List<T>::join (const List<T>& list2)
{
    for (ListIterator<T> li2(list2); li2; ++li2)
        append(li2());
}

template <class T>
void
List<T>::catenate (List<T>& list2)
{
    if (list2.isEmpty())
        //
        // Do nothing.
        //
        ;
    else if (isEmpty())
    {
       head = list2.head;
       tail = list2.tail;
       list2.head = 0;
       list2.tail = 0;
    }
    else
    {
        tail->suc = list2.head;
        list2.head->pre = tail;
        tail = list2.tail;
        list2.head = 0;
        list2.tail = 0;
    }
}

template <class T>
void
List<T>::clear ()
{
    ListLink<T>* next = 0;

    for (ListLink<T>* p = head; p != 0; p = next)
    {
        next = p->suc;
        p->suc = 0;
        delete p;
    }
    tail = head = 0;
}

template <class T>
bool
List<T>::includes (const T& v) const
{
    bool rc = false;
    for (ListIterator<T> li(*this); li && !rc; ++li)
        if (v == li())
            rc = true;
    return rc;
}

template<class T>
bool
List<T>::operator== (const List<T>& rhs) const
{
    if (length() == rhs.length())
    {
        for (ListIterator<T> li(*this), ri(rhs); li; ++li, ++ri)
            if (li() != ri())
                return false;
        return true;
    }

    return false;
}

template<class T>
bool
List<T>::operator!= (const List<T>& rhs) const
{
    return !operator==(rhs);
}

template <class T>
void
List<T>::remove (ListIterator<T>& li)
{
    ListLink<T> *np = li.p->suc;
    remove(li.p);
    li.p = np;
}

template <class T>
void
List<T>::remove (const T& _v)
{
    for (ListIterator<T> litr(*this); litr; ++litr)
        if (litr() == _v)
            remove(litr);
}

template <class T>
void
List<T>::remove (const List<T>& _lv)
{
    for (ListIterator<T> litr(_lv); litr; ++litr)
        remove(litr());
}

template <class T>
void
List<T>::remove (ListLink<T>* ln)
{
    BL_ASSERT(head !=0 && tail != 0);

    if (head == ln && tail == ln)
        head = tail = 0;
    else if (head == ln)
    {
        BL_ASSERT(ln->pre == 0);
        head = ln->suc;
        head->pre = 0;
    }
    else if (tail == ln)
    {
        BL_ASSERT(ln->suc == 0);
        tail = ln->pre;
        tail->suc  = 0;
    }
    else
    {
        BL_ASSERT(ln->suc != 0 && ln->pre != 0);
        ln->suc->pre = ln->pre;
        ln->pre->suc = ln->suc;
    }
    delete ln;
    ln = 0;
}

#ifdef BL_NAMESPACE
}
#ifndef BL_NO_USING_DIRECTIVE
using namespace BL_NAMESPACE;
#endif
#endif

#endif /*BL_LIST_H*/
