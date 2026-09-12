/*  This file is part of Lyos.

    Lyos is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Lyos is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Lyos.  If not, see <http://www.gnu.org/licenses/>. */

/*
 * vmx-smoke: test program for the vmm server (servers/vmm).
 *
 * A normal user program: it talks to the vmm service over IPC
 * (lib/liblyos/vmm.c) and needs no privileges of its own. Embeds the
 * 32-bit protected mode guest payloads of guest.S and walks the check
 * matrix below, printing one PASS/FAIL/SKIP line per check and exiting
 * nonzero if any check failed.
 *
 * Run from the shell with the vmm service up:
 *     vmx-smoke
 */

#include <lyos/types.h>
#include <lyos/ipc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>
#include <lyos/const.h>
#include <lyos/vmm.h>

/* guest memory layout (GPAs), matches guest.S */
#define GUEST_CODE_BASE 0x1000
#define GUEST_MARKER    0x2000
#define GUEST_IO_IN     0x2004
#define GUEST_FPU_SRC0  0x2100
#define GUEST_FPU_SRC1  0x2110
#define GUEST_FPU_DST0  0x2120
#define GUEST_FPU_DST1  0x2130
#define GUEST_CPUID_EAX 0x2200
#define GUEST_CPUID_EDX 0x2204

#define GUEST_RAM_SIZE (4UL << 20)

#define MARKER1 0x600d0001
#define MARKER2 0x600d0002

/* payload offsets within the .guest32 section */
extern char __guest32_start[], __guest32_end[];
extern char vmx_guest_hlt[], vmx_guest_io[], vmx_guest_in[], vmx_guest_cpuid[],
    vmx_guest_ept[], vmx_guest_ept_fault[], vmx_guest_fpu[];

#define PAYLOAD(sym) ((unsigned long)((sym) - __guest32_start))

static int nr_pass, nr_fail, nr_skip;
static int any_fail;

static void report(const char* name, int ok, const char* fmt, ...)
{
    va_list ap;
    char detail[256];

    if (ok) {
        nr_pass++;
        printf("vmx-smoke: PASS %s\n", name);
        return;
    }

    nr_fail++;
    any_fail = 1;
    va_start(ap, fmt);
    vsnprintf(detail, sizeof(detail), fmt, ap);
    va_end(ap);
    printf("vmx-smoke: FAIL %s: %s\n", name, detail);
}

static void report_skip(const char* name, const char* why)
{
    nr_skip++;
    printf("vmx-smoke: SKIP %s: %s\n", name, why);
}

/*****************************************************************************
 *                        helpers
 *****************************************************************************/

static struct vmm_caps caps;
static u32 vm_id;

static int setup_vm(unsigned long rip)
{
    int retval;

    if ((retval = vmm_vm_create(GUEST_RAM_SIZE, rip, &vm_id)) != 0)
        return retval;

    return vmm_vm_write(vm_id, GUEST_CODE_BASE, __guest32_start,
                        __guest32_end - __guest32_start);
}

static int teardown_vm(void) { return vmm_vm_destroy(vm_id); }

static int read_mem(u64 gpa, void* buf, size_t len)
{
    return vmm_vm_read(vm_id, gpa, buf, len);
}

/*
 * Host-side x87/SSE arithmetic performed in assembly across guest runs;
 * userland is compiled with -mno-sse, so the SSE ops live in asm operands
 * the compiler cannot touch.
 */
static double host_x;
static const double host_c1 = 1.5;
static const double host_c2 = 0.25;

static void host_sse_step(void)
{
    /*
     * Userland is built with -mno-sse, so the compiler never allocates xmm
     * registers and the clobber need not (and cannot) be declared.
     */
    __asm__ __volatile__("movsd %[c1], %%xmm0\n\t"
                         "mulsd host_x(%%rip), %%xmm0\n\t"
                         "addsd %[c2], %%xmm0\n\t"
                         "movsd %%xmm0, host_x(%%rip)"
                         :
                         : [c1] "m"(host_c1), [c2] "m"(host_c2)
                         : "memory");
}

