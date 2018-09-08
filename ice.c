#include <agent.h>
#include <gio/gnetworking.h>
#include <stdint.h>
#include <pthread.h>
#include <jansson.h>
#include "secmalloc.h"

pthread_mutex_t ice_mutex = PTHREAD_MUTEX_INITIALIZER;

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

struct ICELIST {
    size_t roomid;
    size_t connectid;
    GMainLoop *gloop;
    NiceAgent *agent;
    struct ICELIST *tail;
};
struct ICELIST *icelist = NULL;
size_t lastroomid = 0;
size_t lastconnectid = 0;

void networkinginit () {
    g_networking_init();
}

void cb_candidate_gathering_done (NiceAgent *agent, guint _stream_id, gpointer data) {
}

void cb_new_selected_pair (NiceAgent *agent, guint _stream_id, guint component_id, gchar *lfoundation, gchar *rfoundation, gpointer data) {
}

void cb_component_state_changed(NiceAgent *agent, guint _stream_id, guint component_id, guint state, gpointer data) {
}

void cb_nice_recv () {
}

void *createicd_thread (void *args) {
    struct ICESERVERS *iceservers = args;
    GMainLoop *gloop = g_main_loop_new(NULL, FALSE);
    NiceAgent *agent = nice_agent_new(g_main_loop_get_context (gloop), NICE_COMPATIBILITY_RFC5245);
    pthread_mutex_lock(&ice_mutex);
    struct ICELIST *ice = (struct ICELIST*)secmalloc(sizeof(struct ICELIST));
    while (1) { // 寻找一个不重复的roomid
        int flag = 0;
        struct ICELIST *p = icelist;
        while (p != NULL) {
            if (lastroomid == p->roomid) {
                flag = 1;
                break;
            }
            p = p->tail;
        }
        if (flag == 0) {
            ice->roomid = lastroomid;
            lastroomid++;
            break;
        } else {
            lastroomid++;
        }
    }
    while (1) { // 寻找一个不重复的connectid
        int flag = 0;
        struct ICELIST *p = icelist;
        while (p != NULL) {
            if (lastconnectid == p->connectid) {
                flag = 1;
                break;
            }
            p = p->tail;
        }
        if (flag == 0) {
            ice->connectid = lastconnectid;
            lastconnectid++;
            break;
        } else {
            lastconnectid++;
        }
    }
    ice->gloop = gloop;
    ice->agent = agent;
    ice->tail = NULL;
    if (icelist == NULL) {
        icelist = ice;
    } else {
        struct ICELIST *p = icelist;
        while (p->tail != NULL) {
            p = p->tail;
        }
        p->tail = ice;
    }
    pthread_mutex_unlock(&ice_mutex);
    if (iceservers != NULL && iceservers->stun_server != NULL && iceservers->stun_port != 0) {
        g_object_set(agent, "stun-server", iceservers->stun_server, NULL);
        g_object_set(agent, "stun-server-port", iceservers->stun_port, NULL);
    }
    g_object_set(agent, "upnp", 0, NULL);
    g_object_set(agent, "controlling-mode", 0, NULL);
    g_signal_connect(agent, "candidate-gathering-done", G_CALLBACK(cb_candidate_gathering_done), NULL);
    g_signal_connect(agent, "new-selected-pair", G_CALLBACK(cb_new_selected_pair), NULL);
    g_signal_connect(agent, "component-state-changed", G_CALLBACK(cb_component_state_changed), NULL);
    guint stream_id = nice_agent_add_stream(agent, 1);
    if (iceservers != NULL && iceservers->turnlist != NULL) {
        struct TURNLIST *p = iceservers->turnlist;
        while (p != NULL) {
            if (p->turn_server != NULL && p->turn_port != 0 && p->turn_user != NULL && p->turn_pwd != NULL) {
                nice_agent_set_relay_info(agent, stream_id, 1, p->turn_server, p->turn_port, p->turn_user, p->turn_pwd, NICE_RELAY_TYPE_TURN_UDP);
            }
            p = p->tail;
        }
    }
    nice_agent_attach_recv(agent, stream_id, 1, g_main_loop_get_context (gloop), cb_nice_recv, NULL);
    nice_agent_gather_candidates(agent, stream_id);
    g_main_loop_run (gloop);
    g_main_loop_unref(gloop);
    g_object_unref(agent);
    if (iceservers != NULL) {
        if (iceservers->stun_server != NULL) {
            secfree(iceservers->stun_server);
        }
        struct TURNLIST *p = iceservers->turnlist;
        while (p != NULL) {
            struct TURNLIST *turn = p;
            p = p->tail;
            if (turn->turn_server != NULL) {
                secfree(turn->turn_server);
            }
            if (turn->turn_user != NULL) {
                secfree(turn->turn_user);
            }
            if (turn->turn_pwd != NULL) {
                secfree(turn->turn_pwd);
            }
            secfree(turn);
        }
        secfree(iceservers);
    }
}

