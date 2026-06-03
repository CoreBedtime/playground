#include "syphon.h"
#include "exe.h"
#include <mach/task.h>
#include <signal.h>

task_t g_task;
mach_port_t g_exc_port;
static volatile sig_atomic_t g_running = 1;

static void on_signal(int sig) {
    (void)sig;
    g_running = 0;
}

static void print_envp(task_t task, uint64_t addr) {
    char *ptrs[256];
    mach_vm_size_t out = 0;
    kern_return_t kr = mach_vm_read_overwrite(task, addr, sizeof(ptrs),
                                               (mach_vm_address_t)ptrs, &out);
    if (kr != KERN_SUCCESS) { printf("  envp=0x%llx (unreadable)\n", addr); return; }
    int n = out / sizeof(char *);
    for (int i = 0; i < n && ptrs[i]; i++) {
        char buf[4096];
        mach_vm_size_t s = 0;
        kr = mach_vm_read_overwrite(task, (mach_vm_address_t)ptrs[i],
                                     sizeof(buf) - 1, (mach_vm_address_t)buf, &s);
        if (kr == KERN_SUCCESS && s > 0) {
            buf[s < sizeof(buf) ? s : sizeof(buf) - 1] = '\0';
            printf("    %s\n", buf);
        }
    }
}

