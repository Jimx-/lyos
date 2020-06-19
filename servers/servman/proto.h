#ifndef _SERVMAN_PROTO_H_
#define _SERVMAN_PROTO_H_

struct sproc;
struct service_up_req;

int alloc_sproc(struct sproc** ppsp);
int init_sproc(struct sproc* sp, struct service_up_req* up_req,
                      endpoint_t source);
int set_sproc(struct sproc* sp, struct service_up_req* up_req,
                     endpoint_t source);
int free_sproc(struct sproc* sp);
int read_exec(struct sproc* sp);

int do_service_up(MESSAGE* msg);
int do_service_init_reply(MESSAGE* msg);
int serv_exec(endpoint_t target, char* exec, int exec_len,
                     char* progname, char** argv);
int serv_prepare_module_stack();
int announce_service(char* name, endpoint_t serv_ep);

int late_reply(struct sproc* sp, int retval);

#endif
