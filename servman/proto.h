#ifndef _SERVMAN_PROTO_H_
#define _SERVMAN_PROTO_H_

PUBLIC int do_service_up(MESSAGE * msg);
PUBLIC int serv_exec(endpoint_t target, char * pathname);
PUBLIC int serv_spawn_module(endpoint_t target, char * mod_base, u32 mod_len);
PUBLIC int spawn_boot_modules();

#endif
