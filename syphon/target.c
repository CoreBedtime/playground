#include "syphon.h"

target_t g_targs[MAX_TARGS];
int g_ntarg;

void add_targ(void *func, int path_reg, const char *name) {
    if (!func || g_ntarg >= MAX_TARGS) return;
    g_targs[g_ntarg].addr = (mach_vm_address_t)func;
    g_targs[g_ntarg].path_reg = path_reg;
    g_targs[g_ntarg].name = name;
    printf("[+] %s @ 0x%llx\n", name, g_targs[g_ntarg].addr);
    g_ntarg++;
}
