/*
 * Copyright (c) 2025-2026 carrefinho
 *
 * SPDX-License-Identifier: MIT
 */

#include <errno.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(zmk_sdc_subrating, CONFIG_ZMK_LOG_LEVEL);

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>

#include <zmk/event_manager.h>
#include <zmk/events/activity_state_changed.h>

#if IS_ENABLED(CONFIG_BT_SUBRATING)

#if IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)

#define SUBRATE_TIMEOUT         CONFIG_ZMK_BLE_SUBRATE_TIMEOUT
#define SUBRATE_DORMANT_DELAY_MS CONFIG_ZMK_BLE_SUBRATE_DORMANT_DELAY

/* ACTIVE tier */
#define SUBRATE_ACTIVE_MIN         CONFIG_ZMK_BLE_SUBRATE_ACTIVE_MIN
#define SUBRATE_ACTIVE_MAX         CONFIG_ZMK_BLE_SUBRATE_ACTIVE_MAX
#define SUBRATE_ACTIVE_CN          CONFIG_ZMK_BLE_SUBRATE_ACTIVE_CN
#define SUBRATE_ACTIVE_MAX_LATENCY CONFIG_ZMK_BLE_SUBRATE_ACTIVE_MAX_LATENCY

BUILD_ASSERT(SUBRATE_ACTIVE_MAX >= SUBRATE_ACTIVE_MIN,
    "ACTIVE_MAX must be >= ACTIVE_MIN");
BUILD_ASSERT(SUBRATE_ACTIVE_MAX * (SUBRATE_ACTIVE_MAX_LATENCY + 1) <= 500,
    "ACTIVE_MAX * (ACTIVE_MAX_LATENCY + 1) must be <= 500");
BUILD_ASSERT(SUBRATE_ACTIVE_CN < SUBRATE_ACTIVE_MAX,
    "ACTIVE_CN must be < ACTIVE_MAX");
BUILD_ASSERT(SUBRATE_TIMEOUT * 2 > 3 * SUBRATE_ACTIVE_MAX * (SUBRATE_ACTIVE_MAX_LATENCY + 1),
    "TIMEOUT too low for active tier");

static const struct bt_conn_le_subrate_param active_params = {
    .subrate_min = SUBRATE_ACTIVE_MIN,
    .subrate_max = SUBRATE_ACTIVE_MAX,
    .max_latency = SUBRATE_ACTIVE_MAX_LATENCY,
    .continuation_number = SUBRATE_ACTIVE_CN,
    .supervision_timeout = SUBRATE_TIMEOUT,
};

/* IDLE tier */
#define SUBRATE_IDLE_MIN         CONFIG_ZMK_BLE_SUBRATE_IDLE_MIN
#define SUBRATE_IDLE_MAX         CONFIG_ZMK_BLE_SUBRATE_IDLE_MAX
#define SUBRATE_IDLE_CN          CONFIG_ZMK_BLE_SUBRATE_IDLE_CN
#define SUBRATE_IDLE_MAX_LATENCY CONFIG_ZMK_BLE_SUBRATE_IDLE_MAX_LATENCY

BUILD_ASSERT(SUBRATE_IDLE_MAX >= SUBRATE_IDLE_MIN,
    "IDLE_MAX must be >= IDLE_MIN");
BUILD_ASSERT(SUBRATE_IDLE_MAX * (SUBRATE_IDLE_MAX_LATENCY + 1) <= 500,
    "IDLE_MAX * (IDLE_MAX_LATENCY + 1) must be <= 500");
BUILD_ASSERT(SUBRATE_IDLE_CN < SUBRATE_IDLE_MAX,
    "IDLE_CN must be < IDLE_MAX");
BUILD_ASSERT(SUBRATE_TIMEOUT * 2 > 3 * SUBRATE_IDLE_MAX * (SUBRATE_IDLE_MAX_LATENCY + 1),
    "TIMEOUT too low for idle tier");

