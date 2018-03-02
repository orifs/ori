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
#include <stdint.h>

#include <iostream>
#include <string>
#include <map>
#include <list>
#include <exception>
#include <unordered_set>

#include "debug.h"

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
    _Val& getValue()
    {
	return v;
    }
    std::unordered_set<_Key> listParents()
    {
	return parents;
    }
    std::unordered_set<_Key> listChildren()
    {
	return children;
    }
    void dump()
    {
	typename std::unordered_set<_Key>::iterator it;
	for (it = parents.begin(); it != parents.end(); it++) {
	    std::cout << (*it).hex() << " <- " << k.hex() << std::endl;
	}
    }
private:
    template<class, class> friend class DAG;
    friend class DAG <_Key, _Val>;
    _Key k;
    _Val v;
    typename std::unordered_set<_Key> parents;
    typename std::unordered_set<_Key> children;
};

template <class _Key, class _Val_Old, class _Val_New>
class DAGMapCB
{
public:
    virtual _Val_New map(_Key k, _Val_Old v) = 0;
};

template <class _Key, class _Val>
class DAG;

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
    /*
     * Fill this DAG in from another to compute a one to one mapping.
     */
    template <class _Val_Old>
    void graphMap(DAGMapCB<_Key, _Val_Old, _Val> &m, DAG<_Key, _Val_Old> d)
    {
	typename std::map<_Key, DAGNode<_Key, _Val_Old> >::iterator it;

	for (it = d.nodeMap.begin(); it != d.nodeMap.end(); it++)
	{
	    _Key k = (*it).first;
	    addNode(k, m.map(k, (*it).second.getValue()));
	    nodeMap[k].parents = (*it).second.parents;
	    nodeMap[k].children = (*it).second.children;
	}
    }
    /*
     * Add a graph node
     */
    void addNode(_Key k, _Val v)
    {
	nodeMap[k] = DAGNode<_Key, _Val>(k, v);
    }
    /*
     * Delete a graph node
     */
    void delNode(_Key k)
    {
	typename std::map<_Key, DAGNode<_Key, _Val> >::iterator n = nodeMap.find(k);

	ASSERT(n != nodeMap.end());

	std::unordered_set<_Key> parents = (*n).listParents();
	std::unordered_set<_Key> children = (*n).listChildren();
	typename std::unordered_set<_Key>::iterator i;

	// Break edges to parents
	for (i = parents.begin(); i != parents.end(); i++)
	{
	    typename std::map<_Key, DAGNode<_Key, _Val> >::iterator p;
	    p = nodeMap.find(*i);
	    p.children.erase(k);
	}

	// Break edges to children
	for (i = children.begin(); i != children.end(); i++)
	{
	    typename std::map<_Key, DAGNode<_Key, _Val> >::iterator c;
	    c = nodeMap.find(*i);
	    c.parents.erase(k);
	}
    }
    /*
     * Prune node, attempts to remove a graph node and update the edges, with 
     * the constraint that a node may have at most two parents.
     */
    void pruneNode(_Key k)
    {
	typename std::map<_Key, DAGNode<_Key, _Val> >::iterator n = nodeMap.find(k);

	ASSERT(n != nodeMap.end());

	std::unordered_set<_Key> children = (*n).listChildren();
	typename std::unordered_set<_Key>::iterator i;

	NOT_IMPLEMENTED(false);
    }
    /*
     * Get a graph node
     */
    _Val& getNode(_Key k)
    {
	typename std::map<_Key, DAGNode<_Key, _Val> >::iterator it = nodeMap.find(k);

	if (it == nodeMap.end()) {
	    assert(false);
	}

	return (*it).second.getValue();
    }
    /*
     * Get a node's parents
     */
    std::unordered_set<_Key> getParents(_Key k)
    {
	typename std::map<_Key, DAGNode<_Key, _Val> >::iterator it = nodeMap.find(k);

	if (it == nodeMap.end()) {
	    assert(false);
	}

	return (*it).second.listParents();
    }
    /*
     * Add a graph edge
     */
    void addEdge(_Key parent, _Key child)
    {
	typename std::map<_Key, DAGNode<_Key, _Val> >::iterator p = nodeMap.find(parent);
	typename std::map<_Key, DAGNode<_Key, _Val> >::iterator c = nodeMap.find(child);

	ASSERT(p != nodeMap.end() && c != nodeMap.end());

	(*c).second.parents.insert(parent);
	(*p).second.children.insert(child);
    }
    /*
     * Delete a graph edge
     */
    void delEdge(_Key parent, _Key child)
    {
	typename std::map<_Key, DAGNode<_Key, _Val> >::iterator p = nodeMap.find(parent);
	typename std::map<_Key, DAGNode<_Key, _Val> >::iterator c = nodeMap.find(child);

	ASSERT(p != nodeMap.end() && c != nodeMap.end());

	(*c).second.parents.erase(parent);
	(*p).second.children.erase(child);
    }
    /*
     * Find the Lowest Common Ancestor (LCA) of two keys
     */
    _Key findLCA(_Key p1, _Key p2)
    {
	typename std::map<_Key, DAGNode<_Key, _Val> >::iterator it1;
	typename std::map<_Key, DAGNode<_Key, _Val> >::iterator it2;
	typename std::unordered_set<_Key> p1p, p2p;
	typename std::unordered_set<_Key> new_p1p, new_p2p;
	typename std::unordered_set<_Key> next_p1p, next_p2p;
	typename std::unordered_set<_Key>::iterator it;

	/*
	 * Step 1: Add next set of nodes in p1 & p2 to unordered_set and paths to
	 * queue.
	 * Step 2: Check if any of these nodes are in the existing sets.
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
		typename std::unordered_set<_Key>::iterator k;
		k = p2p.find(*it);
		if (k != p2p.end()) {
		    return *k;
		}
	    }
	    for (it = new_p1p.begin(); it != new_p1p.end(); it++) {
		p1p.insert(*it);
	    }
	    for (it = new_p2p.begin(); it != new_p2p.end(); it++) {
		typename std::unordered_set<_Key>::iterator k;
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
		    typename std::unordered_set<_Key>::iterator k;
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
		    typename std::unordered_set<_Key>::iterator k;
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
    // XXX: THIS DOES NOT PRODUCE THE RIGHT ORDERING
    std::list<_Key> getBottomUp(_Key tip) {
	std::unordered_set<_Key> s = std::unordered_set<_Key>();
	std::list<_Key> v = std::list<_Key>();
	std::list<_Key> q = std::list<_Key>();
	std::list<_Key> nextQ = std::list<_Key>();

	q.push_back(tip);
	while (true) {
	    if (q.size() == 0)
		return v;

	    for (typename std::list<_Key>::iterator it = q.begin();
		 it != q.end();
		 it++)
	    {
		typename std::unordered_set<_Key>::iterator p;
		p = s.find(*it);
		if (p == s.end()) {
		    v.push_front(*it);
		    s.insert(*it);

		    std::unordered_set<_Key> parents;
		    typename std::unordered_set<_Key>::iterator jt;

		    parents = nodeMap[*it].listParents();

		    for (jt = parents.begin();
			 jt != parents.end();
			 jt++)
		    {
			nextQ.push_back(*jt);
		    }
		}
	    }

	    q.clear();
	    q = nextQ;
	    nextQ.clear();
	}
    }
private:
    template<class, class> friend class DAG;
    typename std::map<_Key, DAGNode<_Key, _Val> > nodeMap;
};

#endif /* __DAG_H__ */

