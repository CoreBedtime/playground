#include "syphon.h"

static arm_debug_state64_t g_ds_template;

void init_ds_template(void) {
    memset(&g_ds_template, 0, sizeof(g_ds_template));
    for (int i = 0; i < g_ntarg; i++) {
        g_ds_template.__bvr[i] = g_targs[i].addr;
        g_ds_template.__bcr[i] = HW_BRK_EN;
    }
    g_ds_template.__bvr[g_ntarg] = (uint64_t)-1;
    g_ds_template.__bcr[g_ntarg] = HW_BRK_EN;
}

int install_hw_breakpoints(void) {
    thread_act_array_t threads;
    mach_msg_type_number_t count;
    kern_return_t kr = task_threads(g_task, &threads, &count);
    if (kr != KERN_SUCCESS) return -1;

    int ok = 0;
    for (unsigned i = 0; i < count; i++) {
        if (find_step(threads[i]) >= 0) {
            mach_port_deallocate(mach_task_self(), threads[i]);
            continue;
        }
        mach_msg_type_number_t sc = ARM_DEBUG_STATE64_COUNT;
        kr = thread_set_state(threads[i], ARM_DEBUG_STATE64,
                              (thread_state_t)&g_ds_template, sc);
        if (kr == KERN_SUCCESS) ok++;
        mach_port_deallocate(mach_task_self(), threads[i]);
    }
    vm_deallocate(mach_task_self(), (vm_address_t)threads,
                  count * sizeof(thread_act_t));
    return ok > 0 ? 0 : -1;
}

int install_hw_breakpoints_in(task_t task) {
    thread_act_array_t threads;
    mach_msg_type_number_t count;
    kern_return_t kr = task_threads(task, &threads, &count);
    if (kr != KERN_SUCCESS) return -1;

    int ok = 0;
    for (unsigned i = 0; i < count; i++) {
        mach_msg_type_number_t sc = ARM_DEBUG_STATE64_COUNT;
        kr = thread_set_state(threads[i], ARM_DEBUG_STATE64,
                              (thread_state_t)&g_ds_template, sc);
        if (kr == KERN_SUCCESS) ok++;
        mach_port_deallocate(mach_task_self(), threads[i]);
    }
    vm_deallocate(mach_task_self(), (vm_address_t)threads,
                  count * sizeof(thread_act_t));
    return ok > 0 ? 0 : -1;
}

int clear_hw_breakpoints(void) {
    if (g_task == MACH_PORT_NULL) return -1;
    thread_act_array_t threads;
    mach_msg_type_number_t count;
    kern_return_t kr = task_threads(g_task, &threads, &count);
    if (kr != KERN_SUCCESS) return -1;

    arm_debug_state64_t zero;
    memset(&zero, 0, sizeof(zero));

    for (unsigned i = 0; i < count; i++) {
        thread_set_state(threads[i], ARM_DEBUG_STATE64,
                         (thread_state_t)&zero, ARM_DEBUG_STATE64_COUNT);
        mach_port_deallocate(mach_task_self(), threads[i]);
    }
    vm_deallocate(mach_task_self(), (vm_address_t)threads,
                  count * sizeof(thread_act_t));
    return 0;
}
