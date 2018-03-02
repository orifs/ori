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

#include <stdio.h>

#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <pwd.h>
#include <grp.h>

#include <string>
#include <set>
#include <sstream>
#include <iostream>
#include <algorithm>

#include <oriutil/debug.h>
#include <oriutil/oriutil.h>
#include <oriutil/orifile.h>
#include <oriutil/oricrypt.h>
#include <oriutil/scan.h>
#include <ori/repo.h>
#include <ori/largeblob.h>
#include <ori/tree.h>

using namespace std;

/********************************************************************
 *
 *
 * AttrMap
 *
 *
 ********************************************************************/

AttrMap::AttrMap()
{
}

bool AttrMap::has(const std::string &attrName) const
{
    return attrs.find(attrName) != attrs.end();
}

void AttrMap::setFromFile(const std::string &filename)
{
    struct stat sb;

    if (stat(filename.c_str(), &sb) < 0) {
	perror("AttrMap::setFromFile stat");
        fprintf(stderr, "(%s)\n", filename.c_str());
	PANIC();
    }

    struct passwd *upw = getpwuid(sb.st_uid);
    struct group *ggr = getgrgid(sb.st_gid);
    setAs<size_t>(ATTR_FILESIZE, sb.st_size);
    setAs<mode_t>(ATTR_PERMS, sb.st_mode & ~S_IFDIR & ~S_IFREG);
    attrs[ATTR_USERNAME] = upw->pw_name;
    attrs[ATTR_GROUPNAME] = ggr->gr_name;
    setAs<time_t>(ATTR_CTIME, sb.st_ctime);
    setAs<time_t>(ATTR_MTIME, sb.st_mtime);
}

void AttrMap::setCreation(mode_t perms)
{
    struct passwd *upw = getpwuid(geteuid());
    struct group *ggr = getgrgid(getegid());
    setAs<size_t>(ATTR_FILESIZE, 0);
    setAs<mode_t>(ATTR_PERMS, perms);
    attrs[ATTR_USERNAME] = upw->pw_name;
    attrs[ATTR_GROUPNAME] = ggr->gr_name;

    time_t currTime = time(NULL);
    setAs<time_t>(ATTR_CTIME, currTime);
    setAs<time_t>(ATTR_MTIME, currTime);
}

void AttrMap::mergeFrom(const AttrMap &other)
{
    for (auto const &it : other) {
        attrs[it.first] = it.second;
    }
}

/********************************************************************
 *
 *
 * TreeEntry
 *
 *
 ********************************************************************/

TreeEntry::TreeEntry()
{
    type = Null;
}

TreeEntry::TreeEntry(const ObjectHash &hash, const ObjectHash &lhash)
    : hash(hash), largeHash(lhash)
{
    if (lhash.isEmpty())
        type = Blob;
    else
        type = LargeBlob;
}

TreeEntry::~TreeEntry()
{
}

bool
TreeEntry::isTree()
{
    return type == Tree;
}

void
TreeEntry::extractToFile(const std::string &filename, Repo *src) const
{
    ASSERT(type != Null);
    Object::sp o(src->getObject(hash));
    if (type == Blob) {
        bytestream::ap bs(o->getPayloadStream());
        bs->copyToFile(filename);
    }
    else if (type == LargeBlob) {
        ::LargeBlob lb(src);
        lb.fromBlob(o->getPayload());
        lb.extractFile(filename);
    }
    else {
        NOT_IMPLEMENTED(false);
    }
}

bool
TreeEntry::hasBasicAttrs()
{
    const char *names[] = {ATTR_FILESIZE, ATTR_PERMS, ATTR_USERNAME,
        ATTR_GROUPNAME, ATTR_CTIME, ATTR_MTIME};
    for (size_t i = 0; i < sizeof(names) / sizeof(const char *); i++) {
        //LOG("Checking attr %s", names[i]);
        if (attrs.attrs.find(names[i]) == attrs.attrs.end())
        {
            WARNING("Attribute '%s' does not exist", names[i]);
            return false;
        }
    }
    return true;
}

void
TreeEntry::print() const
{
    AttrMap::const_iterator it;

    cout << "  Hash: " << hash.hex() << endl;
    if (type == TreeEntry::LargeBlob)
        cout << "  Large Hash: " << largeHash.hex() << endl;

    cout << "  Size: " << attrs.getAs<size_t>(ATTR_FILESIZE) << endl;
    cout << "  User: " << attrs.getAsStr(ATTR_USERNAME) << endl;
    cout << "  Group: " << attrs.getAsStr(ATTR_GROUPNAME) << endl;
    // perms and link
    cout << "  Created: " << attrs.getAs<time_t>(ATTR_CTIME) << endl;
    cout << "  Modified: " << attrs.getAs<time_t>(ATTR_MTIME) << endl;
}

/********************************************************************
 *
 *
 * Tree
 *
 *
 ********************************************************************/

Tree::Tree()
{
}

Tree::~Tree()
{
}

const string
Tree::getBlob() const
{
    strwstream ss;
    ss.enableTypes();
    size_t size = tree.size();
    ss.writeUInt64(size);

    for (auto const &it : tree) {
        const TreeEntry &te = it.second;
        if (te.type == TreeEntry::Tree) {
            ss.write("tree", 4);
        } else if (te.type == TreeEntry::Blob) {
            ss.write("blob", 4);
        } else if (te.type == TreeEntry::LargeBlob) {
            ss.write("lgbl", 4);
        } else {
            PANIC();
        }

        ss.writeHash(te.hash);
        if (te.type == TreeEntry::LargeBlob)
            ss.writeHash(te.largeHash);
        ss.writePStr(it.first);
        
        size_t asize = te.attrs.attrs.size();
        ss.writeUInt32(asize);
        for (auto const &ait : te.attrs.attrs) {
            ss.writePStr(ait.first);
            ss.writePStr(ait.second);
        }
    }

    return ss.str();
}

