/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/**
 * @file mpsl_lib.h
 *
 * @defgroup mpsl_lib Multiprotocol Service Layer library control.
 *
 * @brief Methods for initializing MPSL and required interrupt handlers.
 * @{
 */

#ifndef MPSL_LIB__
#define MPSL_LIB__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/** @brief Initialize MPSL and attach interrupt handlers.
 *
 * @retval   0             MPSL enabled successfully.
 * @retval   -NRF_EPERM    Operation is not supported or MPSL is already initialized.
 * @retval   -NRF_EINVAL   Invalid parameters supplied to MPSL.
 */
int32_t mpsl_lib_init(void);

/** @brief Uninitialize MPSL and disable interrupt handlers.
 *
 * @retval   0             MPSL disabled successfully.
 * @retval   -NRF_EPERM    Operation is not supported.
 */
int32_t mpsl_lib_uninit(void);

#ifdef __cplusplus
}
#endif

#endif /* MPSL_LIB__ */

/**@} */
