#include "syphon.h"

pid_t find_process(const char *name) {
    int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_ALL, 0};
    size_t size = 0;
    if (sysctl(mib, 4, NULL, &size, NULL, 0) < 0) return -1;
    struct kinfo_proc *p = malloc(size);
    if (!p) return -1;
    if (sysctl(mib, 4, p, &size, NULL, 0) < 0) { free(p); return -1; }
    int n = (int)(size / sizeof(struct kinfo_proc));
    for (int i = 0; i < n; i++)
        if (strncmp(p[i].kp_proc.p_comm, name, MAXCOMLEN) == 0) {
            pid_t pid = p[i].kp_proc.p_pid;
            free(p);
            return pid;
        }
    free(p);
    return -1;
}
