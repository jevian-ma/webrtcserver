#include <agent.h>
#include <gio/gnetworking.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdint.h>
#include <pthread.h>
#include <jansson.h>
#include <semaphore.h>
#include "secmalloc.h"

const gchar *candidate_type_name[] = {"host", "srflx", "prflx", "relay"};
guint component_id = 1;

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
    struct TURNLIST *turnlist;
};

struct THREADARGS {
    json_t *obj;
    sem_t sem;
    char *json;
};

struct ICELIST {
    size_t roomid;
    size_t connectid;
    GMainLoop *gloop;
    NiceAgent *agent;
    struct THREADARGS *threadargs;
    struct ICELIST *tail;
};
struct ICELIST *icelist = NULL;
size_t lastroomid = 0;
size_t lastconnectid = 0;

void networkinginit () {
    g_networking_init();
}

void cb_candidate_gathering_done (NiceAgent *agent, guint stream_id, gpointer data) {
    gchar *local_ufrag = NULL;
    gchar *local_pwd = NULL;
    json_t *retobj = NULL;
    GSList *cands = NULL;
    GSList *remote_candidates = NULL;
    struct ICELIST *ice = data;
// 设置本地ice信息
    if (!nice_agent_get_local_credentials(agent, stream_id, &local_ufrag, &local_pwd)) {
        printf("get local credentials fail, in %s, at %d\n", __FILE__, __LINE__);
        goto end1;
    }
    retobj = json_object();
    json_object_set_new(retobj, "iceufrag", json_string(local_ufrag));
    json_object_set_new(retobj, "icepwd", json_string(local_pwd));
    cands = nice_agent_get_local_candidates(agent, stream_id, component_id);
    if (cands == NULL) {
        printf("get local candidates fail, in %s, at %d\n", __FILE__, __LINE__);
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
    json_object_set_new(retobj, "track", arr);
end1:
    if (retobj != NULL) {
        json_object_set_new(retobj, "roomid", json_integer(ice->roomid));
        json_object_set_new(retobj, "connectid", json_integer(ice->connectid));
        ice->threadargs->json = json_dumps(retobj, JSON_COMPACT); // JSON_ESCAPE_SLASH这个flag以后可能会有用，将对象中的"/"替换成"\/"再输出
        json_decref (retobj);
    }
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
    json_t *sendobj = ice->threadargs->obj;
    json_t *iceufragobj = json_object_get (sendobj, "iceufrag");
    json_t *icepwdobj = json_object_get (sendobj, "icepwd");
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
    json_t *candidatesobj = json_object_get (sendobj, "candidates");
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
        c->component_id = component_id;
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
    if (nice_agent_set_remote_candidates(agent, stream_id, component_id, remote_candidates) < 1) {
        printf("failed to set remote candidates, in %s, at %d\n", __FILE__, __LINE__);
    }
end2:
    sem_post(&ice->threadargs->sem);
    if (remote_candidates != NULL) {
        g_slist_free_full(remote_candidates, (GDestroyNotify)&nice_candidate_free);
    }
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

void *createicd_thread (void *args) {
    struct THREADARGS *threadargs = args;
    json_t *obj = threadargs->obj;
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
        if (flag == 0 && lastroomid != 0) {
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
        if (flag == 0 && lastconnectid != 0) {
            ice->connectid = lastconnectid;
            lastconnectid++;
            break;
        } else {
            lastconnectid++;
        }
    }
    ice->gloop = gloop;
    ice->agent = agent;
    ice->threadargs = threadargs;
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
    json_t *iceserversobj = json_object_get (obj, "iceservers");
    if (iceserversobj != NULL) {
        json_t *stun_serverobj = json_object_get (iceserversobj, "stun_server");
        json_t *stun_portobj = json_object_get (iceserversobj, "stun_port");
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
    }
    g_signal_connect(agent, "candidate-gathering-done", G_CALLBACK(cb_candidate_gathering_done), ice);
    g_signal_connect(agent, "new-selected-pair", G_CALLBACK(cb_new_selected_pair), ice);
    g_signal_connect(agent, "component-state-changed", G_CALLBACK(cb_component_state_changed), ice);
    guint stream_id = nice_agent_add_stream(agent, 2);
    nice_agent_attach_recv(agent, stream_id, component_id, g_main_loop_get_context (gloop), cb_nice_recv, NULL);
    if (iceserversobj != NULL) {
        json_t *turnlistobj = json_object_get (iceserversobj, "turnlist");
        if (turnlistobj != NULL) {
            size_t length = json_array_size(turnlistobj);
            for (size_t i = 0 ; i < length ; i++) {
                json_t *turnobj = json_array_get(turnlistobj, i);
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
                            nice_agent_set_relay_info(agent, stream_id, 1, ipaddr, turn_port, turn_user, turn_pwd, NICE_RELAY_TYPE_TURN_UDP);
                        } else if(host->h_addrtype == AF_INET6){
                            char ipaddr[INET6_ADDRSTRLEN];
                            inet_ntop(AF_INET6, host->h_addr_list[0], ipaddr, INET6_ADDRSTRLEN);
                            nice_agent_set_relay_info(agent, stream_id, 1, ipaddr, turn_port, turn_user, turn_pwd, NICE_RELAY_TYPE_TURN_UDP);
                        }
                    }
                }
            }
        }
    }
    if(!nice_agent_gather_candidates(agent, stream_id)) {
        printf("Failed to start candidate gathering, in %s, at %d\n", __FILE__, __LINE__);
    }
    g_main_loop_run (gloop);
    g_main_loop_unref(gloop);
    g_object_unref(agent);
}

void createicd (json_t *obj, char *res) {
    struct THREADARGS *threadargs = (struct THREADARGS*)secmalloc(sizeof(struct THREADARGS));
    threadargs->obj = obj;
    sem_init(&threadargs->sem, 0, 0);
    threadargs->json = NULL;
    pthread_t tid;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_create(&tid, &attr, createicd_thread, threadargs);
    pthread_attr_destroy (&attr);
    sem_wait(&threadargs->sem);
    sem_destroy(&threadargs->sem);
    if (threadargs->json) {
        strcpy(res, threadargs->json);
        free(threadargs->json); // 这里使用free而非secfree，因为这个对象是jansson的json_dumps生成的，不是我生成的。
    } else {
        strcpy(res, "{\"errcode\":-2,\"errmsg\":\"get ice fail\"}");
    }
    secfree(threadargs);
}
