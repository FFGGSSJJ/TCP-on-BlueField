/*
 * Copyright (c) 2021-2022 NVIDIA CORPORATION & AFFILIATES, ALL RIGHTS RESERVED.
 *
 * This software product is a proprietary product of NVIDIA CORPORATION &
 * AFFILIATES (the "Company") and all right, title, and interest in and to the
 * software product, including all associated intellectual property rights, are
 * and shall remain exclusively with the Company.
 *
 * This software product is governed by the End User License Agreement
 * provided with the software product.
 *
 */

#include <stdio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/time.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/queue.h>
#include <netinet/in.h>
#include <setjmp.h>
#include <stdarg.h>
#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <stdbool.h>

#include <rte_eal.h>
#include <rte_common.h>
#include <rte_malloc.h>
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_net.h>
#include <rte_flow.h>

#include <doca_argp.h>
#include <doca_flow.h>
#include <doca_log.h>

#include <dpdk_utils.h>
#include <utils.h>

#include "simple_fwd_ft.h"
#include "simple_fwd_port.h"
#include "simple_fwd_vnf_core.h"

DOCA_LOG_REGISTER(SIMPLE_FWD_VNF:Core);

#define VNF_PKT_L2(M) rte_pktmbuf_mtod(M, uint8_t *)
#define VNF_PKT_LEN(M) rte_pktmbuf_pkt_len(M)
#define VNF_RX_BURST_SIZE (32)

static volatile bool force_quit;

struct vnf_per_core_params {
	int ports[NUM_OF_PORTS];
	int queues[NUM_OF_PORTS];
	bool used;
};

struct vnf_per_core_params core_params_arr[RTE_MAX_LCORE];

static void
vnf_adjust_mbuf(struct rte_mbuf *m,
		struct simple_fwd_pkt_info *pinfo)
{
	int diff = pinfo->outer.l2 - VNF_PKT_L2(m);

	rte_pktmbuf_adj(m, diff);
}

static void
simple_fwd_process_offload(struct rte_mbuf *mbuf, uint16_t queue_id, struct app_vnf *vnf)
{
	struct simple_fwd_pkt_info pinfo;

	memset(&pinfo, 0, sizeof(struct simple_fwd_pkt_info));
	if (simple_fwd_parse_packet(VNF_PKT_L2(mbuf),
		VNF_PKT_LEN(mbuf), &pinfo))
		return;
	pinfo.orig_data = mbuf;
	pinfo.orig_port_id = mbuf->port;
	pinfo.pipe_queue = queue_id;
	pinfo.rss_hash = mbuf->hash.rss;
	if (pinfo.outer.l3_type != IPV4)
		return;
	vnf->vnf_process_pkt(&pinfo);
	vnf_adjust_mbuf(mbuf, &pinfo);
}

int
simple_fwd_process_pkts(void *process_pkts_params)
{
	uint64_t cur_tsc, last_tsc;
	struct rte_mbuf *mbufs[VNF_RX_BURST_SIZE];
	uint16_t j, nb_rx, queue_id;
	uint32_t port_id = 0, core_id = rte_lcore_id();
	struct vnf_per_core_params *params = &core_params_arr[core_id];
	struct simple_fwd_config *app_config = ((struct simple_fwd_process_pkts_params *) process_pkts_params)->cfg;
	struct app_vnf *vnf = ((struct simple_fwd_process_pkts_params *) process_pkts_params)->vnf;

	if (!params->used) {
		DOCA_LOG_DBG("core %u nothing need to do", core_id);
		return 0;
	}
	DOCA_LOG_INFO("core %u process queue %u start", core_id, params->queues[0]);
	last_tsc = rte_rdtsc();
	while (!force_quit) {
		if (core_id == rte_get_main_lcore()) {
			cur_tsc = rte_rdtsc();
			if (cur_tsc > last_tsc + app_config->stats_timer) {
				simple_fwd_dump_port_stats(0);
				last_tsc = cur_tsc;
			}
		}
		for (port_id = 0; port_id < NUM_OF_PORTS; port_id++) {
			queue_id = params->queues[port_id];
			nb_rx = rte_eth_rx_burst(port_id, queue_id, mbufs, VNF_RX_BURST_SIZE);
			for (j = 0; j < nb_rx; j++) {
				if (app_config->hw_offload && core_id == rte_get_main_lcore())
					simple_fwd_process_offload(mbufs[j], queue_id, vnf);
				if (app_config->rx_only)
					rte_pktmbuf_free(mbufs[j]);
				else
					rte_eth_tx_burst(port_id ^ 1, queue_id, &mbufs[j], 1);
			}
			if (!app_config->age_thread)
				vnf->vnf_flow_age(port_id, queue_id);
		}
	}
	return 0;
}

void
simple_fwd_process_pkts_stop()
{
	force_quit = true;
}

static void
stats_callback(void *config, void *param)
{
	struct simple_fwd_config *app_config = (struct simple_fwd_config *) config;

	app_config->stats_timer = *(int *) param;
	DOCA_LOG_DBG("set stats_timer:%lu", app_config->stats_timer);
}

