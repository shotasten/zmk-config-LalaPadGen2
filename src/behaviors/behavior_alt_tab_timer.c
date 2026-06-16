/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT shotaste_behavior_alt_tab_timer

#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <drivers/behavior.h>
#include <zmk/behavior.h>
#include <zmk/events/keycode_state_changed.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

struct behavior_alt_tab_timer_config {
    uint32_t release_after_ms;
};

struct behavior_alt_tab_timer_data {
    struct k_work_delayable release_work;
    uint32_t alt_key;
    bool alt_pressed;
};

static void release_alt(struct behavior_alt_tab_timer_data *data, int64_t timestamp) {
    if (!data->alt_pressed) {
        return;
    }

    raise_zmk_keycode_state_changed_from_encoded(data->alt_key, false, timestamp);
    data->alt_pressed = false;
}

static void release_timer_handler(struct k_work *work) {
    struct k_work_delayable *dwork = k_work_delayable_from_work(work);
    struct behavior_alt_tab_timer_data *data =
        CONTAINER_OF(dwork, struct behavior_alt_tab_timer_data, release_work);

    release_alt(data, k_uptime_get());
}

static int on_pressed(struct zmk_behavior_binding *binding,
                      struct zmk_behavior_binding_event event) {
    const struct device *dev = zmk_behavior_get_binding(binding->behavior_dev);
    const struct behavior_alt_tab_timer_config *config = dev->config;
    struct behavior_alt_tab_timer_data *data = dev->data;

    k_work_cancel_delayable(&data->release_work);

    data->alt_key = binding->param2;
    data->alt_pressed = true;

    raise_zmk_keycode_state_changed_from_encoded(data->alt_key, true, event.timestamp);
    raise_zmk_keycode_state_changed_from_encoded(binding->param1, true, event.timestamp);
    raise_zmk_keycode_state_changed_from_encoded(binding->param1, false, event.timestamp);

    k_work_schedule(&data->release_work, K_MSEC(config->release_after_ms));

    return 0;
}

static int on_released(struct zmk_behavior_binding *binding,
                       struct zmk_behavior_binding_event event) {
    ARG_UNUSED(binding);
    ARG_UNUSED(event);

    return ZMK_BEHAVIOR_OPAQUE;
}

static int behavior_alt_tab_timer_init(const struct device *dev) {
    struct behavior_alt_tab_timer_data *data = dev->data;

    k_work_init_delayable(&data->release_work, release_timer_handler);
    data->alt_key = 0;
    data->alt_pressed = false;

    return 0;
}

static const struct behavior_driver_api behavior_alt_tab_timer_driver_api = {
    .binding_pressed = on_pressed,
    .binding_released = on_released,
};

#define ALT_TAB_TIMER_INST(n)                                                                      \
    static const struct behavior_alt_tab_timer_config behavior_alt_tab_timer_config_##n = {        \
        .release_after_ms = DT_INST_PROP(n, release_after_ms),                                     \
    };                                                                                             \
    static struct behavior_alt_tab_timer_data behavior_alt_tab_timer_data_##n;                     \
    BEHAVIOR_DT_INST_DEFINE(n, behavior_alt_tab_timer_init, NULL,                                  \
                            &behavior_alt_tab_timer_data_##n,                                      \
                            &behavior_alt_tab_timer_config_##n, POST_KERNEL,                       \
                            CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,                                   \
                            &behavior_alt_tab_timer_driver_api);

DT_INST_FOREACH_STATUS_OKAY(ALT_TAB_TIMER_INST)