void
Tree::fromBlob(const string &blob)
{
    strstream ss(blob);
    ss.enableTypes();
    size_t num_entries = ss.readUInt64();
    
    for (size_t i = 0; i < num_entries; i++) {
        TreeEntry entry;
        char type[5] = {'\0'};
        ss.readExact((uint8_t*)type, 4);
        if (strcmp(type, "tree") == 0) {
            entry.type = TreeEntry::Tree;
        }
        else if (strcmp(type, "blob") == 0) {
            entry.type = TreeEntry::Blob;
        }
        else if (strcmp(type, "lgbl") == 0) {
            entry.type = TreeEntry::LargeBlob;
        }
        else {
            PANIC();
        }

        ss.readHash(entry.hash);
        if (entry.type == TreeEntry::LargeBlob) {
            ss.readHash(entry.largeHash);
        }
        string path;
        int status = ss.readPStr(path);
        ASSERT(status > 0);

        size_t num_attrs = ss.readUInt32();
        for (size_t i_a = 0; i_a < num_attrs; i_a++) {
            string attrName, attrValue;

            status = ss.readPStr(attrName);
            ASSERT(status > 0);
            status = ss.readPStr(attrValue);
            ASSERT(status > 0);

            entry.attrs.attrs[attrName] = attrValue;
        }

        tree[path] = entry;
    }
}

void
_recFlatten(
        const std::string &prefix,
        const Tree *t,
        Tree::Flat *rval,
        Repo *r
        )
{
    for (auto const &it : t->tree) {
        const TreeEntry &te = it.second;
        rval->insert(make_pair(prefix + it.first, te));
        if (te.type == TreeEntry::Tree) {
            // Recurse further
            Tree subtree = r->getTree(te.hash);
            _recFlatten(prefix + it.first + "/",
                    &subtree, rval, r);
        }
    }
}

/*
 * Constructs a flat tree given the repository that this tree was taken from.
 */
Tree::Flat
Tree::flattened(Repo *r) const
{
    Tree::Flat rval;
    _recFlatten("/", this, &rval, r);
    return rval;
}

size_t _num_path_components(const std::string &path)
{
    if (path.size() == 0) return 0;
    size_t cnt = 0;
    for (size_t i = 0; i < path.size(); i++) {
        if (path[i] == '/')
            cnt++;
    }
    return 1 + cnt;
}

bool _tree_gt(const std::string &t1, const std::string &t2)
{
    return _num_path_components(t1) > _num_path_components(t2);
}

/*
 * Commits a flat tree into the repository 'r'. This function returns a tree 
 * object for the root tree that was committed.
 */
Tree
Tree::unflatten(const Flat &flat, Repo *r)
{
    map<string, Tree> trees;
    for (auto const &it : flat) {
        const TreeEntry &te = it.second;
        if (te.type == TreeEntry::Tree) {
            if (trees.find(it.first) == trees.end()) {
                trees[it.first] = Tree();
            }
        }
        else if (te.type == TreeEntry::Blob ||
                te.type == TreeEntry::LargeBlob) {
            string treename = OriFile_Dirname(it.first);
            if (trees.find(treename) == trees.end()) {
                trees[treename] = Tree();
            }
            string basename = OriFile_Basename(it.first);
            trees[treename].tree[basename] = te;
        }
        else {
            ASSERT(false && "TreeEntry is type Null");
        }
    }

    // Get the tree hashes, update parents, add to Repo
    vector<string> tree_names;
    for (auto const &it : trees) {
        tree_names.push_back(it.first);
    }
    // Update leaf trees (no child directories) first
    std::sort(tree_names.begin(), tree_names.end(), _tree_gt);

    for (size_t i = 0; i < tree_names.size(); i++) {
        const string &tn = tree_names[i];
        if (tn.size() == 0) continue;
        string blob = trees[tn].getBlob();
        ObjectHash hash = OriCrypt_HashString(blob);

        // Add to Repo
        r->addObject(ObjectInfo::Tree, hash, blob);

        // Add to parent
        TreeEntry te = (*flat.find(tree_names[i])).second;
        te.hash = hash;
        te.type = TreeEntry::Tree;
        ASSERT(te.hasBasicAttrs());

        string parent = OriFile_Dirname(tn);
        trees[parent].tree[OriFile_Basename(tn)] = te;
    }

    r->addBlob(ObjectInfo::Tree, trees[""].getBlob());

    return trees[""];
}

ObjectHash
Tree::hash() const
{
    return OriCrypt_HashString(getBlob());
}

void
Tree::print() const
{
    map<string, TreeEntry>::const_iterator it;

    for (auto &it : tree)
    {
	cout << "Path: " << it.first << endl;
	switch (it.second.type)
	{
	    case TreeEntry::Null:
		cout << "  Type: Null" << endl;
		break;
	    case TreeEntry::Blob:
		cout << "  Type: Blob" << endl;
		break;
	    case TreeEntry::LargeBlob:
		cout << "  Type: LargeBlob" << endl;
		break;
	    case TreeEntry::Tree:
		cout << "  Type: Tree" << endl;
		break;
	    default:
		NOT_IMPLEMENTED(false);
		break;
	}

	it.second.print();
    }
}

