/*
 * Copyright 2020, Data61
 * Commonwealth Scientific and Industrial Research Organisation (CSIRO)
 * ABN 41 687 119 230.
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(DATA61_BSD)
 */

#include <platsupport/io.h>
#include <platsupport/driver_module.h>
#include <camkes.h>
#include <camkes/io.h>
#include <utils/util.h>
#include <camkes/tls.h>
#include <camkes/irq.h>
#include <sel4bench/sel4bench.h>
/*- from 'global-endpoint.template.c' import allocate_cap_instance, allocate_rpc_cap_instance with context -*/

#define NUM_TRACES 10

uint64_t trace_point_start[NUM_TRACES];
uint64_t trace_point_end[NUM_TRACES];
uint64_t trace_point_sum[NUM_TRACES];
uint64_t trace_point_count[NUM_TRACES];

#define TRACE_START(num) do { \
        trace_point_start[num] = sel4bench_get_cycle_count(); \
} while (0)

#define TRACE_END(num) do { \
        trace_point_sum[num] += sel4bench_get_cycle_count() - trace_point_start[num]; \
        trace_point_count[num] ++; \
} while (0)



/* Force the init sections to be created even if no modules are defined. */
static USED SECTION("_env_init") struct {} dummy_env_init_module;
static USED SECTION("_pre_init") struct {} dummy_pre_init_module;
static USED SECTION("_post_init") struct {} dummy_post_init_module;
/* Definitions so that we can find the list of hardware modules that we need to initialise */

extern camkes_module_init_fn_t __start__env_init[];
extern camkes_module_init_fn_t __stop__env_init[];
extern camkes_module_init_fn_t __start__pre_init[];
extern camkes_module_init_fn_t __stop__pre_init[];
extern camkes_module_init_fn_t __start__post_init[];
extern camkes_module_init_fn_t __stop__post_init[];

static ps_io_ops_t io_ops;


void pre_init(void) {
    int err = camkes_io_ops(&io_ops);
    if (err) {
        ZF_LOGE("Failed to initialize io_ops");
    }
    for (camkes_module_init_fn_t * init_fun = __start__env_init; init_fun < __stop__env_init; init_fun++) {
        err = (*init_fun)(&io_ops);
        if (err) {
            ZF_LOGE("Failed to initialize %p", init_fun);
        }

    }
    for (camkes_module_init_fn_t * init_fun = __start__pre_init; init_fun < __stop__pre_init; init_fun++) {
        err = (*init_fun)(&io_ops);
        if (err) {
            ZF_LOGE("Failed to initialize %p", init_fun);
        }

    }
    err = camkes_call_hardware_init_modules(&io_ops);
    if (err) {
        ZF_LOGE("Failed to initialize hardware init modules");
    }
    for (camkes_module_init_fn_t *init_fun = __start__post_init; init_fun < __stop__post_init; init_fun++) {
        err = (*init_fun)(&io_ops);
        if (err) {
            ZF_LOGE("Failed to initialize %p", init_fun);
        }

    }
}


/*- do allocate_cap_instance(me) -*/
/*- set notification = pop('notification') -*/
/*- do allocate_rpc_cap_instance(me) -*/
/*- set endpoint = pop('endpoint') -*/

/*# Allocate reply cap #*/
/*- if options.realtime -*/
        /*- set reply_cap_slot = alloc('reply_cap_slot', seL4_RTReplyObject) -*/
/*- else -*/
        /*# We're going to need a CNode cap in order to save our pending reply
         * caps in the future.
         #*/
        /*- set cnode = alloc_cap('cnode', my_cnode) -*/
        /*- set reply_cap_slot = alloc_cap('reply_cap_slot', None) -*/
/*- endif -*/


struct handlers {
    seL4_Word badge;
    void (*callback_handler)(seL4_Word, void *);
    void * cookie;
    const char* name;
};

static struct handlers *reg_handlers;
static int num_handlers = 0;
int single_threaded_component_register_handler(seL4_Word badge, const char* name, void (*callback_handler)(seL4_Word, void *), void * cookie) {
    reg_handlers = realloc(reg_handlers, (num_handlers+1) * sizeof(*reg_handlers));
    if (!reg_handlers) {
        ZF_LOGF("Failed to allocate handlers");
    }
    reg_handlers[num_handlers] = (struct handlers) {.badge = badge, .callback_handler = callback_handler, .cookie = cookie, .name = name};
    num_handlers++;
    return 0;
}


void trace_start(void *_arg) {
    for (int i = 0; i < NUM_TRACES; i++) {
        trace_point_sum[i] = 0;
        trace_point_count[i] = 0;
        
    }
    int res = trace_start_reg_callback(trace_start, NULL);
    if (res) {
        ZF_LOGE("Failed to register trace callback");
    }

}