static const struct bt_conn_le_subrate_param idle_params = {
    .subrate_min = SUBRATE_IDLE_MIN,
    .subrate_max = SUBRATE_IDLE_MAX,
    .max_latency = SUBRATE_IDLE_MAX_LATENCY,
    .continuation_number = SUBRATE_IDLE_CN,
    .supervision_timeout = SUBRATE_TIMEOUT,
};

/* DORMANT tier */
#define SUBRATE_DORMANT_MIN         CONFIG_ZMK_BLE_SUBRATE_DORMANT_MIN
#define SUBRATE_DORMANT_MAX         CONFIG_ZMK_BLE_SUBRATE_DORMANT_MAX
#define SUBRATE_DORMANT_CN          CONFIG_ZMK_BLE_SUBRATE_DORMANT_CN
#define SUBRATE_DORMANT_MAX_LATENCY CONFIG_ZMK_BLE_SUBRATE_DORMANT_MAX_LATENCY

BUILD_ASSERT(SUBRATE_DORMANT_MAX >= SUBRATE_DORMANT_MIN,
    "DORMANT_MAX must be >= DORMANT_MIN");
BUILD_ASSERT(SUBRATE_DORMANT_MAX * (SUBRATE_DORMANT_MAX_LATENCY + 1) <= 500,
    "DORMANT_MAX * (DORMANT_MAX_LATENCY + 1) must be <= 500");
BUILD_ASSERT(SUBRATE_DORMANT_CN < SUBRATE_DORMANT_MAX,
    "DORMANT_CN must be < DORMANT_MAX");
BUILD_ASSERT(SUBRATE_TIMEOUT * 2 > 3 * SUBRATE_DORMANT_MAX * (SUBRATE_DORMANT_MAX_LATENCY + 1),
    "TIMEOUT too low for dormant tier");

static const struct bt_conn_le_subrate_param dormant_params = {
    .subrate_min = SUBRATE_DORMANT_MIN,
    .subrate_max = SUBRATE_DORMANT_MAX,
    .max_latency = SUBRATE_DORMANT_MAX_LATENCY,
    .continuation_number = SUBRATE_DORMANT_CN,
    .supervision_timeout = SUBRATE_TIMEOUT,
};

#if IS_ENABLED(CONFIG_ZMK_BLE_HOST_CONN_PARAM_DORMANT)

/* Host connection parameters for dormant tier */
#define HOST_DORMANT_INT_MIN    CONFIG_ZMK_BLE_HOST_CONN_DORMANT_INT_MIN
#define HOST_DORMANT_INT_MAX    CONFIG_ZMK_BLE_HOST_CONN_DORMANT_INT_MAX
#define HOST_DORMANT_LATENCY    CONFIG_ZMK_BLE_HOST_CONN_DORMANT_LATENCY
#define HOST_DORMANT_TIMEOUT    CONFIG_ZMK_BLE_HOST_CONN_DORMANT_TIMEOUT

/* Apple guidelines validation */
BUILD_ASSERT(HOST_DORMANT_INT_MIN >= 12,
    "Interval min must be >= 15ms (12 units)");
BUILD_ASSERT(HOST_DORMANT_INT_MIN % 12 == 0,
    "Interval min must be a multiple of 15ms (12 units)");
BUILD_ASSERT(HOST_DORMANT_INT_MAX >= HOST_DORMANT_INT_MIN,
    "Interval max must be >= interval min");
BUILD_ASSERT(HOST_DORMANT_INT_MAX == HOST_DORMANT_INT_MIN ||
             HOST_DORMANT_INT_MAX >= HOST_DORMANT_INT_MIN + 12,
    "Interval max must equal min or be at least 15ms greater");
BUILD_ASSERT(HOST_DORMANT_LATENCY <= 30,
    "Latency must be <= 30");
BUILD_ASSERT((HOST_DORMANT_INT_MAX * 125 / 100) * (HOST_DORMANT_LATENCY + 1) <= 6000,
    "Interval max * (latency + 1) must be <= 6 seconds");
