#ifndef __SSHCLIENT_H__
#define __SSHCLIENT_H__

#include "repo.h"

class SshClient
{
public:
    SshClient(const std::string &remoteHost, const std::string &remoteRepo);
    ~SshClient();

    int connect();
    void disconnect();

    // At the moment the protocol is synchronous
    void sendCommand(const std::string &command);
    void recvResponse(std::string &out);

private:
    std::string remoteHost, remoteRepo;

    int fdFromChild, fdToChild;
    int childPid;
};

class SshRepo : public BasicRepo
{
public:
    SshRepo(SshClient *client);
    ~SshRepo();

    int getObjectRaw(Object::ObjectInfo *info, std::string &raw_data);
    std::set<std::string> listObjects();
    Object addObjectRaw(Object::ObjectInfo info, const std::string &raw_data);

private:
    SshClient *client;
};

#endif
