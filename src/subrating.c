#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(zmk_sdc_subrating, CONFIG_ZMK_LOG_LEVEL);

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>

#include <zmk/event_manager.h>
#include <zmk/events/activity_state_changed.h>

#if IS_ENABLED(CONFIG_BT_SUBRATING)

/* Idle state: high subrating for power savings */
#define SUBRATE_IDLE_MIN        1
#define SUBRATE_IDLE_MAX        15
#define SUBRATE_IDLE_CN         2
#define SUBRATE_MAX_LATENCY     10
#define SUBRATE_TIMEOUT         400  /* 4 seconds in 10ms units */

#if IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
/* Active state: minimal subrating for fast response */
#define SUBRATE_ACTIVE_MIN      1
#define SUBRATE_ACTIVE_MAX      2
#define SUBRATE_ACTIVE_CN       1

static void set_active_subrate(struct bt_conn *conn, void *data) {
    struct bt_conn_info info;

    bt_conn_get_info(conn, &info);

    if (info.state == BT_CONN_STATE_CONNECTED) {
        const struct bt_conn_le_subrate_param params = {
            .subrate_min = SUBRATE_ACTIVE_MIN,
            .subrate_max = SUBRATE_ACTIVE_MAX,
            .max_latency = 0,
            .continuation_number = SUBRATE_ACTIVE_CN,
            .supervision_timeout = SUBRATE_TIMEOUT,
        };

        int err = bt_conn_le_subrate_request(conn, &params);
        if (err && err != -EALREADY) {
            LOG_DBG("Failed to request active subrate: %d", err);
        }
    }
}

static void set_idle_subrate(struct bt_conn *conn, void *data) {
    struct bt_conn_info info;

    bt_conn_get_info(conn, &info);

    if (info.state == BT_CONN_STATE_CONNECTED) {
        const struct bt_conn_le_subrate_param params = {
            .subrate_min = SUBRATE_IDLE_MIN,
            .subrate_max = SUBRATE_IDLE_MAX,
            .max_latency = SUBRATE_MAX_LATENCY,
            .continuation_number = SUBRATE_IDLE_CN,
            .supervision_timeout = SUBRATE_TIMEOUT,
        };

        int err = bt_conn_le_subrate_request(conn, &params);
        if (err && err != -EALREADY) {
            LOG_DBG("Failed to request idle subrate: %d", err);
        }
    }
}

static void subrate_active(void) {
    LOG_DBG("Setting active subrate (factor=%d-%d, cn=%d)",
            SUBRATE_ACTIVE_MIN, SUBRATE_ACTIVE_MAX, SUBRATE_ACTIVE_CN);
    bt_conn_foreach(BT_CONN_TYPE_LE, set_active_subrate, NULL);
}

static void subrate_idle(void) {
    LOG_DBG("Setting idle subrate (factor=%d-%d, cn=%d)",
            SUBRATE_IDLE_MIN, SUBRATE_IDLE_MAX, SUBRATE_IDLE_CN);
    bt_conn_foreach(BT_CONN_TYPE_LE, set_idle_subrate, NULL);
}
#endif /* CONFIG_ZMK_SPLIT_ROLE_CENTRAL */

static void subrate_changed_cb(struct bt_conn *conn,
                               const struct bt_conn_le_subrate_changed *params)
{
    if (params->status == BT_HCI_ERR_SUCCESS) {
        LOG_INF("Subrating: factor=%d, cn=%d",
                params->factor, params->continuation_number);
    } else {
        LOG_WRN("Subrating failed: 0x%02x", params->status);
    }
}

BT_CONN_CB_DEFINE(subrating_conn_cb) = {
    .subrate_changed = subrate_changed_cb,
};

#if IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
/* Only central actively requests subrate changes based on activity.
 * Peripheral just supports subrating passively.
 */
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

/* Set idle subrate defaults for new connections */
static int zmk_sdc_subrating_init(void) {
    const struct bt_conn_le_subrate_param params = {
        .subrate_min = SUBRATE_IDLE_MIN,
        .subrate_max = SUBRATE_IDLE_MAX,
        .max_latency = SUBRATE_MAX_LATENCY,
        .continuation_number = SUBRATE_IDLE_CN,
        .supervision_timeout = SUBRATE_TIMEOUT,
    };

    int err = bt_conn_le_subrate_set_defaults(&params);
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
