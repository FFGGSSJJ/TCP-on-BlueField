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

#include <stdint.h>
#include <signal.h>

#include <rte_cycles.h>
#include <rte_launch.h>
#include <rte_ethdev.h>

#include <doca_argp.h>
#include <doca_log.h>

#include <dpdk_utils.h>
#include <utils.h>

#include "simple_fwd.h"
#include "simple_fwd_port.h"
#include "simple_fwd_vnf_core.h"

DOCA_LOG_REGISTER(SIMPLE_FWD_VNF);

#define DEFAULT_NB_METERS (1 << 13)

static void
signal_handler(int signum)
{
	if (signum == SIGINT || signum == SIGTERM) {
		DOCA_LOG_INFO("Signal %d received, preparing to exit...", signum);
		simple_fwd_process_pkts_stop();
	}
}

int
main(int argc, char **argv)
{
	uint16_t port_id;
	struct simple_fwd_port_cfg port_cfg = {0};
	struct application_dpdk_config dpdk_config = {
		.port_config.nb_ports = 2,
		.port_config.nb_queues = 4,
		.port_config.nb_hairpin_q = 0,
		.sft_config = {0},
		.reserve_main_thread = true,
	};
	struct simple_fwd_config app_cfg = {
		.dpdk_cfg = &dpdk_config,
		.rx_only = 0,
		.hw_offload = 1,
		.stats_timer = 100000,
		.age_thread = false,
	};
	struct app_vnf *vnf;
	struct simple_fwd_process_pkts_params process_pkts_params = {.cfg = &app_cfg};

	/* init and start parsing */
	struct doca_argp_program_general_config *doca_general_config;
	struct doca_argp_program_type_config type_config = {
		.is_dpdk = true,
		.is_grpc = false,
	};

	/* Parse cmdline/json arguments */
	doca_argp_init("simple_forward_vnf", &type_config, &app_cfg);
	register_simple_fwd_params();
	doca_argp_start(argc, argv, &doca_general_config);

	doca_log_create_syslog_backend("doca_core");

	/* update queues and ports */
	dpdk_init(&dpdk_config);

	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);

	/* convert to number of cycles */
	app_cfg.stats_timer *= rte_get_timer_hz();

	vnf = simple_fwd_get_vnf();
	port_cfg.nb_queues = dpdk_config.port_config.nb_queues;
	port_cfg.is_hairpin = !!dpdk_config.port_config.nb_hairpin_q;
	port_cfg.nb_meters = DEFAULT_NB_METERS;
	port_cfg.nb_counters = (1 << 13);
	if (vnf->vnf_init(&port_cfg) != 0) {
		DOCA_LOG_ERR("vnf application init error");
		goto exit_app;
	}

	simple_fwd_map_queue(dpdk_config.port_config.nb_queues);
	process_pkts_params.vnf = vnf;
	rte_eal_mp_remote_launch(simple_fwd_process_pkts, &process_pkts_params, CALL_MAIN);
	rte_eal_mp_wait_lcore();

	RTE_ETH_FOREACH_DEV(port_id)
		doca_flow_destroy_port(port_id);

exit_app:
	/* cleanup app resources */
	simple_fwd_destroy(vnf);

	/* cleanup resources */
	dpdk_fini(&dpdk_config);

	/* ARGP cleanup */
	doca_argp_destroy();

	return 0;
}
