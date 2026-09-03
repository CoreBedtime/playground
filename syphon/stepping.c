#include "syphon.h"

step_state_t g_steps[MAX_STEPPING];
int g_nsteps;

int find_step(mach_port_t thread) {
    for (int i = 0; i < g_nsteps; i++)
        if (g_steps[i].thread == thread) return i;
    return -1;
}

void add_step(mach_port_t thread, int brk_idx) {
    if (g_nsteps < MAX_STEPPING) {
        g_steps[g_nsteps].thread = thread;
        g_steps[g_nsteps].brk_idx = brk_idx;
        g_nsteps++;
    }
}

void remove_step(int idx) {
    g_steps[idx] = g_steps[--g_nsteps];
}
