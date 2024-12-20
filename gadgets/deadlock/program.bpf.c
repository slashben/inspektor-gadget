// SPDX-License-Identifier: (LGPL-2.1 OR BSD-2-Clause)
/* Copyright (c) 2024 The Inspektor Gadget authors */

#include <vmlinux.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>

#include <gadget/buffer.h>
#include <gadget/common.h>
#include <gadget/macros.h>
#include <gadget/mntns_filter.h>

enum operation { lock, unlock };

struct event {
	gadget_timestamp timestamp_raw;
	struct gadget_process proc;

	__u64 mutex_addr;
	enum operation operation_raw;
};

GADGET_TRACER_MAP(events, 1024 * 256);
GADGET_TRACER(deadlock, events, event);

static __always_inline int
gen_mutex_event(struct pt_regs *ctx, enum operation operation, u64 mutex_addr)
{
	struct event *event;

	if (gadget_should_discard_mntns_id(gadget_get_mntns_id()))
		return 0;

	event = gadget_reserve_buf(&events, sizeof(*event));
	if (!event)
		return 0;

	gadget_process_populate(&event->proc);
	event->mutex_addr = mutex_addr;
	event->operation_raw = operation;
	event->timestamp_raw = bpf_ktime_get_ns();

	gadget_submit_buf(ctx, &events, event, sizeof(*event));

	return 0;
}

/* mutex acquisition */
SEC("uprobe/libc:pthread_mutex_lock")
int BPF_UPROBE(trace_uprobe_mutex_lock, void *mutex_addr)
{
	return gen_mutex_event(ctx, lock, (u64)mutex_addr);
}

/* mutex release */
SEC("uprobe/libc:pthread_mutex_unlock")
int BPF_UPROBE(trace_uprobe_mutex_unlock, void *mutex_addr)
{
	return gen_mutex_event(ctx, unlock, (u64)mutex_addr);
}

char LICENSE[] SEC("license") = "Dual BSD/GPL";