static void host_sse_expected(double* out, double x, int n)
{
    volatile double v = x, t;

    while (n-- > 0) {
        t = v * 1.5;
        v = t + 0.25;
    }
    *out = v;
}

/*****************************************************************************
 *                        checks
 *****************************************************************************/

static void check_entry_exit(void)
{
    struct vmm_run_status status = {.status = VMM_RUN_ERROR};
    unsigned int marker;
    int retval;

    if ((retval = setup_vm(GUEST_CODE_BASE + PAYLOAD(vmx_guest_hlt))) != 0) {
        report("entry-exit", 0, "setup failed: %d", retval);
        return;
    }

    retval = vmm_vm_run(vm_id, &status);
    if (retval != 0 || status.status != VMM_RUN_HALTED) {
        report("entry-exit", 0, "run retval=%d status=%u reason=%u", retval,
               status.status, status.exit_reason);
        teardown_vm();
        return;
    }

    if (status.pc <= GUEST_CODE_BASE + PAYLOAD(vmx_guest_hlt)) {
        report("entry-exit", 0, "pc %#llx not advanced", status.pc);
        teardown_vm();
        return;
    }

    if ((retval = read_mem(GUEST_MARKER, &marker, sizeof(marker))) != 0 ||
        marker != MARKER1) {
        report("entry-exit", 0, "marker not visible (retval=%d)", retval);
        teardown_vm();
        return;
    }

    report("entry-exit", 1, "");
}

static void check_resume(void)
{
    struct vmm_run_status status = {.status = VMM_RUN_ERROR};
    unsigned int marker;
    int retval;

    /* the server advanced rip past the first hlt; the second run
     * writes the second marker and halts again */
    if ((retval = vmm_vm_run(vm_id, &status)) != 0 ||
        status.status != VMM_RUN_HALTED) {
        report("resume", 0, "run retval=%d status=%u", retval, status.status);
        teardown_vm();
        return;
    }

    if ((retval = read_mem(GUEST_MARKER, &marker, sizeof(marker))) != 0 ||
        marker != MARKER2) {
        report("resume", 0, "second marker missing (retval=%d)", retval);
        teardown_vm();
        return;
    }

    report("resume", 1, "");
    teardown_vm();
}

static void check_io(void)
{
    struct vmm_run_status status = {.status = VMM_RUN_ERROR};
    unsigned int in_val;
    int retval;

    /* out to the debug console is emulated and the run continues */
    if ((retval = setup_vm(GUEST_CODE_BASE + PAYLOAD(vmx_guest_io))) != 0) {
        report("io", 0, "setup failed: %d", retval);
        return;
    }
    if ((retval = vmm_vm_run(vm_id, &status)) != 0 ||
        status.status != VMM_RUN_HALTED) {
        report("io", 0, "out run retval=%d status=%u reason=%u", retval,
               status.status, status.exit_reason);
        teardown_vm();
        return;
    }
    teardown_vm();

    /* input from an unattached port reads as all ones */
    if ((retval = setup_vm(GUEST_CODE_BASE + PAYLOAD(vmx_guest_in))) != 0) {
        report("io", 0, "in setup failed: %d", retval);
        return;
    }
    if ((retval = vmm_vm_run(vm_id, &status)) != 0 ||
        status.status != VMM_RUN_HALTED) {
        report("io", 0, "in run retval=%d status=%u reason=%u", retval,
               status.status, status.exit_reason);
        teardown_vm();
        return;
    }
    if ((retval = read_mem(GUEST_IO_IN, &in_val, sizeof(in_val))) != 0 ||
        in_val != 0xff) {
        report("io", 0, "emulated in value %#x (retval=%d)", in_val, retval);
        teardown_vm();
        return;
    }

    report("io", 1, "");
    teardown_vm();
}