void trace_stop(void *_arg) {
    printf("traces,%s\n", get_instance_name());
    for (int i = 0; i < MIN(NUM_TRACES, num_handlers+3); i++) {
        if (i == 0) {
            printf("total_endpoint_calls,");
        } else if (i == 1) {
            printf("total_notifications,");
        } else if (i == 2) {
            printf("irq_handlers,");
        } else if(i >= 3 && reg_handlers[i-3].name) {
            printf("%s,", reg_handlers[i-3].name);
        } else {
            break;
        }
        printf("%ld,%ld\n", trace_point_count[i], trace_point_sum[i]);
    }
    int res = trace_stop_reg_callback(trace_stop, NULL);
    if (res) {
        ZF_LOGE("Failed to register trace callback");
    }

}

int run(void) {
    int res = trace_start_reg_callback ? trace_start_reg_callback(trace_start, NULL): 0;
    if (res) {
        ZF_LOGE("Failed to register trace callback");
    }
    res = trace_stop_reg_callback ? trace_stop_reg_callback(trace_stop, NULL): 0;
    if (res) {
        ZF_LOGE("Failed to register trace callback");
    }

    /* Now poll for events and handle them */
    seL4_Word badge;
    seL4_Word notification_base = /*? configuration[me.name].get("global_endpoint_base", 1) ?*/;
    seL4_Word endpoint_base = /*? configuration[me.name].get("global_rpc_endpoint_base", 0) ?*/;
    if (notification_base == endpoint_base) {
        ZF_LOGE("Notification and Endpoint badge values share the same base bitfield.\n"
                "This means that they cannot be distinguished from each other.\n"
                "This may result in instability.");
    }
    seL4_TCB_BindNotification(camkes_get_tls()->tcb_cap, /*? notification ?*/);
    /*- if not options.realtime -*/
        camkes_get_tls()->cnode_cap = /*? cnode ?*/;
    /*- endif -*/
        seL4_MessageInfo_t info = /*? generate_seL4_Recv(options, endpoint,
                                                                '&badge',
                                                                 reply_cap_slot) ?*/;
    while (1) {

        if ((badge & endpoint_base) == endpoint_base) {
            int result = 0;
            TRACE_START(0);
            switch (badge) {
#define X(b) \
    case (b):
     /*- for conn in composition.connections -*/
       /*- if conn.type.get_attribute("to_global_rpc_endpoint") and conn.type.get_attribute("to_global_rpc_endpoint").default and conn.to_ends[0].instance == me -*/

        /*? conn.to_end.interface.name ?*/_BADGES_MACRO_X_ITERATOR()
        /*- if not options.realtime -*/
            /*# We need to save the reply cap because the user's implementation may
             * perform operations that overwrite or discard it.
             #*/
            camkes_declare_reply_cap(/*? reply_cap_slot ?*/);
        /*- endif -*/

        result = /*? conn.to_end.interface.name ?*/_handle_message(&info, badge);
        break;
      /*- endif -*/
    /*- endfor -*/
#undef X
               default:
                    // Fall through below
                    result = -1;
            }

            if (result == 1) {
                TRACE_END(0);
                /* Send the response */
                /*-- if not options.realtime -*/
                    camkes_tls_t * tls = camkes_get_tls();
                    assert(tls != NULL);
                    if (tls->reply_cap_in_tcb) {
                        tls->reply_cap_in_tcb = false;
                        info = /*? generate_seL4_ReplyRecv(options, endpoint,
                                                                'info',
                                                                '&badge',
                                                                reply_cap_slot) ?*/;
                    } else {
                        camkes_unprotect_reply_cap();
                        seL4_Send(/*? reply_cap_slot ?*/, info);
                        info = /*? generate_seL4_Recv(options, endpoint,
                                                            '&badge',
                                                            reply_cap_slot) ?*/;
                    }
                /*-- else -*/
                    info = /*? generate_seL4_ReplyRecv(options, endpoint,
                                                            info,
                                                            '&badge',
                                                            reply_cap_slot) ?*/;
                /*-- endif -*/
                continue;
            } else if (result = 0) {
                TRACE_END(0);
                /* Don't reply to the caller */
                info = /*? generate_seL4_Recv(options, endpoint,
                                                         '&badge',
                                                         reply_cap_slot) ?*/;
                continue;
            }

        }
        TRACE_START(1);
        if ((badge & notification_base) == notification_base) {
            TRACE_START(2);
            int num_handlers_called = camkes_handle_global_endpoint_irq(badge);
            TRACE_END(2);
            trace_point_count[2]+= num_handlers_called;
            trace_point_count[2]--;
            for (int i = 0; i < num_handlers; i++) {
                seL4_Word b = reg_handlers[i].badge;
                if ((badge & b) == b) {
                    if ((i + 3) < NUM_TRACES) {
                        TRACE_START(i + 3);
                    } 
                    reg_handlers[i].callback_handler(b, reg_handlers[i].cookie);
                    if ((i + 3) < NUM_TRACES) {
                        TRACE_END(i + 3);
                    }
                }
            }
        }
        TRACE_END(1);
        info = /*? generate_seL4_Recv(options, endpoint,
                                    '&badge',
                                    reply_cap_slot) ?*/;


    }

}