static void
nr_queues_callback(void *config, void *param)
{
	struct simple_fwd_config *app_config = (struct simple_fwd_config *) config;
	int nr_queues = *(int *) param;

	if (nr_queues < 2) {
		DOCA_LOG_ERR("nr_queues should >= 2\n");
		doca_argp_usage();
	}
	app_config->dpdk_cfg->port_config.nb_queues = nr_queues;
	DOCA_LOG_DBG("set nr_queues:%u", nr_queues);
}

static void
rx_only_callback(void *config, void *param)
{
	struct simple_fwd_config *app_config = (struct simple_fwd_config *) config;

	app_config->rx_only = *(bool *) param ? 1 : 0;
	DOCA_LOG_DBG("set rx_only:%u", app_config->rx_only);
}

static void
hw_offload_callback(void *config, void *param)
{
	struct simple_fwd_config *app_config = (struct simple_fwd_config *) config;

	app_config->hw_offload = *(bool *) param ? 1 : 0;
	DOCA_LOG_DBG("set hw_offload:%u", app_config->hw_offload);
}

static void
hairpinq_callback(void *config, void *param)
{
	struct simple_fwd_config *app_config = (struct simple_fwd_config *) config;

	app_config->dpdk_cfg->port_config.nb_hairpin_q = *(bool *) param ? 1 : 0;
	DOCA_LOG_DBG("set is_hairpin:%u", app_config->dpdk_cfg->port_config.nb_hairpin_q);
}

static void
age_thread_callback(void *config, void *param)
{
	struct simple_fwd_config *app_config = (struct simple_fwd_config *) config;

	app_config->age_thread = *(bool *) param;
	DOCA_LOG_DBG("set age_thread:%s", app_config->age_thread ? "true":"false");
}

void
register_simple_fwd_params()
{
	struct doca_argp_param stats_param = {
		.short_flag = "t",
		.long_flag = "stats-timer",
		.arguments = "<time>",
		.description = "Set interval to dump stats information",
		.callback = stats_callback,
		.arg_type = DOCA_ARGP_TYPE_INT,
		.is_mandatory = false,
		.is_cli_only = false
	};
	struct doca_argp_param nr_queues_param = {
		.short_flag = "q",
		.long_flag = "nr-queues",
		.arguments = "<num>",
		.description = "Set queues number",
		.callback = nr_queues_callback,
		.arg_type = DOCA_ARGP_TYPE_INT,
		.is_mandatory = false,
		.is_cli_only = false
	};
	struct doca_argp_param rx_only_param = {
		.short_flag = "r",
		.long_flag = "rx-only",
		.arguments = NULL,
		.description = "Set rx only",
		.callback = rx_only_callback,
		.arg_type = DOCA_ARGP_TYPE_BOOLEAN,
		.is_mandatory = false,
		.is_cli_only = false
	};
	struct doca_argp_param hw_offload_param = {
		.short_flag = "o",
		.long_flag = "hw-offload",
		.arguments = NULL,
		.description = "Set hw offload",
		.callback = hw_offload_callback,
		.arg_type = DOCA_ARGP_TYPE_BOOLEAN,
		.is_mandatory = false,
		.is_cli_only = false
	};
	struct doca_argp_param hairpinq_param = {
		.short_flag = "hq",
		.long_flag = "hairpinq",
		.arguments = NULL,
		.description = "Set forwarding to hairpin queue",
		.callback = hairpinq_callback,
		.arg_type = DOCA_ARGP_TYPE_BOOLEAN,
		.is_mandatory = false,
		.is_cli_only = false
	};
	struct doca_argp_param age_thread_param = {
		.short_flag = "a",
		.long_flag = "age-thread",
		.arguments = NULL,
		.description = "Start thread do aging",
		.callback = age_thread_callback,
		.arg_type = DOCA_ARGP_TYPE_BOOLEAN,
		.is_mandatory = false,
		.is_cli_only = false
	};

	doca_argp_register_param(&stats_param);
	doca_argp_register_param(&nr_queues_param);
	doca_argp_register_param(&rx_only_param);
	doca_argp_register_param(&hw_offload_param);
	doca_argp_register_param(&hairpinq_param);
	doca_argp_register_param(&age_thread_param);
	doca_argp_register_version_callback(sdk_version_callback);
}

void
simple_fwd_map_queue(uint16_t nb_queues)
{
	int i, queue_idx = 0;

	memset(core_params_arr, 0, sizeof(core_params_arr));
	for (i = 0; i < RTE_MAX_LCORE; i++) {
		if (!rte_lcore_is_enabled(i))
			continue;
		core_params_arr[i].ports[0] = 0;
		core_params_arr[i].ports[1] = 1;
		core_params_arr[i].queues[0] = queue_idx;
		core_params_arr[i].queues[1] = queue_idx;
		core_params_arr[i].used = true;
		queue_idx++;
		if (queue_idx >= nb_queues)
			break;
	}
}

void
simple_fwd_destroy(struct app_vnf *vnf)
{
	vnf->vnf_destroy();
}