static void check_cpuid(void)
{
    struct vmm_run_status status = {.status = VMM_RUN_ERROR};
    unsigned int eax, edx;
    int retval;

    if ((retval = setup_vm(GUEST_CODE_BASE + PAYLOAD(vmx_guest_cpuid))) != 0) {
        report("cpuid", 0, "setup failed: %d", retval);
        return;
    }

    if ((retval = vmm_vm_run(vm_id, &status)) != 0 ||
        status.status != VMM_RUN_HALTED) {
        report("cpuid", 0, "run retval=%d status=%u reason=%u", retval,
               status.status, status.exit_reason);
        teardown_vm();
        return;
    }

    if ((retval = read_mem(GUEST_CPUID_EAX, &eax, sizeof(eax))) != 0 ||
        (retval = read_mem(GUEST_CPUID_EDX, &edx, sizeof(edx))) != 0) {
        report("cpuid", 0, "read failed: %d", retval);
        teardown_vm();
        return;
    }

    /* constrained model: family 6, x87/SSE offered, no APIC */
    if (eax != 0x00000600 || !(edx & (1U << 0)) || !(edx & (1U << 25)) ||
        !(edx & (1U << 26)) || (edx & (1U << 9))) {
        report("cpuid", 0, "leaf 1: eax %#x edx %#x", eax, edx);
        teardown_vm();
        return;
    }

    report("cpuid", 1, "");
    teardown_vm();
}

static void check_ept_isolation(void)
{
    struct vmm_run_status status = {.status = VMM_RUN_ERROR};
    int retval;

    if ((retval = setup_vm(GUEST_CODE_BASE + PAYLOAD(vmx_guest_ept))) != 0) {
        report("ept-isolation", 0, "setup failed: %d", retval);
        return;
    }

    if ((retval = vmm_vm_run(vm_id, &status)) != 0 ||
        status.status != VMM_RUN_FAULTED ||
        status.exit_reason != VMM_EXIT_MEMORY_FAULT) {
        report("ept-isolation", 0, "run retval=%d status=%u reason=%u", retval,
               status.status, status.exit_reason);
        teardown_vm();
        return;
    }

    if (status.pc != GUEST_CODE_BASE + PAYLOAD(vmx_guest_ept_fault)) {
        report("ept-isolation", 0, "bad faulting PC %#llx", status.pc);
        teardown_vm();
        return;
    }

    report("ept-isolation", 1, "");
    teardown_vm();
}

static void check_fpu_separation(void)
{
    static const unsigned char pattern_a[16] = {
        0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7,
        0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf};
    struct vmm_run_status status = {.status = VMM_RUN_ERROR};
    unsigned char dst[32];
    int retval;

    if ((retval = setup_vm(GUEST_CODE_BASE + PAYLOAD(vmx_guest_fpu))) != 0) {
        report("fpu-separation", 0, "setup failed: %d", retval);
        return;
    }

    if ((retval = vmm_vm_write(vm_id, GUEST_FPU_SRC0, pattern_a, 16)) != 0 ||
        (retval = vmm_vm_write(vm_id, GUEST_FPU_SRC1, pattern_a, 16)) != 0) {
        report("fpu-separation", 0, "pattern write failed: %d", retval);
        teardown_vm();
        return;
    }
    memset(dst, 0, sizeof(dst));
    if ((retval = vmm_vm_write(vm_id, GUEST_FPU_DST0, dst, sizeof(dst))) != 0) {
        report("fpu-separation", 0, "target clear failed: %d", retval);
        teardown_vm();
        return;
    }

    /* guest loads its SSE state and halts; host does SSE work meanwhile */
    if ((retval = vmm_vm_run(vm_id, &status)) != 0 ||
        status.status != VMM_RUN_HALTED) {
        report("fpu-separation", 0, "first run retval=%d status=%u", retval,
               status.status);
        teardown_vm();
        return;
    }

    host_x = 1.0;
    host_sse_step();

    /* guest stores its xmm registers back and halts again */
    if ((retval = vmm_vm_run(vm_id, &status)) != 0 ||
        status.status != VMM_RUN_HALTED) {
        report("fpu-separation", 0, "second run retval=%d status=%u", retval,
               status.status);
        teardown_vm();
        return;
    }

    if ((retval = read_mem(GUEST_FPU_DST0, dst, sizeof(dst))) != 0 ||
        memcmp(dst, pattern_a, 16) != 0 ||
        memcmp(dst + 16, pattern_a, 16) != 0) {
        report("fpu-separation", 0, "guest xmm state not preserved");
        teardown_vm();
        return;
    }

    host_sse_step();
    {
        double expected;
        host_sse_expected(&expected, 1.0, 2);
        if (host_x != expected) {
            report("fpu-separation", 0, "host SSE state corrupted (%f)",
                   host_x);
            teardown_vm();
            return;
        }
    }

    report("fpu-separation", 1, "");
    teardown_vm();
}

