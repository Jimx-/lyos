#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <lyos/const.h>
#include <lyos/types.h>
#include <lyos/service.h>
#include <sys/socket.h>

#include <cjson/cJSON.h>

#define CLASS             "class"
#define PCI_CLASS         "pci_class"
#define PCI_DEVICE        "pci_device"
#define DOMAIN            "domain"
#define SYSCALL           "syscall"
#define ALLOW_PROXY_GRANT "allow_proxy_grant"

#define SYSCALL_BASIC "basic"
#define SYSCALL_ALL   "all"
#define SYSCALL_NONE  "none"

static int parse_class(cJSON* root, struct service_up_req* up_req)
{
    cJSON* _class = cJSON_GetObjectItem(root, CLASS);
    if (!_class) return 0;

    if (_class->type != cJSON_String) return EINVAL;

    up_req->class = strdup(_class->valuestring);
    up_req->classlen = strlen(up_req->class);

    return 0;
}

static void parse_pci_class(cJSON* root, struct service_up_req* up_req)
{
    up_req->nr_pci_class = 0;

    cJSON* classes = cJSON_GetObjectItem(root, PCI_CLASS);
    if (!classes) return;

    int i;
    cJSON* _class;
    for (i = 0; i < cJSON_GetArraySize(classes); i++) {
        _class = cJSON_GetArrayItem(classes, i);

        if (_class->type != cJSON_String) continue;

        char* str = _class->valuestring;
        char* check;

        unsigned char base_class = strtoul(str, &check, 0x10);
        unsigned char sub_class = 0, interface = 0;
        unsigned int mask = 0xff0000;

        if (*check == '/') {
            sub_class = strtoul(check + 1, &check, 0x10);
            mask = 0xffff00;
            if (*check == '/') {
                interface = strtoul(check + 1, &check, 0x10);
                mask = 0xffffff;
            }
        }

        if (*check != '\0') continue;

        up_req->pci_class[up_req->nr_pci_class].classid =
            (base_class << 16) | (sub_class << 8) | interface;
        up_req->pci_class[up_req->nr_pci_class].mask = mask;
        up_req->nr_pci_class++;
    }
}

static void parse_pci_device(cJSON* root, struct service_up_req* up_req)
{
    up_req->nr_pci_id = 0;

    cJSON* devices = cJSON_GetObjectItem(root, PCI_DEVICE);
    if (!devices) return;

    int i;
    cJSON* device;
    for (i = 0; i < cJSON_GetArraySize(devices); i++) {
        device = cJSON_GetArrayItem(devices, i);

        if (device->type != cJSON_String) continue;

        char* str = device->valuestring;
        char* check;

        u16 vid = strtoul(str, &check, 0x10);
        u16 did = 0;

        if (*check == ':') {
            did = strtoul(check + 1, &check, 0x10);
        }

        if (*check != '\0') continue;

        up_req->pci_id[up_req->nr_pci_id].vid = vid;
        up_req->pci_id[up_req->nr_pci_id].did = did;
        up_req->nr_pci_id++;
    }
}

static struct {
    char* name;
    int call_nr;
} system_tab[] = {
    {"printx", NR_PRINTX},
    {"sendrec", NR_SENDREC},
    {"datacopy", NR_DATACOPY},
    {"privctl", NR_PRIVCTL},
    {"getinfo", NR_GETINFO},
    {"vmctl", NR_VMCTL},
    {"umap", NR_UMAP},
    {"portio", NR_PORTIO},
    {"vportio", NR_VPORTIO},
    {"sportio", NR_SPORTIO},
    {"irqctl", NR_IRQCTL},
    {"fork", NR_FORK},
    {"clear", NR_CLEAR},
    {"exec", NR_EXEC},
    {"sigsend", NR_SIGSEND},
    {"sigreturn", NR_SIGRETURN},
    {"kill", NR_KILL},
    {"getksig", NR_GETKSIG},
    {"endksig", NR_ENDKSIG},
    {"times", NR_TIMES},
    {"trace", NR_TRACE},
    {"alarm", NR_ALARM},
    {"kprofile", NR_KPROFILE},
    {"setgrant", NR_SETGRANT},
    {"safecopyfrom", NR_SAFECOPYFROM},
    {"safecopyto", NR_SAFECOPYTO},
    {"stime", NR_STIME},
    {NULL, 0},
};

