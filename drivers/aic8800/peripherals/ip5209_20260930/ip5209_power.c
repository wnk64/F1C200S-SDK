// SPDX-License-Identifier: GPL-2.0-only
#include <linux/bitops.h>
#include <linux/i2c.h>
#include <linux/math64.h>
#include <linux/module.h>
#include <linux/power_supply.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/workqueue.h>

#define IP5209_SYS_CTL0 0x01
#define IP5209_SYS_CTL1 0x02
#define IP5209_CHARGE_CURRENT 0x25
#define IP5209_CHARGE_STATUS 0x71
#define IP5209_SYSTEM_STATUS 0x72
#define IP5209_KEY_FLAGS 0x77
#define IP5209_VBAT_LOW 0xa2
#define IP5209_IBAT_LOW 0xa4
#define IP5209_OCV_LOW 0xa8
#define IP5209_CHARGER_ENABLE BIT(1)
#define IP5209_BOOST_ENABLE BIT(2)
#define IP5209_LIGHT_LOAD_SHUTDOWN BIT(1)

struct ip5209 {
	struct regmap *map;
	struct power_supply *battery;
	struct delayed_work poll_work;
	int last_charge;
	int last_system;
	int last_voltage;
};

/* The ADC is signed 14-bit; retry a sample spanning a high-byte rollover. */
static int ip5209_read_adc(struct ip5209 *chip, unsigned int reg, int *value)
{
	unsigned int high, low, check;
	int attempt, ret;

	for (attempt = 0; attempt < 3; attempt++) {
		ret = regmap_read(chip->map, reg + 1, &high);
		if (ret)
			return ret;
		ret = regmap_read(chip->map, reg, &low);
		if (ret)
			return ret;
		ret = regmap_read(chip->map, reg + 1, &check);
		if (ret)
			return ret;
		if ((high & 0x3f) == (check & 0x3f)) {
			*value = sign_extend32(((high & 0x3f) << 8) | low, 13);
			return 0;
		}
	}
	return -EAGAIN;
}

static int ip5209_voltage_uv(struct ip5209 *chip, unsigned int reg, int *uv)
{
	int raw, ret = ip5209_read_adc(chip, reg, &raw);

	if (!ret)
		*uv = 2600000 + DIV_ROUND_CLOSEST(raw * 26855, 100);
	return ret;
}

static int ip5209_get_property(struct power_supply *psy,
			       enum power_supply_property prop,
			       union power_supply_propval *val)
{
	struct ip5209 *chip = power_supply_get_drvdata(psy);
	unsigned int reg, system;
	int raw, ret;

	switch (prop) {
	case POWER_SUPPLY_PROP_STATUS:
		ret = regmap_read(chip->map, IP5209_CHARGE_STATUS, &reg);
		if (ret)
			return ret;
		switch (reg >> 5) {
		case 1: case 2: case 3:
			val->intval = POWER_SUPPLY_STATUS_CHARGING;
			break;
		case 5:
			val->intval = POWER_SUPPLY_STATUS_FULL;
			break;
		case 0:
			ret = ip5209_read_adc(chip, IP5209_IBAT_LOW, &raw);
			if (ret)
				return ret;
			val->intval = raw > 0 ? POWER_SUPPLY_STATUS_DISCHARGING :
				POWER_SUPPLY_STATUS_NOT_CHARGING;
			break;
		case 4: case 6:
			val->intval = POWER_SUPPLY_STATUS_NOT_CHARGING;
			break;
		default:
			val->intval = POWER_SUPPLY_STATUS_UNKNOWN;
		}
		return 0;
	case POWER_SUPPLY_PROP_HEALTH:
		ret = regmap_read(chip->map, IP5209_SYSTEM_STATUS, &system);
		if (ret)
			return ret;
		ret = regmap_read(chip->map, IP5209_CHARGE_STATUS, &reg);
		if (ret)
			return ret;
		if (system & BIT(5))
			val->intval = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
		else if ((reg & 0x07) || (reg >> 5) == 6)
			val->intval = POWER_SUPPLY_HEALTH_SAFETY_TIMER_EXPIRE;
		else
			val->intval = POWER_SUPPLY_HEALTH_GOOD;
		return 0;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		return ip5209_voltage_uv(chip, IP5209_VBAT_LOW, &val->intval);
	case POWER_SUPPLY_PROP_VOLTAGE_OCV:
		return ip5209_voltage_uv(chip, IP5209_OCV_LOW, &val->intval);
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		ret = ip5209_read_adc(chip, IP5209_IBAT_LOW, &raw);
		if (ret)
			return ret;
		/* OCV=VBAT+IBAT*R: silicon positive is discharge. Linux uses charge positive. */
		val->intval = -div_s64((s64)raw * 745985, 1000);
		return 0;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
		ret = regmap_read(chip->map, IP5209_CHARGE_CURRENT, &reg);
		if (!ret)
			val->intval = (reg & 0x1f) * 100000;
		return ret;
	case POWER_SUPPLY_PROP_MODEL_NAME:
		val->strval = "IP5209";
		return 0;
	case POWER_SUPPLY_PROP_MANUFACTURER:
		val->strval = "Injoinic";
		return 0;
	default:
		return -EINVAL;
	}
}