static void check_cleanup(void)
{
    struct vmm_run_status status = {.status = VMM_RUN_ERROR};
    struct vmm_caps now;
    u32 stale_id = 0;
    int i, retval;

    for (i = 0; i < 3; i++) {
        if ((retval = setup_vm(GUEST_CODE_BASE + PAYLOAD(vmx_guest_hlt))) !=
            0) {
            report("cleanup", 0, "iteration %d setup failed: %d", i, retval);
            return;
        }
        if (i == 0) stale_id = vm_id;
        if (vmm_vm_run(vm_id, &status) != 0 ||
            status.status != VMM_RUN_HALTED) {
            report("cleanup", 0, "iteration %d run failed", i);
            teardown_vm();
            return;
        }
        if ((retval = teardown_vm()) != 0) {
            report("cleanup", 0, "iteration %d teardown failed: %d", i, retval);
            return;
        }
    }

    /* failed creations must be rejected without leaking VMs */
    {
        u32 id;

        if (vmm_vm_create(VMM_MEM_MAX * 2, GUEST_CODE_BASE, &id) == 0) {
            report("cleanup", 0, "oversized memory accepted");
            return;
        }
        if (vmm_vm_create(GUEST_RAM_SIZE, VMM_MEM_MAX, &id) == 0) {
            report("cleanup", 0, "out-of-range entry accepted");
            return;
        }
    }

    if ((retval = vmm_query(&now)) != 0 || now.nr_vms != 0) {
        report("cleanup", 0, "server leaks VMs (retval=%d nr_vms=%u)", retval,
               now.nr_vms);
        return;
    }

    /* stale and bogus ids must be rejected (server ids are never reused) */
    if (stale_id != 0 && vmm_vm_destroy(stale_id) == 0) {
        report("cleanup", 0, "stale id accepted");
        return;
    }
    if (vmm_vm_destroy(0x7fffffff) == 0) {
        report("cleanup", 0, "bogus id accepted");
        return;
    }

    report("cleanup", 1, "");
}

/*****************************************************************************
 *                        main
 *****************************************************************************/

static const char* all_checks[] = {
    "caps", "entry-exit",    "resume",         "cpuid",
    "io",   "ept-isolation", "fpu-separation", "cleanup",
};

int main(void)
{
    int retval;

    if ((retval = vmm_query(&caps)) != 0) {
        if (retval == EPROTONOSUPPORT) {
            printf("vmx-smoke: FAIL caps: VMM ABI mismatch (client %u, "
                   "server %u); rebuild and deploy both binaries\n",
                   VMM_ABI_VERSION, caps.abi_version);
            return 1;
        }
        printf("vmx-smoke: FAIL caps: cannot reach the vmm server: %d\n",
               retval);
        return 1;
    }

    printf("vmx-smoke: vmm abi %u caps 0x%x\n", caps.abi_version, caps.caps);

    if (!(caps.caps & HV_CAP_ACTIVE) || !(caps.caps & HV_CAP_STAGE2)) {
        size_t i;
        for (i = 0; i < sizeof(all_checks) / sizeof(all_checks[0]); i++)
            report_skip(all_checks[i], "VT-x with EPT not available");
        printf("vmx-smoke: %d pass, %d fail, %d skip\n", nr_pass, nr_fail,
               nr_skip);
        return 0;
    }

    check_entry_exit();
    if (nr_fail == 0) {
        check_resume();
        check_cpuid();
        check_io();
        check_ept_isolation();
        check_fpu_separation();
        check_cleanup();
    }

    printf("vmx-smoke: %d pass, %d fail, %d skip\n", nr_pass, nr_fail, nr_skip);
    return any_fail ? 1 : 0;
}
