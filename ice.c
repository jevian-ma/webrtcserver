#include <agent.h>
#include <gio/gnetworking.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdint.h>
#include <semaphore.h>
#include <jansson.h>
#include "secmalloc.h"

const gchar *candidate_type_name[] = {"host", "srflx", "prflx", "relay"};
GMainLoop *gloop;
NiceAgent *agent;

enum STREAMTYPE {
    VIDEO,
    AUDIO
};

struct STREAMLIST {
    guint id;
    enum STREAMTYPE type;
    sem_t sem;
    json_t *obj;
    json_t *res;
    struct STREAMLIST *tail;
};

struct ROOMLIST {
    char *id;
    struct STREAMLIST *streamlist;
    struct ROOMLIST *tail;
};
struct ROOMLIST *roomlist = NULL;

void networkinginit () {
    g_networking_init();
}

void cb_candidate_gathering_done (NiceAgent *agent, guint stream_id, gpointer data) {
    gchar *local_ufrag = NULL;
    gchar *local_pwd = NULL;
    GSList *cands = NULL;
    GSList *remote_candidates = NULL;
    struct STREAMLIST *stream = NULL;
    struct ROOMLIST *room = roomlist;
    while (room != NULL) {
        int flag = 0;
        struct STREAMLIST *p = room->streamlist;
        while (p != NULL) {
            if (p->id == stream_id) {
                stream = p;
                flag = 1;
                break;
            }
            p = p->tail;
        }
        if (flag == 1) {
            break;
        }
        room = room->tail;
    }
    if (stream == NULL) {
        printf("stream id is not use, stream_id = %d, in %s, at %d\n", stream_id, __FILE__, __LINE__);
        goto end1;
    }
// 设置本地ice信息
    if (!nice_agent_get_local_credentials(agent, stream_id, &local_ufrag, &local_pwd)) {
        printf("get local credentials fail, stream_id = %d, in %s, at %d\n", stream_id, __FILE__, __LINE__);
        goto end1;
    }
    stream->res = json_object();
    json_object_set_new(stream->res, "iceufrag", json_string(local_ufrag));
    json_object_set_new(stream->res, "icepwd", json_string(local_pwd));
    cands = nice_agent_get_local_candidates(agent, stream_id, 1);
    if (cands == NULL) {
        printf("get local candidates fail, stream_id = %d, in %s, at %d\n", stream_id, __FILE__, __LINE__);
        goto end1;
    }
    json_t *arr = json_array();
    GSList *item = cands;
    while (item != NULL) {
        gchar ipaddr[INET6_ADDRSTRLEN];
        NiceCandidate *c = (NiceCandidate *)item->data;
        nice_address_to_string(&c->addr, ipaddr);
        json_t *tmp = json_object();
        json_object_set_new(tmp, "foundation", json_string(c->foundation));
        json_object_set_new(tmp, "priority", json_integer(c->priority));
        json_object_set_new(tmp, "ipaddr", json_string(ipaddr));
        json_object_set_new(tmp, "port", json_integer(nice_address_get_port(&c->addr)));
        json_object_set_new(tmp, "type", json_string(candidate_type_name[c->type]));
        json_array_append_new(arr, tmp);
        item = item->next;
    }
    json_object_set_new(stream->res, "track", arr);
end1:
    if (local_ufrag != NULL) {
        g_free(local_ufrag);
    }
    if (local_pwd != NULL) {
        g_free(local_pwd);
    }
    if (cands != NULL) {
        g_slist_free_full(cands, (GDestroyNotify)&nice_candidate_free);
    }
// 设置远端ice信息
    json_t *obj = stream->obj;
    json_t *iceufragobj = json_object_get (obj, "iceufrag");
    json_t *icepwdobj = json_object_get (obj, "icepwd");
    if (iceufragobj == NULL || icepwdobj == NULL) {
        printf("iceufrag or icepwd is not found, in %s, at %d\n", __FILE__, __LINE__);
        goto end2;
    }
    const char *iceufrag = json_string_value (iceufragobj);
    const char *icepwd = json_string_value (icepwdobj);
    if (iceufrag == NULL || icepwd == NULL) {
        printf("iceufrag or icepwd is a not format, in %s, at %d\n", __FILE__, __LINE__);
        goto end2;
    }
    if (!nice_agent_set_remote_credentials(agent, stream_id, iceufrag, icepwd)) {
        printf("failed to set remote credentials, in %s, at %d\n", __FILE__, __LINE__);
        goto end2;
    }
    json_t *candidatesobj = json_object_get (obj, "candidates");
    size_t length = json_array_size(candidatesobj);
    for (size_t i = 0 ; i < length ; i++) {
        json_t *candidateobj = json_array_get(candidatesobj, i);
        json_t *priorityobj = json_object_get (candidateobj, "priority");
        uint32_t priority = json_integer_value (priorityobj);
        json_t *ipaddrobj = json_object_get (candidateobj, "ipaddr");
        const char *ipaddr = json_string_value (ipaddrobj);
        json_t *portobj = json_object_get (candidateobj, "port");
        uint16_t port = json_integer_value (portobj);
        json_t *typeobj = json_object_get (candidateobj, "type");
        const char *type = json_string_value (typeobj);
        NiceCandidateType ntype;
        for (guint j = 0; j < G_N_ELEMENTS (candidate_type_name); j++) {
            if (strcmp(type, candidate_type_name[j]) == 0) {
                ntype = j;
                break;
            }
        }
        NiceCandidate *c = nice_candidate_new(ntype);
        c->component_id = 1;
        c->stream_id = stream_id;
        c->transport = NICE_CANDIDATE_TRANSPORT_UDP;
        sprintf(c->foundation, "%d", i+1);
        c->priority = priority;
        if (!nice_address_set_from_string(&c->addr, ipaddr)) {
            printf("failed to parse addr: %s, in %s, at %d\n", ipaddr, __FILE__, __LINE__);
            nice_candidate_free(c);
            goto end2;
        }
        nice_address_set_port(&c->addr, port);
        remote_candidates = g_slist_prepend(remote_candidates, c);
    }
    if (nice_agent_set_remote_candidates(agent, stream_id, 1, remote_candidates) < 1) {
        printf("failed to set remote candidates, in %s, at %d\n", __FILE__, __LINE__);
    }
end2:
    if (remote_candidates != NULL) {
        g_slist_free_full(remote_candidates, (GDestroyNotify)&nice_candidate_free);
    }
    sem_post(&stream->sem);
}

