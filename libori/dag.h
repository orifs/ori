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

#include <string>
#include <vector>
#include <map>

template <class _Key, class _Val>
class DAG;

template <class _Key, class _Val>
class DAGNode
{
public:
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
    std::vector<_Key> listParents()
    {
	return parents;
    }
private:
    template<class, class> friend class DAG;
    friend class DAG <_Key, _Val>;
    _Key k;
    _Val v;
    std::vector<_Key> parents;
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

	if ((*it) == nodeMap.end()) {
	    assert(false);
	}

	return (*it).second.getValue();
    }
    void addChild(_Key parent, _Key child)
    {
	typename std::map<_Key, DAGNode<_Key, _Val> >::iterator it = nodeMap.find(k);

	if ((*it) == nodeMap.end()) {
	    assert(false);
	}

	(*it).second.parents.add(parent);
    }
    _Key findLCA(_Key p1, _Key p2)
    {
	/*
	 * Step 1: Add next set of nodes in p1 & p2 to unordered_set and paths to
	 * queue.
	 * Step 2: Check if any of these nodes are in the the existing sets.
	 *  - Match: Return path and matching key
	 *  - Else: Repeat 1
	 */
    }
private:
    std::map<_Key, DAGNode<_Key, _Val> > nodeMap;
};

#endif /* __DAG_H__ */

