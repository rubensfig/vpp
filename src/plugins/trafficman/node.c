/*
 * node.c - skeleton vpp engine plug-in dual-loop node skeleton
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
#include <vlib/vlib.h>
#include <vnet/vnet.h>
#include <vnet/pg/pg.h>
#include <vppinfra/error.h>
#include <trafficman/trafficman.h>

typedef struct
{
  u32 sw_if_index;
  u32 next_index;
} trafficman_trace_t;

#ifndef CLIB_MARCH_VARIANT

/* packet trace format function */
static u8 * format_trafficman_trace (u8 * s, va_list * args)
{
  CLIB_UNUSED (vlib_main_t * vm) = va_arg (*args, vlib_main_t *);
  CLIB_UNUSED (vlib_node_t * node) = va_arg (*args, vlib_node_t *);
  trafficman_trace_t * t = va_arg (*args, trafficman_trace_t *);
  
  s = format (s, "TRAFFICMAN: sw_if_index %d, next index %d\n",
              t->sw_if_index, t->next_index);
  return s;
}

vlib_node_registration_t trafficman_node;

#endif /* CLIB_MARCH_VARIANT */

#define foreach_trafficman_error \
_(COUNTED, "Packets Processed")

typedef enum 
{
#define _(sym,str) TRAFFICMAN_ERROR_##sym,
  foreach_trafficman_error
#undef _
  TRAFFICMAN_N_ERROR,
} trafficman_error_t;

#ifndef CLIB_MARCH_VARIANT
static char * trafficman_error_strings[] = 
{
#define _(sym,string) string,
  foreach_trafficman_error
#undef _
};
#endif /* CLIB_MARCH_VARIANT */

typedef enum 
{
  TRAFFICMAN_NEXT_CONTINUE,
  TRAFFICMAN_NEXT_DROP,
  TRAFFICMAN_N_NEXT,
} trafficman_next_t;

always_inline int
trafficman_handle_buffer(trafficman_wheel_t ** wp, u32 bi)
{
	// Count packets
	int i = 0;
	
	trafficman_wheel_entry_t *ep = clib_mem_vm_alloc(sizeof (trafficman_wheel_entry_t));

	if ((*wp)->cursize + 1 > 10)
        ep->action = TRAFFICMAN_NEXT_DROP;
    else {
        ep->action = TRAFFICMAN_NEXT_CONTINUE;
    }

	(*wp)->cursize++;

	ep->tx_time = 0;
	ep->buffer_index = bi;

	ep->next = (*wp)->head;
	(*wp)->head = ep;

	return i += 1;
}

always_inline u32
trafficman_return_buffer(trafficman_wheel_t ** wp, u32 * bi, u32 * action)
{
	if ((*wp)->cursize == 0)
		return 0;

	if ((*wp)->head == NULL)
		return 0;

	trafficman_wheel_entry_t *ep = (*wp)->head;

	(*wp)->head = ep->next;

	(*wp)->cursize--;

 	return 0;
}

