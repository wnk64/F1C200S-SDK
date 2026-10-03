// SPDX-License-Identifier: GPL-2.0-only
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/module.h>
#include <linux/reboot.h>

#define POWER_GPIO 132 /* PE4: gpiochip0 base0 + bank E 4*32 + pin4 */

static void f1_power_off(void)
{
    pr_emerg("f1-board-power: filesystem shutdown complete, asserting PE4\n");
    gpio_set_value(POWER_GPIO, 1);
    mdelay(2500);
    pr_emerg("f1-board-power: supply did not cut after 2.5 seconds\n");
}

static int __init f1_power_init(void)
{
    int ret;
    if (pm_power_off) return -EBUSY;
    if (!gpio_is_valid(POWER_GPIO) || gpio_cansleep(POWER_GPIO)) return -EINVAL;
    ret = gpio_request_one(POWER_GPIO, GPIOF_OUT_INIT_LOW, "f1-total-power-off");
    if (ret) return ret;
    pm_power_off = f1_power_off;
    pr_info("f1-board-power: PE4 low; poweroff asserts high, reboot leaves low\n");
    return 0;
}

static void __exit f1_power_exit(void)
{
    if (pm_power_off == f1_power_off) pm_power_off = NULL;
    gpio_set_value(POWER_GPIO, 0);
    gpio_free(POWER_GPIO);
}

module_init(f1_power_init);
module_exit(f1_power_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("F1C200S late PE4 total supply poweroff hook");
