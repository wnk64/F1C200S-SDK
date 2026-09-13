// SPDX-License-Identifier: GPL-2.0
/* TK032F8004/LG4573A three-wire SPI initialisation only. */
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>

struct tk032f8004 {
    struct gpio_desc *cs;
    struct gpio_desc *sclk;
    struct gpio_desc *mosi;
};

static void tk_write_byte(struct tk032f8004 *tk, u8 value)
{
    int bit;

    for (bit = 7; bit >= 0; --bit) {
        gpiod_set_value_cansleep(tk->mosi, !!(value & BIT(bit)));
        gpiod_set_value_cansleep(tk->sclk, 0);
        ndelay(200);
        gpiod_set_value_cansleep(tk->sclk, 1);
        ndelay(200);
    }
}

static void tk_command(struct tk032f8004 *tk, u8 command)
{
    gpiod_set_value_cansleep(tk->cs, 0);
    tk_write_byte(tk, 0x70);
    tk_write_byte(tk, command);
    gpiod_set_value_cansleep(tk->cs, 1);
}

static void tk_data(struct tk032f8004 *tk, u8 data)
{
    gpiod_set_value_cansleep(tk->cs, 0);
    tk_write_byte(tk, 0x72);
    tk_write_byte(tk, data);
    gpiod_set_value_cansleep(tk->cs, 1);
}

static void tk_init_sequence(struct tk032f8004 *tk)
{
    /* LG4573A sequence copied from the supplied TK032F8004 reference. */
    static const u8 sequence[][2] = {
        { 0x20, 0xff }, { 0x29, 0xff },
        { 0xb1, 0xff }, { 0x00, 0x00 }, { 0x00, 0x14 }, { 0x00, 0x06 },
        { 0xb2, 0xff }, { 0x00, 0x10 }, { 0x00, 0xc8 },
        { 0xb3, 0xff }, { 0x00, 0x00 },
        { 0xb4, 0xff }, { 0x00, 0x04 },
        { 0xb5, 0xff }, { 0x00, 0x10 }, { 0x00, 0x30 }, { 0x00, 0x30 }, { 0x00, 0x00 }, { 0x00, 0x00 },
        { 0xb6, 0xff }, { 0x00, 0x01 }, { 0x00, 0x18 }, { 0x00, 0x02 }, { 0x00, 0x40 }, { 0x00, 0x10 }, { 0x00, 0x00 },
        { 0xc0, 0xff }, { 0x00, 0x01 }, { 0x00, 0x18 },
        { 0xc3, 0xff }, { 0x00, 0x03 }, { 0x00, 0x04 }, { 0x00, 0x03 }, { 0x00, 0x03 }, { 0x00, 0x03 },
        { 0xc4, 0xff }, { 0x00, 0x02 }, { 0x00, 0x23 }, { 0x00, 0x11 }, { 0x00, 0x12 }, { 0x00, 0x02 }, { 0x00, 0x77 },
        { 0xc5, 0xff }, { 0x00, 0x73 },
        { 0xc6, 0xff }, { 0x00, 0x24 }, { 0x00, 0x60 }, { 0x00, 0x00 },
        { 0xd0, 0xff }, { 0x00, 0x14 }, { 0x00, 0x01 }, { 0x00, 0x53 }, { 0x00, 0x25 }, { 0x00, 0x02 }, { 0x00, 0x02 }, { 0x00, 0x66 }, { 0x00, 0x14 }, { 0x00, 0x03 },
        { 0xd1, 0xff }, { 0x00, 0x14 }, { 0x00, 0x01 }, { 0x00, 0x53 }, { 0x00, 0x07 }, { 0x00, 0x02 }, { 0x00, 0x02 }, { 0x00, 0x66 }, { 0x00, 0x14 }, { 0x00, 0x03 },
        { 0xd2, 0xff }, { 0x00, 0x14 }, { 0x00, 0x01 }, { 0x00, 0x53 }, { 0x00, 0x25 }, { 0x00, 0x02 }, { 0x00, 0x02 }, { 0x00, 0x66 }, { 0x00, 0x14 }, { 0x00, 0x03 },
        { 0xd3, 0xff }, { 0x00, 0x14 }, { 0x00, 0x01 }, { 0x00, 0x53 }, { 0x00, 0x07 }, { 0x00, 0x02 }, { 0x00, 0x02 }, { 0x00, 0x66 }, { 0x00, 0x14 }, { 0x00, 0x03 },
        { 0xd4, 0xff }, { 0x00, 0x14 }, { 0x00, 0x01 }, { 0x00, 0x53 }, { 0x00, 0x25 }, { 0x00, 0x02 }, { 0x00, 0x02 }, { 0x00, 0x66 }, { 0x00, 0x14 }, { 0x00, 0x03 },
        { 0xd5, 0xff }, { 0x00, 0x14 }, { 0x00, 0x01 }, { 0x00, 0x53 }, { 0x00, 0x07 }, { 0x00, 0x02 }, { 0x00, 0x02 }, { 0x00, 0x66 }, { 0x00, 0x14 }, { 0x00, 0x03 },
        { 0x11, 0xff }, { 0x3a, 0xff }, { 0x00, 0x66 },
        /* LG4573A 0x36: retain BGR (bit 3), clear FH (bit 1). */
        { 0x36, 0xff }, { 0x00, 0x28 }, { 0x29, 0xff },
    };
    unsigned int i;

    for (i = 0; i < ARRAY_SIZE(sequence); ++i) {
        if (sequence[i][1] == 0xff) {
            tk_command(tk, sequence[i][0]);
            if (sequence[i][0] == 0x11)
                msleep(120);
        } else {
            tk_data(tk, sequence[i][1]);
        }
    }
}

static int tk_probe(struct platform_device *pdev)
{
    struct tk032f8004 *tk;

    tk = devm_kzalloc(&pdev->dev, sizeof(*tk), GFP_KERNEL);
    if (!tk)
        return -ENOMEM;

    tk->cs = devm_gpiod_get(&pdev->dev, "cs", GPIOD_OUT_HIGH);
    if (IS_ERR(tk->cs))
        return PTR_ERR(tk->cs);
    tk->sclk = devm_gpiod_get(&pdev->dev, "sclk", GPIOD_OUT_HIGH);
    if (IS_ERR(tk->sclk))
        return PTR_ERR(tk->sclk);
    tk->mosi = devm_gpiod_get(&pdev->dev, "mosi", GPIOD_OUT_LOW);
    if (IS_ERR(tk->mosi))
        return PTR_ERR(tk->mosi);

    tk_init_sequence(tk);
    dev_info(&pdev->dev, "LG4573A init complete (CS=PA0 SCLK=PA2 MOSI=PA1)\n");
    return 0;
}

static const struct of_device_id tk_of_match[] = {
    { .compatible = "tk,tk032f8004-init" },
    { }
};
MODULE_DEVICE_TABLE(of, tk_of_match);

static struct platform_driver tk_driver = {
    .probe = tk_probe,
    .driver = {
        .name = "tk032f8004-init",
        .of_match_table = tk_of_match,
    },
};
module_platform_driver(tk_driver);

MODULE_DESCRIPTION("TK032F8004 LG4573A GPIO initialisation driver");
MODULE_LICENSE("GPL");