static int parse_syscall(cJSON* root, struct service_up_req* up_req)
{
    cJSON* syscalls = cJSON_GetObjectItem(root, SYSCALL);
    if (!syscalls) return 0;

    int i, j;
    cJSON* syscall;
    if (syscalls->type == cJSON_Array) {
        for (i = 0; i < cJSON_GetArraySize(syscalls); i++) {
            syscall = cJSON_GetArrayItem(syscalls, i);
            if (syscall->type != cJSON_String) return EINVAL;

            char* str = syscall->valuestring;

            for (j = 0; system_tab[j].name; j++) {
                if (!strcmp(str, system_tab[j].name)) {
                    break;
                }
            }

            if (!system_tab[j].name) {
                fprintf(stderr, "service: unknown system call name %s\n", str);
                return EINVAL;
            } else {
                SET_BIT(up_req->syscall_mask, system_tab[j].call_nr);
            }
        }
    } else if (syscalls->type == cJSON_String) {
        char* str = syscalls->valuestring;

        if (!strcmp(str, SYSCALL_BASIC)) {
        } else if (!strcmp(str, SYSCALL_NONE)) {
            up_req->flags &= ~SUR_BASIC_SYSCALLS;
        } else if (!strcmp(str, SYSCALL_ALL)) {
            for (i = 0; i < NR_SYS_CALLS; i++) {
                SET_BIT(up_req->syscall_mask, i);
            }
        } else {
            fprintf(stderr, "service: unknown system call keyword %s\n", str);
            return EINVAL;
        }
    } else {
        fprintf(stderr, "service: unknown system call value type\n");
        return EINVAL;
    }

    return 0;
}

static int parse_grant(cJSON* root, struct service_up_req* up_req)
{
    cJSON* allow_proxy_grant = cJSON_GetObjectItem(root, ALLOW_PROXY_GRANT);
    if (!allow_proxy_grant) return 0;

    if (cJSON_IsTrue(allow_proxy_grant)) up_req->flags |= SUR_ALLOW_PROXY_GRANT;

    return 0;
}

static const struct {
    const char* name;
    int domain;
} domain_tab[] = {
    {"LOCAL", PF_LOCAL},
    {"UNIX", PF_LOCAL},
    {"FILE", PF_LOCAL},
    {"INET", PF_INET},
};

static int parse_domain(cJSON* root, struct service_up_req* up_req)
{
    up_req->nr_domain = 0;

    cJSON* domains = cJSON_GetObjectItem(root, DOMAIN);
    if (!domains) return 0;

    int i, j;
    cJSON* domain_obj;
    int domain;
    for (i = 0; i < cJSON_GetArraySize(domains); i++) {
        domain_obj = cJSON_GetArrayItem(domains, i);
        if (domain_obj->type != cJSON_String) continue;

        char* str = domain_obj->valuestring;

        for (j = 0; j < sizeof(domain_tab) / sizeof(domain_tab[0]); j++) {
            if (!strcmp(str, domain_tab[j].name)) {
                break;
            }
        }

        if (j == sizeof(domain_tab) / sizeof(domain_tab[0])) {
            domain = atoi(str);
        } else {
            domain = domain_tab[j].domain;
        }

        if (domain <= 0 || domain >= PF_MAX) {
            fprintf(stderr, "service: invalid domain %d\n", domain);
            return EINVAL;
        }

        up_req->domain[up_req->nr_domain++] = domain;
    }

    return 0;
}

int parse_config(char* progname, char* path, struct service_up_req* up_req)
{
    struct stat stat_buf;
    int r;
    if ((r = stat(path, &stat_buf)) != 0) return r;

    char* buf = (char*)malloc(stat_buf.st_size);
    if (!buf) return ENOMEM;

    int fd = open(path, O_RDONLY);
    read(fd, buf, stat_buf.st_size);
    close(fd);

    cJSON* root = cJSON_Parse(buf);
    if (!root) {
        free(buf);
        return EINVAL;
    }

    up_req->flags = SUR_BASIC_SYSCALLS;

    r = parse_class(root, up_req);
    if (r) return r;

    parse_pci_class(root, up_req);
    parse_pci_device(root, up_req);

    r = parse_syscall(root, up_req);
    if (r) return r;

    r = parse_grant(root, up_req);
    if (r) return r;

    r = parse_domain(root, up_req);
    if (r) return r;

    cJSON_Delete(root);
    free(buf);

    return 0;
}