BUILD_ASSERT(HOST_DORMANT_TIMEOUT * 10 >
             (HOST_DORMANT_INT_MAX * 125 / 100) * (HOST_DORMANT_LATENCY + 1) * 3,
    "Timeout must be > interval_max * (latency + 1) * 3");

static const struct bt_le_conn_param host_dormant_params = {
    .interval_min = HOST_DORMANT_INT_MIN,
    .interval_max = HOST_DORMANT_INT_MAX,
    .latency = HOST_DORMANT_LATENCY,
    .timeout = HOST_DORMANT_TIMEOUT,
};

/* Normal host parameters (from ZMK defaults) */
static const struct bt_le_conn_param host_active_params = {
    .interval_min = CONFIG_BT_PERIPHERAL_PREF_MIN_INT,
    .interval_max = CONFIG_BT_PERIPHERAL_PREF_MAX_INT,
    .latency = CONFIG_BT_PERIPHERAL_PREF_LATENCY,
    .timeout = CONFIG_BT_PERIPHERAL_PREF_TIMEOUT,
};

static void apply_conn_param_to_host(struct bt_conn *conn, void *data) {
    const struct bt_le_conn_param *params = data;
    struct bt_conn_info info;

    bt_conn_get_info(conn, &info);

    /* Host connections are where we act as peripheral */
    if (info.role == BT_CONN_ROLE_PERIPHERAL && info.state == BT_CONN_STATE_CONNECTED) {
        int err = bt_conn_le_param_update(conn, params);
        if (err && err != -EALREADY) {
            LOG_WRN("Failed to request host conn param update: %d", err);
        }
    }
}

#endif /* CONFIG_ZMK_BLE_HOST_CONN_PARAM_DORMANT */

static void dormant_timer_handler(struct k_work *work);
static K_WORK_DELAYABLE_DEFINE(dormant_work, dormant_timer_handler);

enum subrate_tier { TIER_ACTIVE, TIER_IDLE, TIER_DORMANT };
static enum subrate_tier current_tier = TIER_IDLE;

static void apply_subrate_to_conn(struct bt_conn *conn, void *data) {
    const struct bt_conn_le_subrate_param *params = data;
    struct bt_conn_info info;

    bt_conn_get_info(conn, &info);

    if (info.role == BT_CONN_ROLE_CENTRAL && info.state == BT_CONN_STATE_CONNECTED) {
        int err = bt_conn_le_subrate_request(conn, params);
        if (err && err != -EALREADY) {
            LOG_WRN("Failed to request subrate: %d", err);
        }
    }
}

static void set_tier(enum subrate_tier tier) {
    if (tier == current_tier) {
        return;
    }

    enum subrate_tier prev_tier = current_tier;
    current_tier = tier;

    const struct bt_conn_le_subrate_param *params;
    const char *tier_name;

    switch (tier) {
    case TIER_ACTIVE:
        params = &active_params;
        tier_name = "ACTIVE";
        break;
    case TIER_IDLE:
        params = &idle_params;
        tier_name = "IDLE";
        break;
    case TIER_DORMANT:
        params = &dormant_params;
        tier_name = "DORMANT";
        break;
    default:
        return;
    }

    LOG_INF("Subrating tier: %s (factor=%d-%d, latency=%d, cn=%d)",
            tier_name, params->subrate_min, params->subrate_max,
            params->max_latency, params->continuation_number);

    bt_conn_le_subrate_set_defaults(params);
    bt_conn_foreach(BT_CONN_TYPE_LE, apply_subrate_to_conn, (void *)params);

#if IS_ENABLED(CONFIG_ZMK_BLE_HOST_CONN_PARAM_DORMANT)
    /* Update host connection parameters when entering/exiting dormant */
    if (tier == TIER_DORMANT) {
        LOG_INF("Host conn params: dormant (interval=%d-%d, latency=%d)",
                HOST_DORMANT_INT_MIN, HOST_DORMANT_INT_MAX, HOST_DORMANT_LATENCY);
        bt_conn_foreach(BT_CONN_TYPE_LE, apply_conn_param_to_host,
                        (void *)&host_dormant_params);
    } else if (prev_tier == TIER_DORMANT) {
        LOG_INF("Host conn params: active (interval=%d-%d, latency=%d)",
                CONFIG_BT_PERIPHERAL_PREF_MIN_INT, CONFIG_BT_PERIPHERAL_PREF_MAX_INT,
                CONFIG_BT_PERIPHERAL_PREF_LATENCY);
        bt_conn_foreach(BT_CONN_TYPE_LE, apply_conn_param_to_host,
                        (void *)&host_active_params);
    }
#endif
}

