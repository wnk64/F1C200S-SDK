// SPDX-License-Identifier: GPL-2.0
/* External DRM panel driver for the TK032F8004 RGB panel. */
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/module.h>
#include <linux/of_graph.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <drm/drm_connector.h>
#include <drm/drm_modes.h>
#include <drm/drm_panel.h>

struct tk032f8004 {
    struct drm_panel panel;
    struct gpio_desc *reset;
    struct gpio_desc *enable;
    struct gpio_desc *backlight;
    struct gpio_desc *cs;
    struct gpio_desc *sclk;
    struct gpio_desc *mosi;
    bool prepared;
    bool enabled;
};

static inline struct tk032f8004 *to_tk(struct drm_panel *panel)
{
    return container_of(panel, struct tk032f8004, panel);
}

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
        { 0x11, 0xff }, { 0x3a, 0xff }, { 0x00, 0x66 }, { 0x36, 0xff }, { 0x00, 0x2a }, { 0x29, 0xff },
    };
    unsigned int i;

    for (i = 0; i < ARRAY_SIZE(sequence); i++) {
        if (sequence[i][1] == 0xff)
            tk_command(tk, sequence[i][0]);
        else
            tk_data(tk, sequence[i][1]);
        if (sequence[i][0] == 0x11)
            msleep(10);
    }
}

static int tk_prepare(struct drm_panel *panel)
{
    struct tk032f8004 *tk = to_tk(panel);
    if (tk->prepared)
        return 0;
    if (tk->enable)
        gpiod_set_value_cansleep(tk->enable, 1);
    if (tk->reset) {
        gpiod_set_value_cansleep(tk->reset, 0);
        msleep(20);
        gpiod_set_value_cansleep(tk->reset, 1);
        msleep(20);
    }
    tk_init_sequence(tk);
    tk->prepared = true;
    return 0;
}

static int tk_unprepare(struct drm_panel *panel)
{
    struct tk032f8004 *tk = to_tk(panel);
    if (!tk->prepared)
        return 0;
    if (tk->backlight)
        gpiod_set_value_cansleep(tk->backlight, 0);
    if (tk->enable)
        gpiod_set_value_cansleep(tk->enable, 0);
    tk->prepared = false;
    return 0;
}

static int tk_enable(struct drm_panel *panel)
{
    struct tk032f8004 *tk = to_tk(panel);
    if (tk->backlight)
        gpiod_set_value_cansleep(tk->backlight, 1);
    tk->enabled = true;
    return 0;
}

static int tk_disable(struct drm_panel *panel)
{
    struct tk032f8004 *tk = to_tk(panel);
    if (tk->backlight)
        gpiod_set_value_cansleep(tk->backlight, 0);
    tk->enabled = false;
    return 0;
}

static int tk_get_modes(struct drm_panel *panel, struct drm_connector *connector)
{
    struct device_node *timings = of_get_child_by_name(panel->dev->of_node, "display-timings");
    struct drm_display_mode mode;
    int ret;
    if (!timings)
        return 0;
    ret = of_get_drm_display_mode(panel->dev->of_node, &mode, NULL, 0);
    of_node_put(timings);
    if (ret)
        return ret;
    mode.type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
    drm_mode_probed_add(connector, drm_mode_duplicate(connector->dev, &mode));
    connector->display_info.width_mm = 69;
    connector->display_info.height_mm = 42;
    return 1;
}

static const struct drm_panel_funcs tk_panel_funcs = {
    .prepare = tk_prepare, .unprepare = tk_unprepare,
    .enable = tk_enable, .disable = tk_disable, .get_modes = tk_get_modes,
};

static int tk_probe(struct platform_device *pdev)
{
    struct tk032f8004 *tk = devm_kzalloc(&pdev->dev, sizeof(*tk), GFP_KERNEL);
    if (!tk) return -ENOMEM;
    tk->reset = devm_gpiod_get_optional(&pdev->dev, "reset", GPIOD_OUT_LOW);
    tk->enable = devm_gpiod_get_optional(&pdev->dev, "enable", GPIOD_OUT_LOW);
    tk->backlight = devm_gpiod_get_optional(&pdev->dev, "backlight", GPIOD_OUT_LOW);
    tk->cs = devm_gpiod_get(&pdev->dev, "cs", GPIOD_OUT_HIGH);
    tk->sclk = devm_gpiod_get(&pdev->dev, "sclk", GPIOD_OUT_HIGH);
    tk->mosi = devm_gpiod_get(&pdev->dev, "mosi", GPIOD_OUT_LOW);
    if (IS_ERR(tk->reset) || IS_ERR(tk->enable) || IS_ERR(tk->backlight) || IS_ERR(tk->cs) || IS_ERR(tk->sclk) || IS_ERR(tk->mosi))
        return -EINVAL;
    drm_panel_init(&tk->panel, &pdev->dev, &tk_panel_funcs, DRM_MODE_CONNECTOR_DPI);
    drm_panel_add(&tk->panel);
    platform_set_drvdata(pdev, tk);
    return 0;
}

static int tk_remove(struct platform_device *pdev)
{
    drm_panel_remove(&((struct tk032f8004 *)platform_get_drvdata(pdev))->panel);
    return 0;
}

static const struct of_device_id tk_of_match[] = { { .compatible = "tk,tk032f8004" }, {} };
MODULE_DEVICE_TABLE(of, tk_of_match);
static struct platform_driver tk_driver = { .probe = tk_probe, .remove = tk_remove,
    .driver = { .name = "tk032f8004-panel", .of_match_table = tk_of_match } };
module_platform_driver(tk_driver);
MODULE_LICENSE("GPL");