static int ip5209_set_property(struct power_supply *psy,
			       enum power_supply_property prop,
			       const union power_supply_propval *val)
{
	struct ip5209 *chip = power_supply_get_drvdata(psy);
	int ret;

	if (prop != POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX ||
	    val->intval < 0 || val->intval > 3100000 || val->intval % 100000)
		return -EINVAL;
	ret = regmap_update_bits(chip->map, IP5209_CHARGE_CURRENT, 0x1f,
				 val->intval / 100000);
	if (!ret)
		power_supply_changed(psy);
	return ret;
}

static int ip5209_property_writeable(struct power_supply *psy,
				     enum power_supply_property prop)
{
	return prop == POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX;
}

static enum power_supply_property ip5209_properties[] = {
	POWER_SUPPLY_PROP_STATUS, POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_VOLTAGE_NOW, POWER_SUPPLY_PROP_VOLTAGE_OCV,
	POWER_SUPPLY_PROP_CURRENT_NOW, POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX,
	POWER_SUPPLY_PROP_MODEL_NAME, POWER_SUPPLY_PROP_MANUFACTURER,
};

static const struct power_supply_desc ip5209_battery_desc = {
	.name = "ip5209-battery",
	.type = POWER_SUPPLY_TYPE_BATTERY,
	.properties = ip5209_properties,
	.num_properties = ARRAY_SIZE(ip5209_properties),
	.get_property = ip5209_get_property,
	.set_property = ip5209_set_property,
	.property_is_writeable = ip5209_property_writeable,
};

static ssize_t ip5209_bit_show(struct device *dev, char *buf,
			      unsigned int reg, unsigned int bit)
{
	struct ip5209 *chip = i2c_get_clientdata(to_i2c_client(dev));
	unsigned int value;
	int ret = regmap_read(chip->map, reg, &value);

	return ret ? ret : scnprintf(buf, PAGE_SIZE, "%u\n", !!(value & bit));
}

static ssize_t ip5209_bit_store(struct device *dev, const char *buf, size_t count,
			       unsigned int reg, unsigned int bit)
{
	struct ip5209 *chip = i2c_get_clientdata(to_i2c_client(dev));
	bool enabled;
	int ret = kstrtobool(buf, &enabled);

	if (ret)
		return ret;
	ret = regmap_update_bits(chip->map, reg, bit, enabled ? bit : 0);
	if (ret)
		return ret;
	power_supply_changed(chip->battery);
	return count;
}

static ssize_t charger_enabled_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	return ip5209_bit_show(dev, buf, IP5209_SYS_CTL0, IP5209_CHARGER_ENABLE);
}
static ssize_t charger_enabled_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	return ip5209_bit_store(dev, buf, count, IP5209_SYS_CTL0, IP5209_CHARGER_ENABLE);
}
static DEVICE_ATTR_RW(charger_enabled);

static ssize_t light_load_shutdown_enabled_show(struct device *dev,
						struct device_attribute *attr, char *buf)
{
	return ip5209_bit_show(dev, buf, IP5209_SYS_CTL1, IP5209_LIGHT_LOAD_SHUTDOWN);
}
static ssize_t light_load_shutdown_enabled_store(struct device *dev,
						 struct device_attribute *attr,
						 const char *buf, size_t count)
{
	return ip5209_bit_store(dev, buf, count, IP5209_SYS_CTL1, IP5209_LIGHT_LOAD_SHUTDOWN);
}
static DEVICE_ATTR_RW(light_load_shutdown_enabled);

