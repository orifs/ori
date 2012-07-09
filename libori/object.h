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

#ifndef __OBJECT_H__
#define __OBJECT_H__

#include <stdint.h>

#include <utility>
#include <string>
#include <map>

#include "stream.h"


#define ORI_OBJECT_TYPESIZE	    4
#define ORI_OBJECT_FLAGSSIZE	4
#define ORI_OBJECT_SIZE		    8
#define ORI_OBJECT_HDRSIZE	    24

#define ORI_MD_HASHSIZE 32 //SHA256_DIGEST_LENGTH

#define ORI_FLAG_COMPRESSED 0x0001

#define ORI_FLAG_DEFAULT ORI_FLAG_COMPRESSED

class Object {
public:
    enum Type { Null, Commit, Tree, Blob, LargeBlob, Purged };
    enum MdType { MdNull, MdBackref };
    enum BRState { BRNull, BRRef, BRPurged };

    typedef std::auto_ptr<Object> ap;

    struct ObjectInfo {
        ObjectInfo();
        ObjectInfo(const char *hash);
        std::string getInfo() const;
        void setInfo(const std::string &info);
        ssize_t writeTo(int fd, bool seekable = true);
        bool hasAllFields() const;

        // Flags operations
        bool getCompressed() const;

        bool operator <(const ObjectInfo &) const;

        Type type;
        int flags;
        size_t payload_size;
        std::string hash;
    };

    Object() {}
    Object(const ObjectInfo &info) : info(info) {}
    virtual ~Object() {}

    virtual const ObjectInfo &getInfo() const { return info; }
    virtual std::auto_ptr<bytestream> getPayloadStream() = 0;
    virtual std::auto_ptr<bytestream> getStoredPayloadStream() = 0;
    virtual size_t getStoredPayloadSize() = 0;
    
    virtual std::string getPayload() {
        return getPayloadStream()->readAll();
    }
    
    // Static methods
    static const char *getStrForType(Type t);
    static Type getTypeForStr(const char *str);

protected:
    ObjectInfo info;
};

typedef Object::ObjectInfo ObjectInfo;


class LocalObject : public Object
{
public:
    LocalObject();
    ~LocalObject();
    int create(const std::string &path, Type type, uint32_t flags = ORI_FLAG_DEFAULT);
    int createFromRawData(const std::string &path, const ObjectInfo &info,
            const std::string &raw_data);
    int open(const std::string &path, const std::string &hash="");
    void close();
    size_t getFileSize();

    // BaseObject implementation
    std::auto_ptr<bytestream> getPayloadStream();
    std::auto_ptr<bytestream> getStoredPayloadStream();
    size_t getStoredPayloadSize();

    // Payload Operations
    int purge();
    int setPayload(bytestream *bs);
    int setPayload(const std::string &blob);
    std::string computeHash();
    // Metadata
    void addMetadataEntry(MdType type, const std::string &data);
    std::string computeMetadataHash();
    bool checkMetadata();
    void clearMetadata();
    // Backreferences
    void addBackref(const std::string &objId, BRState state);
    void updateBackref(const std::string &objId, BRState state);
    std::map<std::string, BRState> getBackref();

private:
    int fd;
    size_t storedLen;
    std::string objPath;
    size_t fileSize;

    void setupLzma(lzma_stream *strm, bool encode);
    bool appendLzma(int dstFd, lzma_stream *strm, lzma_action action);

    const char *_getIdForMdType(MdType);
    MdType _getMdTypeForStr(const char *);
};



#endif /* __OBJECT_H__ */