static void dormant_timer_handler(struct k_work *work) {
    set_tier(TIER_DORMANT);
}

static void subrate_active(void) {
    k_work_cancel_delayable(&dormant_work);
    set_tier(TIER_ACTIVE);
}

static void subrate_idle(void) {
    k_work_cancel_delayable(&dormant_work);
    set_tier(TIER_IDLE);
    k_work_schedule(&dormant_work, K_MSEC(SUBRATE_DORMANT_DELAY_MS));
}

static int subrating_activity_listener(const zmk_event_t *eh) {
    struct zmk_activity_state_changed *ev = as_zmk_activity_state_changed(eh);
    if (ev == NULL) {
        return -ENOTSUP;
    }

    switch (ev->state) {
    case ZMK_ACTIVITY_ACTIVE:
        subrate_active();
        break;
    case ZMK_ACTIVITY_IDLE:
    case ZMK_ACTIVITY_SLEEP:
        subrate_idle();
        break;
    default:
        LOG_WRN("Unhandled activity state: %d", ev->state);
        return -EINVAL;
    }
    return 0;
}

ZMK_LISTENER(sdc_subrating, subrating_activity_listener);
ZMK_SUBSCRIPTION(sdc_subrating, zmk_activity_state_changed);

static int zmk_sdc_subrating_init(void) {
    int err = bt_conn_le_subrate_set_defaults(&idle_params);
    if (err) {
        LOG_ERR("Failed to set subrating defaults: %d", err);
        return err;
    }

    LOG_INF("Subrating: active=%d-%d/%d, idle=%d-%d/%d, dormant=%d-%d/%d (delay=%ds)",
            SUBRATE_ACTIVE_MIN, SUBRATE_ACTIVE_MAX, SUBRATE_ACTIVE_MAX_LATENCY,
            SUBRATE_IDLE_MIN, SUBRATE_IDLE_MAX, SUBRATE_IDLE_MAX_LATENCY,
            SUBRATE_DORMANT_MIN, SUBRATE_DORMANT_MAX, SUBRATE_DORMANT_MAX_LATENCY,
            SUBRATE_DORMANT_DELAY_MS / 1000);

    return 0;
}

SYS_INIT(zmk_sdc_subrating_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);

#endif /* CONFIG_ZMK_SPLIT_ROLE_CENTRAL */

#if IS_ENABLED(CONFIG_ZMK_SPLIT) && !IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)

/*
 * Peripheral half: request ACTIVE subrate when local activity is detected.
 * This ensures the peripheral can send keystrokes quickly without waiting
 * for the central to initiate a tier change.
 */

static const struct bt_conn_le_subrate_param peripheral_active_params = {
    .subrate_min = 1,
    .subrate_max = 2,
    .max_latency = 0,
    .continuation_number = 1,
    .supervision_timeout = 400, /* 4 seconds */
};

static bool peripheral_is_active = false;

static void apply_subrate_to_peripheral_conn(struct bt_conn *conn, void *data) {
    const struct bt_conn_le_subrate_param *params = data;
    struct bt_conn_info info;

    bt_conn_get_info(conn, &info);

    if (info.role == BT_CONN_ROLE_PERIPHERAL && info.state == BT_CONN_STATE_CONNECTED) {
        int err = bt_conn_le_subrate_request(conn, params);
        if (err && err != -EALREADY) {
            LOG_WRN("Peripheral failed to request subrate: %d", err);
        }
    }
}