always_inline uword
trafficman_inline (vlib_main_t * vm,
     		 vlib_node_runtime_t * node, vlib_frame_t * frame,
		 int is_ip4, int is_trace)
{
  trafficman_main_t *nsm = &trafficman_main;
  u32 n_left_from, *from;
  vlib_buffer_t *bufs[VLIB_FRAME_SIZE], **b;
  u16 nexts[VLIB_FRAME_SIZE], *next;
  trafficman_wheel_t *wp = nsm->wheel_by_thread[vm->thread_index];
  int counter = 0;
  u32 output[VLIB_FRAME_SIZE];
  u32 action;

  from = vlib_frame_vector_args (frame);
  n_left_from = frame->n_vectors;

  vlib_get_buffers (vm, from, bufs, n_left_from);
  b = bufs;
  next = nexts;

  while (n_left_from >= 4)
    {
      if (PREDICT_TRUE (n_left_from >= 8))
	{
	  vlib_prefetch_buffer_header (b[4], STORE);
	  vlib_prefetch_buffer_header (b[5], STORE);
	  vlib_prefetch_buffer_header (b[6], STORE);
	  vlib_prefetch_buffer_header (b[7], STORE);

          CLIB_PREFETCH (b[4]->data, CLIB_CACHE_LINE_BYTES, STORE);
          CLIB_PREFETCH (b[5]->data, CLIB_CACHE_LINE_BYTES, STORE);
          CLIB_PREFETCH (b[6]->data, CLIB_CACHE_LINE_BYTES, STORE);
          CLIB_PREFETCH (b[7]->data, CLIB_CACHE_LINE_BYTES, STORE);
	}

     // $$$$ process 4x pkts right here
      next[0] = 0;
      next[1] = 0;
      next[2] = 0;
      next[3] = 0;

      counter = trafficman_handle_buffer(&wp, from[0]);
      counter = trafficman_handle_buffer(&wp, from[1]);
      counter += trafficman_handle_buffer(&wp, from[2]);
      counter += trafficman_handle_buffer(&wp, from[3]);

      if (is_trace)
	{
	  if (b[0]->flags & VLIB_BUFFER_IS_TRACED)
	    {
	      trafficman_trace_t *t = 
                   vlib_add_trace (vm, node, b[0], sizeof (*t));
	      t->next_index = next[0];
              t->sw_if_index = vnet_buffer(b[0])->sw_if_index[VLIB_RX];
	    }
	  if (b[1]->flags & VLIB_BUFFER_IS_TRACED)
	    {
	      trafficman_trace_t *t = 
                    vlib_add_trace (vm, node, b[1], sizeof (*t));
	      t->next_index = next[1];
              t->sw_if_index = vnet_buffer(b[1])->sw_if_index[VLIB_RX];
	    }
	  if (b[2]->flags & VLIB_BUFFER_IS_TRACED)
	    {
	      trafficman_trace_t *t = 
                    vlib_add_trace (vm, node, b[2], sizeof (*t));
	      t->next_index = next[2];
              t->sw_if_index = vnet_buffer(b[2])->sw_if_index[VLIB_RX];
	    }
	  if (b[3]->flags & VLIB_BUFFER_IS_TRACED)
	    {
	      trafficman_trace_t *t = 
                    vlib_add_trace (vm, node, b[3], sizeof (*t));
	      t->next_index = next[3];
              t->sw_if_index = vnet_buffer(b[3])->sw_if_index[VLIB_RX];
	    }
	}

      b += 4;
      from += 4;
      next += 4;
      n_left_from -= 4;
    }

  while (n_left_from > 0)
    {
     /* $$$$ process 1 pkt right here */
      next[0] = 0;

      counter = trafficman_handle_buffer(&wp, from[0]);

      if (is_trace)
	{
	  if (b[0]->flags & VLIB_BUFFER_IS_TRACED)
	    {
	      trafficman_trace_t *t = 
                    vlib_add_trace (vm, node, b[0], sizeof (*t));
	      t->next_index = next[0];
              t->sw_if_index = vnet_buffer(b[0])->sw_if_index[VLIB_RX];
	    }
	}

      b += 1;
      from += 1;
      next += 1;
      n_left_from -= 1;
    }

  u32 count;
  for (u32 i = 0; i < frame->n_vectors; i++)
  {
    u32 index, action;
  	trafficman_return_buffer(&wp, &index, &action);

	output[i] = index;
    nexts[i] = action;

	count = i;
  }

  vlib_buffer_enqueue_to_next (vm, node, output, nexts, count);

  vlib_node_increment_counter (vm, trafficman_node.index, TRAFFICMAN_ERROR_COUNTED, counter);

  return frame->n_vectors;
}

VLIB_NODE_FN (trafficman_node) (vlib_main_t * vm, vlib_node_runtime_t * node,
                             vlib_frame_t * frame)
{
  if (PREDICT_FALSE (node->flags & VLIB_NODE_FLAG_TRACE))
    return trafficman_inline (vm, node, frame, 1 /* is_ip4 */ ,
			    1 /* is_trace */ );
  else
    return trafficman_inline (vm, node, frame, 1 /* is_ip4 */ ,
			    0 /* is_trace */ );
}

/* *INDENT-OFF* */
#ifndef CLIB_MARCH_VARIANT
VLIB_REGISTER_NODE (trafficman_node) = 
{
  .name = "trafficman",
  .vector_size = sizeof (u32),
  .format_trace = format_trafficman_trace,
  .type = VLIB_NODE_TYPE_INTERNAL,
  
  .n_errors = ARRAY_LEN(trafficman_error_strings),
  .error_strings = trafficman_error_strings,

  .n_next_nodes = TRAFFICMAN_N_NEXT,

  /* edit / add dispositions here */
  .next_nodes = {
        [TRAFFICMAN_NEXT_CONTINUE] = "ethernet-input",
        [TRAFFICMAN_NEXT_DROP] = "error-drop",
  },
};
#endif /* CLIB_MARCH_VARIANT */
/* *INDENT-ON* */
/*
 * fd.io coding-style-patch-verification: ON
 *
 * Local Variables:
 * eval: (c-set-style "gnu")
 * End:
 */
