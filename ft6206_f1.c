// SPDX-License-Identifier: GPL-2.0
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/interrupt.h>

#define FT5X06_REG_TD_STATUS 0x02
#define FT5X06_REG_P1_XH     0x03

struct ft6206_f1 {
    struct i2c_client *client;
    struct input_dev *input;
    struct gpio_desc *reset;
    u16 max_x, max_y;
    bool swap_xy, invert_x, invert_y;
};

static irqreturn_t ft6206_f1_irq(int irq, void *data)
{
    struct ft6206_f1 *ts = data;
    u8 report[6];
    int count, x, y;

    if (i2c_smbus_read_i2c_block_data(ts->client, FT5X06_REG_TD_STATUS,
                                      sizeof(report), report) != sizeof(report))
        return IRQ_HANDLED;
    count = report[0] & 0x0f;
    if (!count) {
        input_mt_slot(ts->input, 0);
        input_mt_report_slot_state(ts->input, MT_TOOL_FINGER, false);
        input_sync(ts->input);
        return IRQ_HANDLED;
    }
    x = ((report[1] & 0x0f) << 8) | report[2];
    y = ((report[3] & 0x0f) << 8) | report[4];
    if (ts->swap_xy) swap(x, y);
    if (ts->invert_x) x = ts->max_x - x;
    if (ts->invert_y) y = ts->max_y - y;
    x = clamp(x, 0, (int)ts->max_x);
    y = clamp(y, 0, (int)ts->max_y);
    input_mt_slot(ts->input, 0);
    input_mt_report_slot_state(ts->input, MT_TOOL_FINGER, true);
    input_report_abs(ts->input, ABS_MT_POSITION_X, x);
    input_report_abs(ts->input, ABS_MT_POSITION_Y, y);
    input_mt_sync_frame(ts->input);
    input_sync(ts->input);
    return IRQ_HANDLED;
}

static int ft6206_f1_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    struct ft6206_f1 *ts;
    struct input_dev *input;
    u32 value;
    int ret;

    ts = devm_kzalloc(&client->dev, sizeof(*ts), GFP_KERNEL);
    input = devm_input_allocate_device(&client->dev);
    if (!ts || !input) return -ENOMEM;
    ts->client = client;
    ts->input = input;
    ts->max_x = 479;
    ts->max_y = 799;
    if (!of_property_read_u32(client->dev.of_node, "touchscreen-size-x", &value)) ts->max_x = value - 1;
    if (!of_property_read_u32(client->dev.of_node, "touchscreen-size-y", &value)) ts->max_y = value - 1;
    ts->swap_xy = of_property_read_bool(client->dev.of_node, "touchscreen-swapped-x-y");
    ts->invert_x = of_property_read_bool(client->dev.of_node, "touchscreen-inverted-x");
    ts->invert_y = of_property_read_bool(client->dev.of_node, "touchscreen-inverted-y");
    ts->reset = devm_gpiod_get_optional(&client->dev, "reset", GPIOD_OUT_HIGH);
    if (IS_ERR(ts->reset)) return PTR_ERR(ts->reset);
    if (ts->reset) { gpiod_set_value_cansleep(ts->reset, 0); msleep(10); gpiod_set_value_cansleep(ts->reset, 1); msleep(200); }
    input->name = "FT6206 capacitive touch";
    input->id.bustype = BUS_I2C;
    input_set_abs_params(input, ABS_MT_POSITION_X, 0, ts->max_x, 0, 0);
    input_set_abs_params(input, ABS_MT_POSITION_Y, 0, ts->max_y, 0, 0);
    input_mt_init_slots(input, 2, INPUT_MT_DIRECT);
    ret = input_register_device(input);
    if (ret) return ret;
    i2c_set_clientdata(client, ts);
    return devm_request_threaded_irq(&client->dev, client->irq, NULL, ft6206_f1_irq,
                                     IRQF_ONESHOT | IRQF_TRIGGER_FALLING,
                                     dev_name(&client->dev), ts);
}

static const struct of_device_id ft6206_f1_of_match[] = { { .compatible = "focaltech,ft6206" }, {} };
MODULE_DEVICE_TABLE(of, ft6206_f1_of_match);
static const struct i2c_device_id ft6206_f1_id[] = { { "ft6206", 0 }, {} };
MODULE_DEVICE_TABLE(i2c, ft6206_f1_id);
static struct i2c_driver ft6206_f1_driver = { .driver = { .name = "ft6206-f1", .of_match_table = ft6206_f1_of_match }, .probe = ft6206_f1_probe, .id_table = ft6206_f1_id };
module_i2c_driver(ft6206_f1_driver);
MODULE_LICENSE("GPL");
