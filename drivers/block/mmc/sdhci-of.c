#include <lyos/types.h>
#include <lyos/ipc.h>
#include <lyos/const.h>
#include <lyos/vm.h>
#include <sys/mman.h>

#include <libfdt/libfdt.h>
#include <libof/libof.h>

#include "sdhci.h"

struct sdhci_host* sdhci_of_init(const void* blob, unsigned long offset,
                                 const struct sdhci_host_ops* ops,
                                 size_t priv_size)
{
    struct sdhci_host* host;
    phys_bytes base, size;
    void* reg_base;
    int irq;
    int ret;

    ret = of_address_parse_one(blob, offset, 0, &base, &size);
    if (ret < 0) return NULL;

    reg_base = mm_map_phys(SELF, base, size, MMP_IO);
    if (reg_base == MAP_FAILED) return NULL;

    irq = irq_of_parse_and_map(blob, offset, 0);
    if (!irq) return NULL;

    host = sdhci_alloc_host(priv_size);
    if (!host) return NULL;

    host->ioaddr = reg_base;
    host->irq = irq;
    host->ops = ops;

    return host;
}
