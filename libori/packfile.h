#ifndef __PACKFILE_H__
#define __PACKFILE_H__

#include <tr1/memory>

#include "objecthash.h"

class Packfile
{
public:
    typedef std::tr1::shared_ptr<Packfile> sp;
    
    Packfile(const std::string &filename);
    ~Packfile();

    void addObject(const ObjectHash &hash, bytestream *data_str);
    void purge(const ObjectHash &hash);

private:
    int fd;
};

#endif