static ssize_t boost_enabled_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	return ip5209_bit_show(dev, buf, IP5209_SYS_CTL0, IP5209_BOOST_ENABLE);
}
static DEVICE_ATTR_RO(boost_enabled);

static ssize_t key_flags_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	struct ip5209 *chip = i2c_get_clientdata(to_i2c_client(dev));
	unsigned int flags;
	int ret = regmap_read(chip->map, IP5209_KEY_FLAGS, &flags);

	return ret ? ret : scnprintf(buf, PAGE_SIZE, "0x%02x\n", flags & 0x0b);
}
static DEVICE_ATTR_RO(key_flags);

static struct attribute *ip5209_attrs[] = {
	&dev_attr_charger_enabled.attr,
	&dev_attr_light_load_shutdown_enabled.attr,
	&dev_attr_boost_enabled.attr,
	&dev_attr_key_flags.attr,
	NULL,
};
static const struct attribute_group ip5209_attr_group = { .attrs = ip5209_attrs };

static void ip5209_poll(struct work_struct *work)
{
	struct ip5209 *chip = container_of(to_delayed_work(work), struct ip5209, poll_work);
	unsigned int charge, system;
	int voltage;

	if (!regmap_read(chip->map, IP5209_CHARGE_STATUS, &charge) &&
	    !regmap_read(chip->map, IP5209_SYSTEM_STATUS, &system) &&
	    !ip5209_voltage_uv(chip, IP5209_VBAT_LOW, &voltage)) {
		if (charge != chip->last_charge || system != chip->last_system ||
		    abs(voltage - chip->last_voltage) >= 10000) {
			chip->last_charge = charge;
			chip->last_system = system;
			chip->last_voltage = voltage;
			power_supply_changed(chip->battery);
		}
	}
	schedule_delayed_work(&chip->poll_work, msecs_to_jiffies(5000));
}

static const struct regmap_config ip5209_regmap_config = {
	.reg_bits = 8, .val_bits = 8, .max_register = IP5209_OCV_LOW + 1,
	.cache_type = REGCACHE_NONE,
};

static int ip5209_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct power_supply_config config = {};
	struct ip5209 *chip;
	unsigned int control;
	int ret;

	chip = devm_kzalloc(&client->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;
	chip->map = devm_regmap_init_i2c(client, &ip5209_regmap_config);
	if (IS_ERR(chip->map))
		return PTR_ERR(chip->map);
	ret = regmap_read(chip->map, IP5209_SYS_CTL0, &control);
	if (ret)
		return ret;
	i2c_set_clientdata(client, chip);
	config.drv_data = chip;
	config.of_node = client->dev.of_node;
	chip->battery = devm_power_supply_register(&client->dev, &ip5209_battery_desc, &config);
	if (IS_ERR(chip->battery))
		return PTR_ERR(chip->battery);
	ret = devm_device_add_group(&client->dev, &ip5209_attr_group);
	if (ret)
		return ret;
	chip->last_charge = -1;
	chip->last_system = -1;
	INIT_DELAYED_WORK(&chip->poll_work, ip5209_poll);
	schedule_delayed_work(&chip->poll_work, msecs_to_jiffies(5000));
	dev_info(&client->dev, "IP5209 battery monitor registered; supply settings preserved\n");
	return 0;
}

static int ip5209_remove(struct i2c_client *client)
{
	struct ip5209 *chip = i2c_get_clientdata(client);

	cancel_delayed_work_sync(&chip->poll_work);
	return 0;
}

static const struct of_device_id ip5209_of_match[] = {
	{ .compatible = "injoinic,ip5209" }, {},
};
MODULE_DEVICE_TABLE(of, ip5209_of_match);
static const struct i2c_device_id ip5209_ids[] = { { "ip5209", 0 }, {} };
MODULE_DEVICE_TABLE(i2c, ip5209_ids);
static struct i2c_driver ip5209_driver = {
	.driver = { .name = "ip5209-power", .of_match_table = ip5209_of_match },
	.probe = ip5209_probe, .remove = ip5209_remove, .id_table = ip5209_ids,
};
module_i2c_driver(ip5209_driver);
MODULE_AUTHOR("F1C200S project");
MODULE_DESCRIPTION("IP5209 battery monitoring and documented charge controls");
MODULE_LICENSE("GPL");
