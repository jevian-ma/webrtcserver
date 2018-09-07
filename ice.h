#ifndef __ICE_H__
#define __ICE_H__

#include <agent.h>

void networkinginit ();
void createicd ();

struct TURNLIST {
    char *turn_server;
    uint16_t turn_port;
    char *turn_user;
    char *turn_pwd;
    struct TURNLIST *tail;
};

struct ICESERVERS {
    char *stun_server;
    uint16_t stun_port;
    struct TURNLIST* turnlist;
};

#endif
