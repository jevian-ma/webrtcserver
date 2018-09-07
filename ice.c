#include "ice.h"
#include <gio/gnetworking.h>
#include <stdint.h>
#include <pthread.h>
#include "secmalloc.h"

struct ICELIST {
    uint64_t roomid;
    uint64_t connectid;
    GMainLoop *gloop;
    NiceAgent *agent;
    struct ICELIST *tail;
};
struct ICELIST *icelist = NULL;
uint64_t lastroomid = 0;
uint64_t lastconnectid = 0;

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
    if (icelist == NULL) {
        icelist = (struct ICELIST*)secmalloc(sizeof(struct ICELIST));
        icelist->roomid = lastroomid;
        icelist->connectid = lastconnectid;
        icelist->gloop = gloop;
        icelist->agent = agent;
        icelist->tail = NULL;
        lastroomid++;
        lastconnectid++;
    } else {
        struct ICELIST *ice = (struct ICELIST*)secmalloc(sizeof(struct ICELIST));
        while (1) { // 寻找一个不重复的roomid
            int findrepeat = 0;
            struct ICELIST *pointer = icelist;
            while (pointer != NULL) {
                if (lastroomid == pointer->roomid) {
                    findrepeat = 1;
                    break;
                }
                pointer = pointer->tail;
            }
            if (findrepeat == 0) {
                ice->roomid = lastroomid;
                break;
            }
            lastroomid++;
        }
        while (1) { // 寻找一个不重复的connectid
            int findrepeat = 0;
            struct ICELIST *pointer = icelist;
            while (pointer != NULL) {
                if (lastconnectid == pointer->connectid) {
                    findrepeat = 1;
                    break;
                }
                pointer = pointer->tail;
            }
            if (findrepeat == 0) {
                ice->connectid = lastconnectid;
                break;
            }
            lastconnectid++;
        }
        ice->gloop = gloop;
        ice->agent = agent;
        ice->tail = NULL;
        struct ICELIST *p = icelist;
        while (p->tail != NULL) {
            p = p->tail;
        }
        p->tail = ice;
        lastroomid++;
        lastconnectid++;
    }
    if (iceservers != NULL && iceservers->stun_server != NULL) {
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
        struct TURNLIST *turn = iceservers->turnlist;
        while (turn != NULL) {
            nice_agent_set_relay_info(agent, stream_id, 1, turn->turn_server, turn->turn_port, turn->turn_user, turn->turn_pwd, NICE_RELAY_TYPE_TURN_UDP);
            turn = turn->tail;
        }
    }
    nice_agent_attach_recv(agent, stream_id, 1, g_main_loop_get_context (gloop), cb_nice_recv, NULL);
    nice_agent_gather_candidates(agent, stream_id);
    g_main_loop_run (gloop);
    g_main_loop_unref(gloop);
    g_object_unref(agent);
    if (iceservers) {
        if (iceservers->stun_server != NULL) {
            secfree(iceservers->stun_server);
        }
        struct TURNLIST *turn = iceservers->turnlist;
        while (turn != NULL) {
            struct TURNLIST *pointer = turn;
            turn = turn->tail;
            if (pointer->turn_server != NULL) {
                secfree(pointer->turn_server);
            }
            if (pointer->turn_user != NULL) {
                secfree(pointer->turn_user);
            }
            if (pointer->turn_pwd != NULL) {
                secfree(pointer->turn_pwd);
            }
            secfree(pointer);
        }
        secfree(iceservers);
    }
}

void createicd (struct ICESERVERS *iceservers) {
    pthread_t tid;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_create(&tid, &attr, createicd_thread, iceservers);
    pthread_attr_destroy (&attr);
}
