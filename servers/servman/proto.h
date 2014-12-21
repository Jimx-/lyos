#ifndef _SERVMAN_PROTO_H_
#define _SERVMAN_PROTO_H_

PUBLIC int do_service_up(MESSAGE * msg);
PUBLIC int serv_exec(endpoint_t target, char * pathname);
PUBLIC int serv_prepare_module_stack();
PUBLIC int announce_service(char * name, endpoint_t serv_ep);

#endif