static int peripheral_subrating_activity_listener(const zmk_event_t *eh) {
    struct zmk_activity_state_changed *ev = as_zmk_activity_state_changed(eh);
    if (ev == NULL) {
        return -ENOTSUP;
    }

    if (ev->state == ZMK_ACTIVITY_ACTIVE) {
        if (!peripheral_is_active) {
            LOG_INF("Peripheral requesting ACTIVE subrate");
            bt_conn_foreach(BT_CONN_TYPE_LE, apply_subrate_to_peripheral_conn,
                            (void *)&peripheral_active_params);
            peripheral_is_active = true;
        }
    } else {
        peripheral_is_active = false;
    }

    return 0;
}

ZMK_LISTENER(sdc_subrating_peripheral, peripheral_subrating_activity_listener);
ZMK_SUBSCRIPTION(sdc_subrating_peripheral, zmk_activity_state_changed);

#endif /* CONFIG_ZMK_SPLIT && !CONFIG_ZMK_SPLIT_ROLE_CENTRAL */

/* Callbacks for logging (all builds) */

static void subrate_changed_cb(struct bt_conn *conn,
                               const struct bt_conn_le_subrate_changed *params)
{
    struct bt_conn_info info;
    char addr_str[BT_ADDR_LE_STR_LEN];

    bt_conn_get_info(conn, &info);
    bt_addr_le_to_str(bt_conn_get_dst(conn), addr_str, sizeof(addr_str));

    const char *role = info.role == BT_CONN_ROLE_CENTRAL ? "central" : "peripheral";

    if (params->status == BT_HCI_ERR_SUCCESS) {
        LOG_INF("Subrating [%s %s]: factor=%d, cn=%d",
                role, addr_str, params->factor, params->continuation_number);
    } else {
        LOG_WRN("Subrating failed [%s %s]: 0x%02x", role, addr_str, params->status);
    }
}

#if IS_ENABLED(CONFIG_BT_USER_PHY_UPDATE)
static const char *phy_to_str(uint8_t phy) {
    switch (phy) {
    case BT_GAP_LE_PHY_1M:
        return "1M";
    case BT_GAP_LE_PHY_2M:
        return "2M";
    case BT_GAP_LE_PHY_CODED:
        return "Coded";
    default:
        return "Unknown";
    }
}

static void phy_updated_cb(struct bt_conn *conn, struct bt_conn_le_phy_info *info)
{
    char addr_str[BT_ADDR_LE_STR_LEN];
    bt_addr_le_to_str(bt_conn_get_dst(conn), addr_str, sizeof(addr_str));

    LOG_INF("PHY updated [%s]: tx=%s, rx=%s",
            addr_str, phy_to_str(info->tx_phy), phy_to_str(info->rx_phy));
}
#endif

static void le_param_updated_cb(struct bt_conn *conn, uint16_t interval,
                                uint16_t latency, uint16_t timeout)
{
    char addr_str[BT_ADDR_LE_STR_LEN];
    bt_addr_le_to_str(bt_conn_get_dst(conn), addr_str, sizeof(addr_str));

    /* interval is in 1.25ms units, timeout is in 10ms units */
    uint32_t interval_us = interval * 1250;  /* Convert to microseconds */
    LOG_INF("Conn params [%s]: interval=%d.%02dms, latency=%d, timeout=%dms",
            addr_str, interval_us / 1000, (interval_us % 1000) / 10,
            latency, timeout * 10);
}

BT_CONN_CB_DEFINE(subrating_conn_cb) = {
    .subrate_changed = subrate_changed_cb,
#if IS_ENABLED(CONFIG_BT_USER_PHY_UPDATE)
    .le_phy_updated = phy_updated_cb,
#endif
    .le_param_updated = le_param_updated_cb,
};

#endif /* CONFIG_BT_SUBRATING */
