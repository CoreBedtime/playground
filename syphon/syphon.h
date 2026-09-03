#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <mach/exception_types.h>
#include <mach/task.h>
#include <mach/thread_status.h>
#include <sys/sysctl.h>
#include <dlfcn.h>
#include <limits.h>

extern task_t g_task;
extern mach_port_t g_exc_port;

#define MAX_TARGS 6

typedef struct {
    mach_vm_address_t addr;
    int path_reg;
    const char *name;
} target_t;

extern target_t g_targs[MAX_TARGS];
extern int g_ntarg;
void add_targ(void *func, int path_reg, const char *name);

#define MAX_STEPPING 64

typedef struct {
    mach_port_t thread;
    int brk_idx;
    int entry_idx;
    mach_vm_address_t ret_addr;
    uint64_t child_pid_addr;
} step_state_t;

extern step_state_t g_steps[MAX_STEPPING];
extern int g_nsteps;
int find_step(mach_port_t thread);
void add_step(mach_port_t thread, int brk_idx);
void remove_step(int idx);

#define HW_BRK_EN  ((0xFU << 5) | (0x2 << 1) | 0x1)
#define HW_BRK_DIS ((0xFU << 5) | (0x2 << 1) | 0x0)

void init_ds_template(void);
int install_hw_breakpoints(void);
int install_hw_breakpoints_in(task_t task);
int clear_hw_breakpoints(void);

typedef struct {
    mach_msg_header_t head;
    mach_msg_body_t body;
    mach_msg_port_descriptor_t thread;
    mach_msg_port_descriptor_t task;
    NDR_record_t ndr;
    exception_type_t exception;
    mach_msg_type_number_t codeCnt;
    mach_exception_data_type_t code[2];
} exc_req_t;

typedef struct {
    mach_msg_header_t head;
    NDR_record_t ndr;
    kern_return_t ret;
} exc_rep_t;

kern_return_t send_reply(mach_msg_header_t *req, kern_return_t ret);
void reset_exception_ports(void);

pid_t find_process(const char *name);
