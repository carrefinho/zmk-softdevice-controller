#include <errno.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(zmk_sdc_subrating, CONFIG_ZMK_LOG_LEVEL);

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>

#include <zmk/event_manager.h>
#include <zmk/events/activity_state_changed.h>

#if IS_ENABLED(CONFIG_BT_SUBRATING)

/* Idle state: high subrating for power savings */
#define SUBRATE_IDLE_MIN        CONFIG_ZMK_BLE_SUBRATE_IDLE_MIN
#define SUBRATE_IDLE_MAX        CONFIG_ZMK_BLE_SUBRATE_IDLE_MAX
#define SUBRATE_IDLE_CN         CONFIG_ZMK_BLE_SUBRATE_IDLE_CN
#define SUBRATE_MAX_LATENCY     CONFIG_ZMK_BLE_SUBRATE_MAX_LATENCY
#define SUBRATE_TIMEOUT         CONFIG_ZMK_BLE_SUBRATE_TIMEOUT

/*
 * Bluetooth Core Spec constraints for subrating parameters:
 * 1. subrate_max * (max_latency + 1) <= 500
 * 2. subrate_max >= subrate_min
 * 3. continuation_number < subrate_max
 * 4. supervision_timeout (ms) > 2 * conn_interval (ms) * subrate_max * (max_latency + 1)
 *
 * For constraint 4, we check against minimum BLE interval (7.5ms).
 * Formula: TIMEOUT * 10ms > 2 * 7.5ms * MAX * (LATENCY + 1)
 *          TIMEOUT * 2 > 3 * MAX * (LATENCY + 1)
 */

/* Idle parameter validation */
BUILD_ASSERT(SUBRATE_IDLE_MAX >= SUBRATE_IDLE_MIN,
    "ZMK_BLE_SUBRATE_IDLE_MAX must be >= ZMK_BLE_SUBRATE_IDLE_MIN");

BUILD_ASSERT(SUBRATE_IDLE_MAX * (SUBRATE_MAX_LATENCY + 1) <= 500,
    "ZMK_BLE_SUBRATE_IDLE_MAX * (ZMK_BLE_SUBRATE_MAX_LATENCY + 1) must be <= 500");

BUILD_ASSERT(SUBRATE_IDLE_CN < SUBRATE_IDLE_MAX,
    "ZMK_BLE_SUBRATE_IDLE_CN must be < ZMK_BLE_SUBRATE_IDLE_MAX");

/* Supervision timeout check for minimum 7.5ms connection interval */
BUILD_ASSERT(SUBRATE_TIMEOUT * 2 > 3 * SUBRATE_IDLE_MAX * (SUBRATE_MAX_LATENCY + 1),
    "ZMK_BLE_SUBRATE_TIMEOUT too low for 7.5ms interval; increase timeout or reduce IDLE_MAX/MAX_LATENCY");

static const struct bt_conn_le_subrate_param idle_params = {
    .subrate_min = SUBRATE_IDLE_MIN,
    .subrate_max = SUBRATE_IDLE_MAX,
    .max_latency = SUBRATE_MAX_LATENCY,
    .continuation_number = SUBRATE_IDLE_CN,
    .supervision_timeout = SUBRATE_TIMEOUT,
};

#if IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
/* Active state: minimal subrating for fast response */
#define SUBRATE_ACTIVE_MIN      CONFIG_ZMK_BLE_SUBRATE_ACTIVE_MIN
#define SUBRATE_ACTIVE_MAX      CONFIG_ZMK_BLE_SUBRATE_ACTIVE_MAX
#define SUBRATE_ACTIVE_CN       CONFIG_ZMK_BLE_SUBRATE_ACTIVE_CN

/* Active parameter validation */
BUILD_ASSERT(SUBRATE_ACTIVE_MAX >= SUBRATE_ACTIVE_MIN,
    "ZMK_BLE_SUBRATE_ACTIVE_MAX must be >= ZMK_BLE_SUBRATE_ACTIVE_MIN");

