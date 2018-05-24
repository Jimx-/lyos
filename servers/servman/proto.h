#ifndef _SERVMAN_PROTO_H_
#define _SERVMAN_PROTO_H_

struct sproc;
struct service_up_req;

PUBLIC int alloc_sproc(struct sproc ** ppsp);
PUBLIC int init_sproc(struct sproc * sp, struct service_up_req * up_req, endpoint_t source);
PUBLIC int set_sproc(struct sproc * sp, struct service_up_req * up_req, endpoint_t source);
PUBLIC int free_sproc(struct sproc * sp);
PUBLIC int read_exec(struct sproc * sp);

PUBLIC int do_service_up(MESSAGE * msg);
PUBLIC int do_service_init_reply(MESSAGE * msg);
PUBLIC int serv_exec(endpoint_t target, char * exec, int exec_len, char * progname, char** argv);
PUBLIC int serv_prepare_module_stack();
PUBLIC int announce_service(char * name, endpoint_t serv_ep);

PUBLIC int late_reply(struct sproc * sp, int retval);

#endif
