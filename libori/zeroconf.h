#ifndef __ZEROCONF_H__
#define __ZEROCONF_H__

struct OriPeer
{
    std::string hostname;
    uint16_t port;
};

void ori_setup_mdns(uint16_t port_num);
int ori_run_mdns(bool use_select = true);
std::vector<OriPeer> get_local_peers();

#endif