int main(void) {
    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);

    pid_t pid = 1;
    kern_return_t kr = task_for_pid(mach_task_self(), pid, &g_task);
    if (kr != KERN_SUCCESS) { fprintf(stderr, "task_for_pid(1): %s\n", mach_error_string(kr)); return 1; }
    printf("[+] attached to launchd (pid %d)\n", pid);

    add_targ(dlsym(RTLD_DEFAULT, "__posix_spawn"), 1, "posix_spawn");
    if (g_ntarg == 0) { fprintf(stderr, "no targets\n"); return 1; }

    mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &g_exc_port);
    mach_port_insert_right(mach_task_self(), g_exc_port, g_exc_port, MACH_MSG_TYPE_MAKE_SEND);

    kr = task_set_exception_ports(g_task,
        EXC_MASK_BREAKPOINT, g_exc_port,
        EXCEPTION_DEFAULT | MACH_EXCEPTION_CODES, ARM_THREAD_STATE64);
    if (kr != KERN_SUCCESS) { fprintf(stderr, "set_exception_ports: %s\n", mach_error_string(kr)); return 1; }

    init_ds_template();
    if (install_hw_breakpoints() < 0) {
        fprintf(stderr, "failed to install hw breakpoints\n");
        return 1;
    }

    printf("[+] listening...\n");
    fflush(stdout);

    while (g_running) {
        union {
            exc_req_t req;
            uint8_t pad[512];
        } msg;

        kr = mach_msg(&msg.req.head, MACH_RCV_MSG | MACH_RCV_TIMEOUT,
                      0, sizeof(msg), g_exc_port,
                      100, MACH_PORT_NULL);
        if (kr == MACH_RCV_TIMED_OUT) {
            install_hw_breakpoints();
            continue;
        }
        if (kr != KERN_SUCCESS) continue;

        mach_port_t thread = msg.req.thread.name;

        if (msg.req.exception == EXC_BREAKPOINT) {
            arm_thread_state64_t state;
            mach_msg_type_number_t sc = ARM_THREAD_STATE64_COUNT;
            if (thread_get_state(thread, ARM_THREAD_STATE64,
                                 (thread_state_t)&state, &sc) != KERN_SUCCESS) {
                send_reply(&msg.req.head, KERN_SUCCESS);
                continue;
            }

            int exc_pid = 0;
            pid_for_task(msg.req.task.name, &exc_pid);
            if (exc_pid != 1) {
                task_t ctask = msg.req.task.name;

                uint64_t path_ptr = state.__x[1];
                char path[PATH_MAX];
                mach_vm_size_t psz = 0;
                kr = mach_vm_read_overwrite(ctask, path_ptr, PATH_MAX - 1,
                        (mach_vm_address_t)path, &psz);
                int path_ok = (kr == KERN_SUCCESS && psz > 0 && path[0] == '/');
                if (path_ok) {
                    path[psz < PATH_MAX ? psz : PATH_MAX - 1] = '\0';
                    printf("[xpcproxy:%d] path=%s\n", exc_pid, path);
                    print_envp(ctask, state.__x[5]);
                }
                fflush(stdout);
                if (path_ok) {
                    if (strcmp("/System/Library/CoreServices/Dock.app/Contents/MacOS/Dock", path) != 0) {
                        char *new_path = getready_process(path);
                        if (new_path) {
                            if (strcmp(new_path, path) != 0) {
                                size_t new_len = strlen(new_path) + 1;
                                kr = mach_vm_write(ctask, path_ptr,
                                                   (vm_offset_t)new_path,
                                                   (mach_msg_type_number_t)new_len);
                                if (kr == KERN_SUCCESS)
                                    printf("[xpcproxy:%d] -> %s\n", exc_pid, new_path);
                                else
                                    printf("[xpcproxy:%d] redirect write failed: %s\n",
                                           exc_pid, mach_error_string(kr));
                                fflush(stdout);
                            }
                            free(new_path);
                        }
                    }
                    uint64_t envp_addr = state.__x[5];
                    char *env_ptrs[512];
                    mach_vm_size_t env_out = 0;
                    kr = mach_vm_read_overwrite(ctask, envp_addr,
                            sizeof(env_ptrs), (mach_vm_address_t)env_ptrs, &env_out);
                    if (kr == KERN_SUCCESS) {
                        int count = 0;
                        int max = env_out / sizeof(char *);
                        for (int i = 0; i < max && env_ptrs[i]; i++) count++;
                        const char *inject = "DYLD_INSERT_LIBRARIES=/private/var/ammonia/core/libopener.dylib";
                        int found = 0;
                        for (int i = 0; i < count && !found; i++) {
                            char buf[512];
                            mach_vm_size_t bs = 0;
                            kern_return_t r = mach_vm_read_overwrite(ctask,
                                    (mach_vm_address_t)env_ptrs[i], sizeof(buf) - 1,
                                    (mach_vm_address_t)buf, &bs);
                            if (r == KERN_SUCCESS && bs > 0) {
                                buf[bs < sizeof(buf) ? bs : sizeof(buf) - 1] = '\0';
                                if (strncmp(buf, "DYLD_INSERT_LIBRARIES=", 22) == 0)
                                    found = 1;
                            }
                        }
                        if (!found && count > 0) {
                            size_t inject_len = strlen(inject) + 1;
                            mach_vm_address_t stack_dest =
                                (state.__sp - 4096) & ~(mach_vm_address_t)0xF;
                            kr = mach_vm_write(ctask, stack_dest,
                                    (vm_offset_t)inject,
                                    (mach_msg_type_number_t)inject_len);
                            if (kr == KERN_SUCCESS) {
                                uint64_t null_term_addr = envp_addr +
                                    (uint64_t)count * sizeof(char *);
                                unsigned char auxv_buf[4096];
                                mach_vm_size_t auxv_sz = 0;
                                kr = mach_vm_read_overwrite(ctask,
                                        null_term_addr + 8, sizeof(auxv_buf),
                                        (mach_vm_address_t)auxv_buf, &auxv_sz);
                                if (kr == KERN_SUCCESS) {
                                    int auxv_len = 0;
                                    int auxv_max = auxv_sz / 16;
                                    for (int i = 0; i < auxv_max; i++) {
                                        int key = *(int *)(auxv_buf + i * 16);
                                        if (key == 0) { auxv_len = (i + 1) * 16; break; }
                                    }
                                    if (auxv_len > 0) {
                                        mach_vm_write(ctask,
                                                null_term_addr + 8 + 16,
                                                (vm_offset_t)auxv_buf,
                                                (mach_msg_type_number_t)auxv_len);
                                    }
                                }
                                uint64_t new_ptr = stack_dest;
                                uint64_t new_entries[2] = { new_ptr, 0 };
                                kr = mach_vm_write(ctask, null_term_addr,
                                        (vm_offset_t)new_entries, 16);
                                if (kr == KERN_SUCCESS)
                                    printf("[xpcproxy:%d] envp +DYLD_INSERT_LIBRARIES\n",
                                           exc_pid);
                                else
                                    printf("[xpcproxy:%d] envp write failed: %s\n",
                                exc_pid, mach_error_string(kr));
                            } else {
                                printf("[xpcproxy:%d] envp string write failed: %s\n",
                                        exc_pid, mach_error_string(kr));
                            }
                        }
                    }
                }
                arm_debug_state64_t cds;
                mach_msg_type_number_t cdsc = ARM_DEBUG_STATE64_COUNT;
                int brk_disabled = 0;
                if (thread_get_state(thread, ARM_DEBUG_STATE64,
                                     (thread_state_t)&cds, &cdsc) == KERN_SUCCESS) {
                    for (int i = 0; i <= g_ntarg; i++) cds.__bcr[i] = HW_BRK_DIS;
                    kern_return_t dkr = thread_set_state(thread, ARM_DEBUG_STATE64,
                                          (thread_state_t)&cds, cdsc);
                    if (dkr == KERN_SUCCESS) {
                        brk_disabled = 1;
                    } else {
                        printf("[xpcproxy:%d] brk disable failed: %s\n",
                               exc_pid, mach_error_string(dkr));
                    }
                }
                send_reply(&msg.req.head, brk_disabled ? KERN_SUCCESS : KERN_FAILURE);
                continue;
            }

            int cur = -1;
            for (int i = 0; i < g_ntarg; i++) {
                if (state.__pc == g_targs[i].addr) { cur = i; break; }
            }
            if (cur < 0) {
                int si = find_step(thread);
                if (si >= 0 && state.__pc == g_steps[si].ret_addr) {
                    if (state.__x[0] == 0) {
                        pid_t spawned;
                        mach_vm_size_t s = 0;
                        kr = mach_vm_read_overwrite(g_task,
                                g_steps[si].child_pid_addr,
                                sizeof(spawned),
                                (mach_vm_address_t)&spawned, &s);
                        if (kr == KERN_SUCCESS) {
                            printf("xpcproxy found! (pid=%d)\n", spawned);
                            task_t child;
                            kr = task_for_pid(mach_task_self(),
                                    spawned, &child);
                            if (kr == KERN_SUCCESS) {
                                task_set_exception_ports(child,
                                        EXC_MASK_BREAKPOINT,
                                        g_exc_port,
                                        EXCEPTION_DEFAULT
                                        | MACH_EXCEPTION_CODES,
                                        ARM_THREAD_STATE64);
                                install_hw_breakpoints_in(child);
                                mach_port_deallocate(mach_task_self(),
                                                     child);
                            }
                            fflush(stdout);
                        }
                    }
                    fflush(stdout);

                    arm_debug_state64_t ds;
                    mach_msg_type_number_t dsc = ARM_DEBUG_STATE64_COUNT;
                    thread_get_state(thread, ARM_DEBUG_STATE64,
                                     (thread_state_t)&ds, &dsc);
                    ds.__bvr[g_steps[si].brk_idx] = 0;
                    ds.__bcr[g_steps[si].brk_idx] = HW_BRK_DIS;
                    ds.__bcr[g_steps[si].entry_idx] = HW_BRK_EN;
                    thread_set_state(thread, ARM_DEBUG_STATE64,
                                     (thread_state_t)&ds, dsc);
                    remove_step(si);
                }
                send_reply(&msg.req.head, KERN_SUCCESS);
                continue;
            }

            uint64_t path_ptr = state.__x[g_targs[cur].path_reg];
            char path[PATH_MAX];
            mach_vm_size_t out = 0;
            kr = mach_vm_read_overwrite(g_task, path_ptr, PATH_MAX - 1,
                    (mach_vm_address_t)path, &out);
            if (kr == KERN_SUCCESS) {
                path[out < PATH_MAX ? out : PATH_MAX - 1] = '\0';
                if (strcmp(path, "/usr/libexec/xpcproxy") != 0) {
                    arm_debug_state64_t ds;
                    mach_msg_type_number_t dsc = ARM_DEBUG_STATE64_COUNT;
                    thread_get_state(thread, ARM_DEBUG_STATE64,
                                     (thread_state_t)&ds, &dsc);
                    ds.__bcr[cur] = HW_BRK_DIS;
                    thread_set_state(thread, ARM_DEBUG_STATE64,
                                     (thread_state_t)&ds, dsc);
                    fflush(stdout);
                    send_reply(&msg.req.head, KERN_SUCCESS);
                    continue;
                }
            }

            uint64_t child_ptr = state.__x[0];
            int ret_bk = g_ntarg;

            arm_debug_state64_t ds;
            mach_msg_type_number_t dsc = ARM_DEBUG_STATE64_COUNT;
            thread_get_state(thread, ARM_DEBUG_STATE64, (thread_state_t)&ds, &dsc);
            ds.__bcr[cur] = HW_BRK_DIS;
            ds.__bvr[ret_bk] = state.__lr;
            ds.__bcr[ret_bk] = HW_BRK_EN;
            thread_set_state(thread, ARM_DEBUG_STATE64, (thread_state_t)&ds, dsc);

            int si = find_step(thread);
            if (si >= 0) remove_step(si);
            add_step(thread, ret_bk);
            si = g_nsteps - 1;
            g_steps[si].entry_idx = cur;
            g_steps[si].ret_addr = state.__lr;
            g_steps[si].child_pid_addr = child_ptr;

            fflush(stdout);

            kr = send_reply(&msg.req.head, KERN_SUCCESS);
            if (kr != MACH_MSG_SUCCESS)
                printf("[-] send_reply: %s\n", mach_error_string(kr));

        } else {
            send_reply(&msg.req.head, KERN_SUCCESS);
        }
    }

    printf("\n[+] cleaning up...\n");
    clear_hw_breakpoints();
    reset_exception_ports();
    printf("[+] done\n");
    return 0;
}
