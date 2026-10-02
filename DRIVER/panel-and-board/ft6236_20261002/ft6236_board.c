// SPDX-License-Identifier: GPL-2.0
#include <linux/gpio/consumer.h>
#include <linux/gpio/machine.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/property.h>

/* Schematic20260930: P05 reset, P06 INT; PCA parent INT -> PE5. */
static struct gpiod_lookup_table irq_lookup = {
    .dev_id = NULL,
    .table = {
        GPIO_LOOKUP("0-0020", 6, "f1-ft6236-int", GPIO_ACTIVE_LOW),
        { }
    },
};
static struct gpiod_lookup_table reset_lookup = {
    .dev_id = "0-0038",
    .table = {
        GPIO_LOOKUP("0-0020", 5, "reset", GPIO_ACTIVE_LOW),
        { }
    },
};
static const struct property_entry touch_properties[] = {
    PROPERTY_ENTRY_U32("touchscreen-size-x", 800),
    PROPERTY_ENTRY_U32("touchscreen-size-y", 480),
    PROPERTY_ENTRY_BOOL("touchscreen-inverted-y"),
    PROPERTY_ENTRY_BOOL("touchscreen-swapped-x-y"),
    { }
};
static struct gpio_desc *interrupt_gpio;
static struct i2c_client *touch_client;

static int __init ft6236_board_init(void)
{
    struct i2c_adapter *adapter;
    struct i2c_board_info info = {
        I2C_BOARD_INFO("ft6236", 0x38),
        .properties = touch_properties,
    };
    int result;

    adapter = i2c_get_adapter(0);
    if (!adapter)
        return -ENODEV;
    gpiod_add_lookup_table(&irq_lookup);
    gpiod_add_lookup_table(&reset_lookup);
    interrupt_gpio = gpiod_get(NULL, "f1-ft6236-int", GPIOD_IN);
    if (IS_ERR(interrupt_gpio)) {
        result = PTR_ERR(interrupt_gpio);
        goto remove_tables;
    }
    info.irq = gpiod_to_irq(interrupt_gpio);
    if (info.irq <= 0) {
        result = info.irq ? info.irq : -EINVAL;
        goto put_gpio;
    }
    touch_client = i2c_new_client_device(adapter, &info);
    i2c_put_adapter(adapter);
    if (IS_ERR(touch_client)) {
        result = PTR_ERR(touch_client);
        goto put_gpio_no_adapter;
    }
    pr_info("ft6236_board: I2C0-0038, PCA9555 P05 reset/P06 IRQ=%d; native480x800\n", info.irq);
    return 0;

put_gpio:
    i2c_put_adapter(adapter);
put_gpio_no_adapter:
    gpiod_put(interrupt_gpio);
    gpiod_remove_lookup_table(&reset_lookup);
    gpiod_remove_lookup_table(&irq_lookup);
    return result;
remove_tables:
    i2c_put_adapter(adapter);
    gpiod_remove_lookup_table(&reset_lookup);
    gpiod_remove_lookup_table(&irq_lookup);
    return result;
}

static void __exit ft6236_board_exit(void)
{
    i2c_unregister_device(touch_client);
    gpiod_put(interrupt_gpio);
    gpiod_remove_lookup_table(&reset_lookup);
    gpiod_remove_lookup_table(&irq_lookup);
}
module_init(ft6236_board_init);
module_exit(ft6236_board_exit);
MODULE_DESCRIPTION("New-board FT6236 wiring registration; upstream EDT touchscreen driver");
MODULE_LICENSE("GPL");
