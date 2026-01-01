/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/**
 * @file mpsl_work.h
 *
 * @defgroup mpsl_work Multiprotocol Service Layer work queue.
 *
 * @brief Internal MPSL work queue.
 * @{
 */

#ifndef MPSL_WORK__
#define MPSL_WORK__

#ifdef __cplusplus
extern "C" {
#endif

#include <zephyr/kernel.h>

extern struct k_work_q mpsl_work_q;

/** @brief Submit a work item to the MPSL work queue. */
static inline void mpsl_work_submit(struct k_work *work)
{
	if (k_work_submit_to_queue(&mpsl_work_q, work) < 0) {
		__ASSERT(false, "k_work_submit_to_queue() failed.");
	}
}

/** @brief Submit an idle work item to the MPSL work queue after a delay. */
static inline void mpsl_work_schedule(struct k_work_delayable *dwork, k_timeout_t delay)
{
	if (k_work_schedule_for_queue(&mpsl_work_q, dwork, delay) < 0) {
		__ASSERT(false, "k_work_schedule_for_queue() failed.");
	}
}

/** @brief Reschedule a work item to the MPSL work queue after a delay. */
static inline void mpsl_work_reschedule(struct k_work_delayable *dwork, k_timeout_t delay)
{
	if (k_work_reschedule_for_queue(&mpsl_work_q, dwork, delay) < 0) {
		__ASSERT(false, "k_work_reschedule_for_queue() failed.");
	}
}

#ifdef __cplusplus
}
#endif

#endif /* MPSL_WORK__ */

/**@} */
