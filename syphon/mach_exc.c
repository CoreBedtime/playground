#include "syphon.h"

kern_return_t send_reply(mach_msg_header_t *req, kern_return_t ret) {
    /* In received messages, msgh_remote_port carries the reply port
     * (send-once right) and msgh_local_port carries the destination
     * port (our exception port).  Use msgh_remote_port for the reply. */
    exc_rep_t rep = {0};
    rep.head.msgh_bits = req->msgh_bits & MACH_MSGH_BITS_REMOTE_MASK;
    rep.head.msgh_size = sizeof(rep);
    rep.head.msgh_remote_port = req->msgh_remote_port;
    rep.head.msgh_local_port = MACH_PORT_NULL;
    rep.head.msgh_id = req->msgh_id + 100;
    rep.ndr = NDR_record;
    rep.ret = ret;
    return mach_msg(&rep.head, MACH_SEND_MSG, sizeof(rep), 0,
                    MACH_PORT_NULL, MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
}

void reset_exception_ports(void) {
    if (g_task != MACH_PORT_NULL)
        task_set_exception_ports(g_task, EXC_MASK_BREAKPOINT,
                                 MACH_PORT_NULL,
                                 EXCEPTION_DEFAULT | MACH_EXCEPTION_CODES,
                                 ARM_THREAD_STATE64);
    if (g_exc_port != MACH_PORT_NULL)
        mach_port_mod_refs(mach_task_self(), g_exc_port,
                           MACH_PORT_RIGHT_RECEIVE, -1);
}