BUILD_ASSERT(SUBRATE_ACTIVE_MAX * 1 <= 500,  /* max_latency=0 for active */
    "ZMK_BLE_SUBRATE_ACTIVE_MAX must be <= 500");

BUILD_ASSERT(SUBRATE_ACTIVE_CN < SUBRATE_ACTIVE_MAX,
    "ZMK_BLE_SUBRATE_ACTIVE_CN must be < ZMK_BLE_SUBRATE_ACTIVE_MAX");

/* Supervision timeout check for active state (max_latency=0) */
BUILD_ASSERT(SUBRATE_TIMEOUT * 2 > 3 * SUBRATE_ACTIVE_MAX,
    "ZMK_BLE_SUBRATE_TIMEOUT too low for active state; increase timeout or reduce ACTIVE_MAX");

static const struct bt_conn_le_subrate_param active_params = {
    .subrate_min = SUBRATE_ACTIVE_MIN,
    .subrate_max = SUBRATE_ACTIVE_MAX,
    .max_latency = 0,
    .continuation_number = SUBRATE_ACTIVE_CN,
    .supervision_timeout = SUBRATE_TIMEOUT,
};

static void set_active_subrate(struct bt_conn *conn, void *data) {
    struct bt_conn_info info;

    bt_conn_get_info(conn, &info);

    if (info.role == BT_CONN_ROLE_CENTRAL && info.state == BT_CONN_STATE_CONNECTED) {
        int err = bt_conn_le_subrate_request(conn, &active_params);
        if (err && err != -EALREADY) {
            LOG_WRN("Failed to request active subrate: %d", err);
        }
    }
}

static void set_idle_subrate(struct bt_conn *conn, void *data) {
    struct bt_conn_info info;

    bt_conn_get_info(conn, &info);

    if (info.role == BT_CONN_ROLE_CENTRAL && info.state == BT_CONN_STATE_CONNECTED) {
        int err = bt_conn_le_subrate_request(conn, &idle_params);
        if (err && err != -EALREADY) {
            LOG_WRN("Failed to request idle subrate: %d", err);
        }
    }
}

static void subrate_active(void) {
    LOG_DBG("Setting active subrate (factor=%d-%d, cn=%d)",
            SUBRATE_ACTIVE_MIN, SUBRATE_ACTIVE_MAX, SUBRATE_ACTIVE_CN);

    /* Update defaults for future connections */
    bt_conn_le_subrate_set_defaults(&active_params);

    /* Update existing connections */
    bt_conn_foreach(BT_CONN_TYPE_LE, set_active_subrate, NULL);
}

static void subrate_idle(void) {
    LOG_DBG("Setting idle subrate (factor=%d-%d, cn=%d)",
            SUBRATE_IDLE_MIN, SUBRATE_IDLE_MAX, SUBRATE_IDLE_CN);

    /* Update defaults for future connections */
    bt_conn_le_subrate_set_defaults(&idle_params);

    /* Update existing connections */
    bt_conn_foreach(BT_CONN_TYPE_LE, set_idle_subrate, NULL);
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
#endif /* CONFIG_ZMK_SPLIT_ROLE_CENTRAL */

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
#endif /* CONFIG_BT_USER_PHY_UPDATE */

BT_CONN_CB_DEFINE(subrating_conn_cb) = {
    .subrate_changed = subrate_changed_cb,
#if IS_ENABLED(CONFIG_BT_USER_PHY_UPDATE)
    .le_phy_updated = phy_updated_cb,
#endif
};

static int zmk_sdc_subrating_init(void) {
    int err = bt_conn_le_subrate_set_defaults(&idle_params);
    if (err) {
        LOG_ERR("Failed to set subrating defaults: %d", err);
        return err;
    }

    LOG_INF("Subrating defaults: factor=%d-%d, cn=%d",
            SUBRATE_IDLE_MIN, SUBRATE_IDLE_MAX, SUBRATE_IDLE_CN);

    return 0;
}

SYS_INIT(zmk_sdc_subrating_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);

#endif /* CONFIG_BT_SUBRATING */
