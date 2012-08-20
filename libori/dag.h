/*
 * Copyright (c) 2012 Stanford University
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR(S) DISCLAIM ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL AUTHORS BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef __DAG_H__
#define __DAG_H__

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#include <iostream>
#include <string>
#include <map>
#include <tr1/unordered_set>
#include <exception>

template <class _Key, class _Val>
class DAG;

template <class _Key, class _Val>
class DAGNode
{
public:
    DAGNode()
    {
    }
    DAGNode(_Key k, _Val v)
	: k(k), v(v)
    {
    }
    ~DAGNode()
    {
    }
    _Val getValue()
    {
	return v;
    }
    std::tr1::unordered_set<_Key> listParents()
    {
	return parents;
    }
    std::tr1::unordered_set<_Key> listChildren()
    {
	return children;
    }
    void dump()
    {
	typename std::tr1::unordered_set<_Key>::iterator it;
	for (it = parents.begin(); it != parents.end(); it++) {
	    std::cout << (*it).hex() << " <- " << k.hex() << std::endl;
	}
    }
private:
    template<class, class> friend class DAG;
    friend class DAG <_Key, _Val>;
    _Key k;
    _Val v;
    typename std::tr1::unordered_set<_Key> parents;
    typename std::tr1::unordered_set<_Key> children;
};

template <class _Key, class _Val>
class DAG
{
public:
    DAG()
    {
    }
    ~DAG()
    {
    }
    void addNode(_Key k, _Val v)
    {
	nodeMap[k] = DAGNode<_Key, _Val>(k, v);
    }
    _Val getNode(_Key k)
    {
	typename std::map<_Key, DAGNode<_Key, _Val> >::iterator it = nodeMap.find(k);

	if (it == nodeMap.end()) {
	    assert(false);
	}

	return (*it).second.getValue();
    }
    void addChild(_Key parent, _Key child)
    {
	typename std::map<_Key, DAGNode<_Key, _Val> >::iterator p = nodeMap.find(parent);
	typename std::map<_Key, DAGNode<_Key, _Val> >::iterator c = nodeMap.find(child);

	if (p == nodeMap.end() || c == nodeMap.end()) {
	    assert(false);
	}

	(*c).second.parents.insert(parent);
	(*p).second.children.insert(child);
    }
    _Key findLCA(_Key p1, _Key p2)
    {
	typename std::map<_Key, DAGNode<_Key, _Val> >::iterator it1;
	typename std::map<_Key, DAGNode<_Key, _Val> >::iterator it2;
	typename std::tr1::unordered_set<_Key> p1p, p2p;
	typename std::tr1::unordered_set<_Key> new_p1p, new_p2p;
	typename std::tr1::unordered_set<_Key> next_p1p, next_p2p;
	typename std::tr1::unordered_set<_Key>::iterator it;

	/*
	 * Step 1: Add next set of nodes in p1 & p2 to unordered_set and paths to
	 * queue.
	 * Step 2: Check if any of these nodes are in the the existing sets.
	 *  - Match: Return path and matching key
	 *  - Else: Repeat 1
	 */

	it1 = nodeMap.find(p1);
	it2 = nodeMap.find(p2);
	if (it1 == nodeMap.end() || it2 == nodeMap.end())
	    throw std::exception();

	for (it = it1->second.parents.begin();
	     it != it1->second.parents.end();
	     it++)
	{
	    new_p1p.insert(*it);
	}
	for (it = it2->second.parents.begin();
	     it != it2->second.parents.end();
	     it++)
	{
	    new_p2p.insert(*it);
	}

	// XXX: Check if one is the ancestor of another

	while (1) {
	    /*
	     * Check if any of the new keys exist in the set of already walked 
	     * nodes stored in p1p and p2p.
	     */
	    for (it = new_p1p.begin(); it != new_p1p.end(); it++) {
		typename std::tr1::unordered_set<_Key>::iterator k;
		k = p2p.find(*it);
		if (k != p2p.end()) {
		    return *k;
		}
	    }
	    for (it = new_p1p.begin(); it != new_p1p.end(); it++) {
		p1p.insert(*it);
	    }
	    for (it = new_p2p.begin(); it != new_p2p.end(); it++) {
		typename std::tr1::unordered_set<_Key>::iterator k;
		k = p1p.find(*it);
		if (k != p1p.end()) {
		    return *k;
		}
	    }
	    for (it = new_p2p.begin(); it != new_p2p.end(); it++) {
		p2p.insert(*it);
	    }

	    /*
	     * Push the next level of parents.
	     */
	    for (it = new_p1p.begin(); it != new_p1p.end(); it++) {
		it1 = nodeMap.find(*it);
		if (it1 != nodeMap.end()) {
		    typename std::tr1::unordered_set<_Key>::iterator k;
		    for (k = it1->second.parents.begin();
			 k != it1->second.parents.end();
			 k++)
		    {
			next_p1p.insert(*k);
		    }
		}
	    }
	    for (it = new_p2p.begin(); it != new_p2p.end(); it++) {
		it2 = nodeMap.find(*it);
		if (it2 != nodeMap.end()) {
		    typename std::tr1::unordered_set<_Key>::iterator k;
		    for (k = it2->second.parents.begin();
			 k != it2->second.parents.end();
			 k++)
		    {
			next_p2p.insert(*k);
		    }
		}
	    }

	    /*
	     * Update
	     */
	    if (next_p1p.size() == 0 && next_p2p.size() == 0)
		throw std::exception();

	    new_p1p = next_p1p;
	    new_p2p = next_p2p;
	    next_p1p.clear();
	    next_p2p.clear();
	}
    }
    void dump() {
	typename std::map<_Key, DAGNode<_Key, _Val> >::iterator it;
	for (it = nodeMap.begin(); it != nodeMap.end(); it++)
	{
	    it->second.dump();
	}
    }
private:
    typename std::map<_Key, DAGNode<_Key, _Val> > nodeMap;
};

#endif /* __DAG_H__ */

