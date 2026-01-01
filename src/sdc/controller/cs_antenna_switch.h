/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef CS_ANTENNA_SWITCH_H__
#define CS_ANTENNA_SWITCH_H__

#include <stdint.h>

/** @brief Antenna switching Callback for use in Channel Sounding.
 *
 *  @param[in] antenna_index the index of the antenna being switched to.
 */
static inline void cs_antenna_switch_func(uint8_t antenna_number)
{
	(void)antenna_number;
	/* Channel Sounding antenna switching not implemented */
}

/** @brief Function to initialize the pins used by antenna switching in Channel Sounding. */
static inline void cs_antenna_switch_init(void)
{
	/* Channel Sounding antenna switching not implemented */
}

/** @brief Function to clear the pins used by antenna switching in Channel Sounding. */
static inline void cs_antenna_switch_clear(void)
{
	/* Channel Sounding antenna switching not implemented */
}

#endif /* CS_ANTENNA_SWITCH_H__ */
