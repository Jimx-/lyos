#ifndef _ASM_X86_LDT_H
#define _ASM_X86_LDT_H

struct user_desc {
    unsigned int entry_number;
    unsigned int base_addr;
};

#endif /* _ASM_X86_LDT_H */
