#ifndef __SERVER_H__
#define __SERVER_H__

#define ORI_PROTO_VERSION "1.0"

class SshServer
{
public:
    SshServer();

    void serve();
    
    void cmd_hello();
    void cmd_listObjs();
    void cmd_listCommits();
    void cmd_readObjs();
    void cmd_getObjInfo();
    void cmd_getHead();
    void cmd_getFSID();

private:
};

#endif