void cb_new_selected_pair (NiceAgent *agent, guint stream_id, guint component_id, gchar *lfoundation, gchar *rfoundation, gpointer data) {
    printf("this is cb_new_selected_pair function, in %s, at %d\n", __FILE__, __LINE__);
}

void cb_component_state_changed (NiceAgent *agent, guint stream_id, guint component_id, guint state, gpointer data) {
    printf("this is cb_component_state_changed function, in %s, at %d\n", __FILE__, __LINE__);
}

void cb_nice_recv (NiceAgent *agent, guint stream_id, guint component_id, guint len, gchar *buf, gpointer data) {
    printf("this is cb_nice_recv function, in %s, at %d\n", __FILE__, __LINE__);
}

void createliveroom (json_t *obj, char *res) {
    json_t *roomidobj = json_object_get (obj, "roomid");
    if (roomidobj == NULL) {
        strcpy(res, "{\"errcode\":-3,\"errmsg\":\"roomid is not find\"}");
        printf("%s, in %s, at %d\n", res, __FILE__, __LINE__);
        return;
    }
    const char *roomid = json_string_value(roomidobj);
    if (roomid == NULL) {
        strcpy(res, "{\"errcode\":-4,\"errmsg\":\"roomid parse fail\"}");
        printf("%s, in %s, at %d\n", res, __FILE__, __LINE__);
        return;
    }
    int flag = 0;
    struct ROOMLIST *p = roomlist;
    while (p != NULL) {
        if (strcmp(roomid, p->id) == 0) {
            flag = 1;
            break;
        }
        p = p->tail;
    }
    if (flag == 1) {
        strcpy(res, "{\"errcode\":-5,\"errmsg\":\"cann't to create the live room, roomid is exist\"}");
        printf("%s, in %s, at %d\n", res, __FILE__, __LINE__);
        return;
    }
    struct ROOMLIST *room = (struct ROOMLIST*)secmalloc(sizeof(struct ROOMLIST));
    size_t length1 = strlen (roomid);
    room->id = (char*)secmalloc(length1 + 1);
    strcpy (room->id, roomid);
    room->tail = NULL;
    if (roomlist == NULL) {
        roomlist = room;
    } else {
        struct ROOMLIST *p = roomlist;
        while (p->tail != NULL) {
            p = p->tail;
        }
        p->tail = room;
    }
    struct STREAMLIST *stream1 = (struct STREAMLIST*)secmalloc(sizeof(struct STREAMLIST));
    stream1->id = nice_agent_add_stream(agent, 1);
    stream1->type = VIDEO;
    sem_init(&stream1->sem, 0, 0);
    stream1->obj = json_object_get (obj, "videoice");
    stream1->res = NULL;
    struct STREAMLIST *stream2 = (struct STREAMLIST*)secmalloc(sizeof(struct STREAMLIST));
    stream2->id = nice_agent_add_stream(agent, 1);
    stream2->type = AUDIO;
    sem_init(&stream2->sem, 0, 0);
    stream2->obj = json_object_get (obj, "audioice");;
    stream2->res = NULL;
    stream2->tail = NULL;
    stream1->tail = stream2;
    room->streamlist = stream1;
    struct STREAMLIST *stream = room->streamlist;
    while (stream != NULL) {
        nice_agent_attach_recv(agent, stream->id, 1, g_main_loop_get_context (gloop), cb_nice_recv, room);
        json_t *turn_serversobj = json_object_get (obj, "turn_servers");
        if (turn_serversobj != NULL) {
            size_t length2 = json_array_size(turn_serversobj);
            for (size_t i = 0 ; i < length2 ; i++) {
                json_t *turnobj = json_array_get(turn_serversobj, i);
                json_t *turn_serverobj = json_object_get (turnobj, "turn_server");
                json_t *turn_portobj = json_object_get (turnobj, "turn_port");
                json_t *turn_userobj = json_object_get (turnobj, "turn_user");
                json_t *turn_pwdobj = json_object_get (turnobj, "turn_pwd");
                if (turn_serverobj != NULL && turn_portobj != NULL && turn_userobj != NULL && turn_pwdobj != NULL) {
                    const char *turn_server = json_string_value(turn_serverobj);
                    uint16_t turn_port = json_integer_value(turn_portobj);
                    const char *turn_user = json_string_value(turn_userobj);
                    const char *turn_pwd = json_string_value(turn_pwdobj);
                    if (turn_server != NULL && turn_port != 0 && turn_user != NULL && turn_pwd != NULL) {
                        struct hostent *host = gethostbyname(turn_server);
                        if(host->h_addrtype == AF_INET) {
                            char ipaddr[INET_ADDRSTRLEN];
                            inet_ntop(AF_INET, host->h_addr_list[0], ipaddr, INET_ADDRSTRLEN);
                            nice_agent_set_relay_info(agent, stream->id, 1, ipaddr, turn_port, turn_user, turn_pwd, NICE_RELAY_TYPE_TURN_UDP);
                        } else if(host->h_addrtype == AF_INET6){
                            char ipaddr[INET6_ADDRSTRLEN];
                            inet_ntop(AF_INET6, host->h_addr_list[0], ipaddr, INET6_ADDRSTRLEN);
                            nice_agent_set_relay_info(agent, stream->id, 1, ipaddr, turn_port, turn_user, turn_pwd, NICE_RELAY_TYPE_TURN_UDP);
                        }
                    }
                }
            }
        }
        if(!nice_agent_gather_candidates(agent, stream->id)) {
            printf("Failed to start candidate gathering, in %s, at %d\n", __FILE__, __LINE__);
        }
        stream = stream->tail;
    }
    sem_wait(&stream1->sem);
    sem_wait(&stream2->sem);
    json_t *resobj = json_object();
    json_object_set_new(resobj, "videoice", stream1->res);
    json_object_set_new(resobj, "audioice", stream2->res);
    char *json = json_dumps(resobj, JSON_COMPACT); // JSON_ESCAPE_SLASH这个flag以后可能会有用，将对象中的"/"替换成"\/"再输出
    size_t length3 = strlen(json);
    strcpy(res, json);
    free (json); // 这里是释放json_dumps开辟的内存，因此不使用secfree
    json_decref (resobj);
}

