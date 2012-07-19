#include <errno.h>

#include "tempdir.h"
#include "localobject.h"
#include "util.h"
#include "scan.h"

TempDir::TempDir(const std::string &dirpath)
    : dirpath(dirpath)
{
    index.open(pathTo("index"));
}

TempDir::~TempDir()
{
    // TODO: remove the temp dir
}

std::string
TempDir::pathTo(const std::string &file)
{
    return dirpath + "/" + file;
}

/*
 * Repo implementation
 */
Object::sp
TempDir::getObject(const std::string &objId)
{
    assert(objId.length() == 64);
    std::string path = pathTo(objId);
    if (Util_FileExists(path)) {
        LocalObject::sp o(new LocalObject());
        o->open(path, objId);
        return Object::sp(o);
    }
    return Object::sp();
}

bool
TempDir::hasObject(const std::string &objId)
{
    assert(objId.length() == 64);
    return index.hasObject(objId);
}

std::set<ObjectInfo>
TempDir::listObjects()
{
    return index.getList();
}

int
TempDir::addObjectRaw(const ObjectInfo &info, bytestream *bs)
{
    assert(info.hasAllFields());
    if (!index.hasObject(info.hash)) {
        std::string objPath = pathTo(info.hash);

        LocalObject o;
        if (o.createFromRawData(objPath, info, bs->readAll()) < 0) {
            perror("Unable to create object");
            return -errno;
        }

        index.updateInfo(info.hash, info);
    }

    return 0;
}
