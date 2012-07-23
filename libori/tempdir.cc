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

#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>

#include "tempdir.h"
#include "localobject.h"
#include "util.h"
#include "scan.h"

TempDir::TempDir(const std::string &dirpath)
    : dirpath(dirpath), objects_fd(-1)
{
    index.open(pathTo("index"));

    objects_fd = open(pathTo("objects").c_str(),
            O_RDWR | O_CREAT, // | O_APPEND
            0644);
    if (objects_fd < 0) {
        perror("TempDir open");
    }
}

TempDir::~TempDir()
{
    if (objects_fd > 0) {
        printf("Closing temp dir\n");
        close(objects_fd);
        objects_fd = -1;
    }

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
    assert(objects_fd > 0);
    /*std::string path = pathTo(objId);
    if (Util_FileExists(path)) {
        LocalObject::sp o(new LocalObject());
        o->open(path, objId);
        return Object::sp(o);
    }
    return Object::sp();*/
    if (!hasObject(objId)) {
        printf("Temp objects do not contain %s\n", objId.c_str());
        index.dump();
        return Object::sp();
    }

    off_t off = offsets[objId];
    int status = lseek(objects_fd, off, SEEK_SET);
    if (status < 0) {
        perror("TempDir::getObject lseek");
        return Object::sp();
    }

    std::string info_ser;
    info_ser.resize(16);
    status = read(objects_fd, &info_ser[0], 16);
    if (status < 0) {
        perror("TempDir::getObject read");
        return Object::sp();
    }

    ObjectInfo info(objId.c_str());
    info.setInfo(info_ser);

    uint64_t stored_len;
    read(objects_fd, &stored_len, 8);

    return Object::sp(new TempObject(objects_fd, off+24, stored_len, info));
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
    assert(info.hash.size() == 64);
    if (!index.hasObject(info.hash)) {
        /*std::string objPath = pathTo(info.hash);

        LocalObject o;
        if (o.createFromRawData(objPath, info, bs->readAll()) < 0) {
            perror("Unable to create object");
            return -errno;
        }*/
        off_t off = lseek(objects_fd, 0, SEEK_END);
        std::string info_ser = info.getInfo();
        uint64_t stored_len = bs->sizeHint();

        write(objects_fd, info_ser.data(), info_ser.size());
        write(objects_fd, &stored_len, 8);

        int written = bs->copyToFd(objects_fd);
        if (written < 0) return written;

        if (stored_len == 0) {
            stored_len = written;
            pwrite(objects_fd, &stored_len, 8, off+16);
        }

        index.updateInfo(info.hash, info);
        offsets[info.hash] = off;
    }

    return 0;
}


/*
 * TempObject
 */
TempObject::TempObject(int fd, off_t off, size_t len, const ObjectInfo &info)
    : Object(info), fd(fd), off(off), len(len)
{
}

bytestream::ap
TempObject::getPayloadStream()
{
    if (info.getCompressed()) {
        bytestream::ap bs(getStoredPayloadStream());
        return bytestream::ap(new lzmastream(bs.release(), false, info.payload_size));
    }
    else {
        return getStoredPayloadStream();
    }
}

bytestream::ap
TempObject::getStoredPayloadStream()
{
    return bytestream::ap(new fdstream(fd, off, len));
}

size_t
TempObject::getStoredPayloadSize()
{
    return len;
}
