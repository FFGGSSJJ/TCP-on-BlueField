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

#ifndef SIMPLE_FWD_VNF_CORE_H_
#define SIMPLE_FWD_VNF_CORE_H_

#include <offload_rules.h>

#include "app_vnf.h"

struct simple_fwd_config {
	struct application_dpdk_config *dpdk_cfg;
	uint16_t rx_only;
	uint16_t hw_offload;
	uint64_t stats_timer;
	bool age_thread;
};

struct simple_fwd_process_pkts_params {
	struct simple_fwd_config *cfg;
	struct app_vnf *vnf;
};

void register_simple_fwd_params(void);
int simple_fwd_process_pkts(void *process_pkts_params);
void simple_fwd_process_pkts_stop(void);
void simple_fwd_map_queue(uint16_t nb_queues);
void simple_fwd_destroy(struct app_vnf *vnf);

#endif /* SIMPLE_FWD_VNF_CORE_H_ */