void createicd () {
    json_t *configobj;
    if (access("config.json", R_OK) == 0) {
        json_error_t error;
        configobj = json_load_file("config.json", 0, &error);
        if (configobj == NULL) {
            printf("json error on line %d: %s, in %s, at %d\n", error.line, error.text, __FILE__, __LINE__);
        }
    } else if (access("/etc/webrtcserver/config.json", R_OK) == 0) {
        json_error_t error;
        configobj = json_load_file("/etc/webrtcserver/config.json", 0, &error);
        if (configobj == NULL) {
            printf("json error on line %d: %s, in %s, at %d\n", error.line, error.text, __FILE__, __LINE__);           
        }
    }
    GMainLoop *gloop = g_main_loop_new(NULL, FALSE);
    agent = nice_agent_new(g_main_loop_get_context (gloop), NICE_COMPATIBILITY_RFC5245);
    json_t *stun_serverobj = json_object_get (configobj, "stun_server");
    json_t *stun_portobj = json_object_get (configobj, "stun_port");
    if(stun_serverobj != NULL && stun_portobj != NULL) {
        const char *stun_server = json_string_value(stun_serverobj);
        uint16_t stun_port = json_integer_value(stun_portobj);
        if (stun_server != NULL && stun_port != 0) {
            struct hostent *host = gethostbyname(stun_server);
            if(host->h_addrtype == AF_INET) {
                char ipaddr[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, host->h_addr_list[0], ipaddr, INET_ADDRSTRLEN);
                g_object_set(agent, "stun-server", ipaddr, NULL);
                g_object_set(agent, "stun-server-port", stun_port, NULL);
            } else if(host->h_addrtype == AF_INET6){
                char ipaddr[INET6_ADDRSTRLEN];
                inet_ntop(AF_INET6, host->h_addr_list[0], ipaddr, INET6_ADDRSTRLEN);
                g_object_set(agent, "stun-server", ipaddr, NULL);
                g_object_set(agent, "stun-server-port", stun_port, NULL);
            }
        }
    }
    g_signal_connect(agent, "candidate-gathering-done", G_CALLBACK(cb_candidate_gathering_done), NULL);
    g_signal_connect(agent, "new-selected-pair", G_CALLBACK(cb_new_selected_pair), NULL);
    g_signal_connect(agent, "component-state-changed", G_CALLBACK(cb_component_state_changed), NULL);
    g_main_loop_run (gloop);
    g_main_loop_unref(gloop);
    g_object_unref(agent);
}
