
/*
 * trafficman.h - skeleton vpp engine plug-in header file
 *
 * Copyright (c) <current-year> <your-organization>
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef __included_trafficman_h__
#define __included_trafficman_h__

#include <vnet/vnet.h>
#include <vnet/ip/ip.h>
#include <vnet/ethernet/ethernet.h>

#include <vppinfra/hash.h>
#include <vppinfra/error.h>


typedef struct trafficman_wheel_entry_t
{
    f64 tx_time;
    u32 rx_sw_if_index;
    u32 tx_sw_if_index;
    u32 output_next_index;

    u32 buffer_index;
    u32 action;
    
    struct trafficman_wheel_entry_t *next;

    u32 pad;			/* pad to 32-bytes */
} trafficman_wheel_entry_t;

typedef struct
{
    u32 wheel_size;
    u32 cursize;

    trafficman_wheel_entry_t *head;
    CLIB_CACHE_LINE_ALIGN_MARK (pad);
} trafficman_wheel_t;

typedef struct {
    /* API message ID base */
    u16 msg_id_base;

    /* on/off switch for the periodic function */
    u8 periodic_timer_enabled;
    /* Node index, non-zero if the periodic process has been created */
    u32 periodic_node_index;

    trafficman_wheel_t **wheel_by_thread;
    u64 mmap_size;
    u32 wheel_slots_per_wrk;

    /* convenience */
    vlib_main_t * vlib_main;
    vnet_main_t * vnet_main;
    ethernet_main_t * ethernet_main;
} trafficman_main_t;

extern trafficman_main_t trafficman_main;

extern vlib_node_registration_t trafficman_node;
extern vlib_node_registration_t trafficman_periodic_node;

/* Periodic function events */
#define TRAFFICMAN_EVENT1 1
#define TRAFFICMAN_EVENT2 2
#define TRAFFICMAN_EVENT_PERIODIC_ENABLE_DISABLE 3

void trafficman_create_periodic_process (trafficman_main_t *);

#endif /* __included_trafficman_h__ */

/*
 * fd.io coding-style-patch-verification: ON
 *
 * Local Variables:
 * eval: (c-set-style "gnu")
 * End:
 */