void createicd (json_t *obj) {
    struct ICESERVERS *iceservers;
    json_t *iceserversobj = json_object_get (obj, "iceservers");
    if (iceserversobj != NULL) {
        iceservers = (struct ICESERVERS*)secmalloc(sizeof(struct ICESERVERS));
        json_t *stun_serverobj = json_object_get (iceserversobj, "stun_server");
        if (stun_serverobj != NULL) {
            const char *stun_server = json_string_value(stun_serverobj);
            size_t stun_serverlen = strlen(stun_server);
            iceservers->stun_server = (char*)secmalloc(stun_serverlen + 1);
            strcpy(iceservers->stun_server, stun_server);
        } else {
            iceservers->stun_server = NULL;
        }
        json_t *stun_portobj = json_object_get (iceserversobj, "stun_port");
        if (stun_portobj != NULL) {
            iceservers->stun_port = json_integer_value(stun_portobj);
        } else {
            iceservers->stun_port = 0;
        }
        iceservers->turnlist = NULL;
        json_t *turnlistobj = json_object_get (iceserversobj, "turnlist");
        if (turnlistobj != NULL) {
            size_t i;
            size_t length = json_array_size(turnlistobj);
            for (i = 0; i < length; i++) {
                struct TURNLIST *turn = (struct TURNLIST*)secmalloc(sizeof(struct TURNLIST));
                json_t *turnobj = json_array_get(turnlistobj, i);
                json_t *turn_serverobj = json_object_get (turnobj, "turn_server");
                if (turn_serverobj != NULL) {
                    const char *turn_server = json_string_value(turn_serverobj);
                    size_t turn_serverlen = strlen(turn_server);
                    turn->turn_server = (char*)secmalloc(turn_serverlen + 1);
                    strcpy(turn->turn_server, turn_server);
                } else {
                    turn->turn_server = NULL;
                }
                json_t *turn_portobj = json_object_get (turnobj, "turn_port");
                if (turn_portobj != NULL) {
                    turn->turn_port = json_integer_value(turn_portobj);
                } else {
                    turn->turn_port = 0;
                }
                json_t *turn_userobj = json_object_get (turnobj, "turn_user");
                if (turn_userobj != NULL) {
                    const char *turn_user = json_string_value(turn_userobj);
                    size_t turn_userlen = strlen(turn_user);
                    turn->turn_user = (char*)secmalloc(turn_userlen + 1);
                    strcpy(turn->turn_user, turn_user);
                } else {
                    turn->turn_user = NULL;
                }
                json_t *turn_pwdobj = json_object_get (turnobj, "turn_pwd");
                if (turn_pwdobj != NULL) {
                    const char *turn_pwd = json_string_value(turn_pwdobj);
                    size_t turn_pwdlen = strlen(turn_pwd);
                    turn->turn_pwd = (char*)secmalloc(turn_pwdlen + 1);
                    strcpy(turn->turn_pwd, turn_pwd);
                } else {
                    turn->turn_pwd = NULL;
                }
                turn->tail = NULL;
                if (iceservers->turnlist == NULL) {
                    iceservers->turnlist = turn;
                } else {
                    struct TURNLIST *p = iceservers->turnlist;
                    while (p->tail != NULL) {
                        p = p->tail;
                    }
                    p->tail = turn;
                }
            }
        }
    } else {
        iceservers = NULL;
    }
    pthread_t tid;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_create(&tid, &attr, createicd_thread, iceservers);
    pthread_attr_destroy (&attr);
}
