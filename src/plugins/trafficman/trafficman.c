/*
 * trafficman.c - skeleton vpp engine plug-in
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

#include <vnet/vnet.h>
#include <vnet/plugin/plugin.h>
#include <trafficman/trafficman.h>

#include <vlibapi/api.h>
#include <vlibmemory/api.h>
#include <vpp/app/version.h>
#include <stdbool.h>

#include <trafficman/trafficman.api_enum.h>
#include <trafficman/trafficman.api_types.h>

#define REPLY_MSG_ID_BASE tmp->msg_id_base
#include <vlibapi/api_helper_macros.h>

trafficman_main_t trafficman_main;


static trafficman_wheel_t *
trafficman_wheel_alloc (trafficman_main_t *nsm)
{
  u32 pagesize = getpagesize ();
  trafficman_wheel_t *wp;
  
  nsm->mmap_size = sizeof (trafficman_wheel_t) +
  	256 * sizeof (trafficman_wheel_entry_t);
  
  nsm->mmap_size += pagesize - 1;
  nsm->mmap_size &= ~(pagesize - 1);
  
  wp = clib_mem_vm_alloc (nsm->mmap_size);
  ASSERT (wp != 0);

  wp->wheel_size = 10;
  wp->cursize = 0;

  wp->head = NULL;
  
  return wp;
}

static void
trafficman_configure (trafficman_main_t *nsm)
{
    int num_workers = vlib_num_workers ();;

    // Reserve space for wheels
    vec_validate (nsm->wheel_by_thread, num_workers);

    /* Initialize the output scheduler wheels */
    for (int i = 0; i < num_workers + 1; i++) 
	nsm->wheel_by_thread[i] = trafficman_wheel_alloc (nsm);
}

/* Action function shared between message handler and debug CLI */

int trafficman_enable_disable (trafficman_main_t * tmp, u32 sw_if_index,
                                   int enable_disable)
{
  vnet_sw_interface_t * sw;
  int rv = 0;

  /* Utterly wrong? */
  if (pool_is_free_index (tmp->vnet_main->interface_main.sw_interfaces,
                          sw_if_index))
    return VNET_API_ERROR_INVALID_SW_IF_INDEX;

  /* Not a physical port? */
  sw = vnet_get_sw_interface (tmp->vnet_main, sw_if_index);
  if (sw->type != VNET_SW_INTERFACE_TYPE_HARDWARE)
    return VNET_API_ERROR_INVALID_SW_IF_INDEX;

  trafficman_create_periodic_process (tmp);

  vnet_feature_enable_disable ("device-input", "trafficman",
                               sw_if_index, enable_disable, 0, 0);

  /* Send an event to enable/disable the periodic scanner process */
  vlib_process_signal_event (tmp->vlib_main,
                             tmp->periodic_node_index,
                             TRAFFICMAN_EVENT_PERIODIC_ENABLE_DISABLE,
                            (uword)enable_disable);
  return rv;
}

static clib_error_t *
trafficman_enable_disable_command_fn (vlib_main_t * vm,
                                   unformat_input_t * input,
                                   vlib_cli_command_t * cmd)
{
  trafficman_main_t * tmp = &trafficman_main;
  u32 sw_if_index = ~0;
  int enable_disable = 1;

  int rv;

  while (unformat_check_input (input) != UNFORMAT_END_OF_INPUT)
    {
      if (unformat (input, "disable"))
        enable_disable = 0;
      else if (unformat (input, "%U", unformat_vnet_sw_interface,
                         tmp->vnet_main, &sw_if_index))
        ;
      else
        break;
  }

  if (sw_if_index == ~0)
    return clib_error_return (0, "Please specify an interface...");

  rv = trafficman_enable_disable (tmp, sw_if_index, enable_disable);

  switch(rv)
    {
  case 0:
    break;

  case VNET_API_ERROR_INVALID_SW_IF_INDEX:
    return clib_error_return
      (0, "Invalid interface, only works on physical ports");
    break;

  case VNET_API_ERROR_UNIMPLEMENTED:
    return clib_error_return (0, "Device driver doesn't support redirection");
    break;

  default:
    return clib_error_return (0, "trafficman_enable_disable returned %d",
                              rv);
    }

  trafficman_configure(tmp);

  return 0;
}

/* *INDENT-OFF* */
VLIB_CLI_COMMAND (trafficman_enable_disable_command, static) =
{
  .path = "trafficman enable-disable",
  .short_help =
  "trafficman enable-disable <interface-name> [disable]",
  .function = trafficman_enable_disable_command_fn,
};
/* *INDENT-ON* */

/* API message handler */
static void vl_api_trafficman_enable_disable_t_handler
(vl_api_trafficman_enable_disable_t * mp)
{
  vl_api_trafficman_enable_disable_reply_t * rmp;
  trafficman_main_t * tmp = &trafficman_main;
  int rv;

  rv = trafficman_enable_disable (tmp, ntohl(mp->sw_if_index),
                                      (int) (mp->enable_disable));

  REPLY_MACRO(VL_API_TRAFFICMAN_ENABLE_DISABLE_REPLY);
}

/* API definitions */
#include <trafficman/trafficman.api.c>

static clib_error_t * trafficman_init (vlib_main_t * vm)
{
  trafficman_main_t * tmp = &trafficman_main;
  clib_error_t * error = 0;

  tmp->vlib_main = vm;
  tmp->vnet_main = vnet_get_main();

  /* Add our API messages to the global name_crc hash table */
  tmp->msg_id_base = setup_message_id_table ();

  return error;
}

VLIB_INIT_FUNCTION (trafficman_init);

/* *INDENT-OFF* */
VNET_FEATURE_INIT (trafficman, static) =
{
  .arc_name = "device-input",
  .node_name = "trafficman",
  .runs_before = VNET_FEATURES ("ethernet-input"),
};
/* *INDENT-ON */

/* *INDENT-OFF* */
VLIB_PLUGIN_REGISTER () =
{
  .version = VPP_BUILD_VER,
  .description = "trafficman plugin description goes here",
};
/* *INDENT-ON* */

/*
 * fd.io coding-style-patch-verification: ON
 *
 * Local Variables:
 * eval: (c-set-style "gnu")
 * End:
 */
