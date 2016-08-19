/*
 * pisound Linux kernel module.
 * Copyright (C) 2016  Vilniaus Blokas UAB, http://blokas.io/pisound
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/jack.h>

#include "pisound_spi.h"
#include "pisound_midi.h"
#include "pisound_debug.h"

static const struct of_device_id pisound_of_match[] = {
	{ .compatible = "blokaslabs,pisound", },
	{ .compatible = "blokaslabs,pisound-spi", },
	{},
};

static struct gpio_desc *osr0, *osr1, *osr2;
static struct gpio_desc *reset;
static struct gpio_desc *button;

static int pisnd_hw_params(struct snd_pcm_substream *substream, struct snd_pcm_hw_params *params)
{
	printd("rate   = %d\n", params_rate(params));
	printd("ch     = %d\n", params_channels(params));
	printd("bits   = %u\n", snd_pcm_format_physical_width(params_format(params)));
	printd("format = %d\n", params_format(params));

	gpiod_set_value(reset, false);

	switch (params_rate(params))
	{
	case 48000:
		gpiod_set_value(osr0, true);
		gpiod_set_value(osr1, false);
		gpiod_set_value(osr2, false);
		break;
	case 96000:
		gpiod_set_value(osr0, true);
		gpiod_set_value(osr1, true);
		gpiod_set_value(osr2, false);
		break;
	case 192000:
		gpiod_set_value(osr0, true);
		gpiod_set_value(osr1, true);
		gpiod_set_value(osr2, true);
		break;
	default:
		printe("Unsupported rate %u!\n", params_rate(params));
		return -EINVAL;
	}

	gpiod_set_value(reset, true);

	return 0;
}

static unsigned int rates[3] = {
	48000, 96000, 192000
};

static struct snd_pcm_hw_constraint_list constraints_rates = {
	.count = ARRAY_SIZE(rates),
	.list = rates,
	.mask = 0,
};

static unsigned int sample_bits[] = {
	24, 32
};

static struct snd_pcm_hw_constraint_list constraints_sample_bits = {
	.count = ARRAY_SIZE(sample_bits),
	.list = sample_bits,
	.mask = 0,
};

static int pisnd_startup(struct snd_pcm_substream *substream)
{
	int err = snd_pcm_hw_constraint_list(substream->runtime, 0, SNDRV_PCM_HW_PARAM_RATE, &constraints_rates);

	if (err < 0)
		return err;

	err = snd_pcm_hw_constraint_list(substream->runtime, 0, SNDRV_PCM_HW_PARAM_SAMPLE_BITS, &constraints_sample_bits);

	if (err < 0)
		return err;

	return 0;
}

static struct snd_soc_ops pisnd_ops = {
	.startup = pisnd_startup,
	.hw_params = pisnd_hw_params,
};

static struct snd_soc_dai_link pisnd_dai[] = {
	{
		.name           = "pisound",
		.stream_name    = "pisound",
		.cpu_dai_name   = "bcm2708-i2s.0",
		.codec_dai_name = "snd-soc-dummy-dai",
		.platform_name  = "bcm2708-i2s.0",
		.codec_name     = "snd-soc-dummy",
		.dai_fmt        = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBM_CFM,
		.ops            = &pisnd_ops,
	},
};

static int pisnd_card_probe(struct snd_soc_card *card)
{
	int err = pisnd_midi_init(card->snd_card);

	if (err < 0)
		printe("pisnd_midi_init failed: %d\n", err);

	return err;
}

static int pisnd_card_remove(struct snd_soc_card *card)
{
	pisnd_midi_uninit();
	return 0;
}

static struct snd_soc_card pisnd_card = {
	.name         = "pisound",
	.owner        = THIS_MODULE,
	.dai_link     = pisnd_dai,
	.num_links    = ARRAY_SIZE(pisnd_dai),
	.probe        = pisnd_card_probe,
	.remove       = pisnd_card_remove,
};

static int pisnd_init_gpio(struct device *dev)
{
	osr0 = gpiod_get_index(dev, "osr", 0, GPIOD_ASIS);
	osr1 = gpiod_get_index(dev, "osr", 1, GPIOD_ASIS);
	osr2 = gpiod_get_index(dev, "osr", 2, GPIOD_ASIS);

	reset = gpiod_get_index(dev, "reset", 0, GPIOD_ASIS);

	button = gpiod_get_index(dev, "button", 0, GPIOD_ASIS);

	gpiod_direction_output(osr0,  1);
	gpiod_direction_output(osr1,  1);
	gpiod_direction_output(osr2,  1);
	gpiod_direction_output(reset, 1);

	gpiod_set_value(reset, false);
	gpiod_set_value(osr0,   true);
	gpiod_set_value(osr1,  false);
	gpiod_set_value(osr2,  false);
	gpiod_set_value(reset,  true);

	gpiod_export(button, false);

	return 0;
}

static int pisnd_uninit_gpio(void)
{
	int i;

	gpiod_unexport(button);

	struct gpio_desc **gpios[] = {
		&osr0, &osr1, &osr2, &reset, &button,
	};

	for (i=0; i<ARRAY_SIZE(gpios); ++i)
	{
		if (*gpios[i] == NULL)
		{
			printd("weird, GPIO[%d] is NULL already\n", i);
			continue;
		}

		gpiod_put(*gpios[i]);
		*gpios[i] = NULL;
	}

	return 0;
}

static struct kobject *pisnd_kobj;

static ssize_t pisnd_serial_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", pisnd_spi_get_serial());
}

static ssize_t pisnd_id_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", pisnd_spi_get_id());
}

static ssize_t pisnd_version_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", pisnd_spi_get_version());
}

static struct kobj_attribute pisnd_serial_attribute = __ATTR(serial, 0644, pisnd_serial_show, NULL);
static struct kobj_attribute pisnd_id_attribute = __ATTR(id, 0644, pisnd_id_show, NULL);
static struct kobj_attribute pisnd_version_attribute = __ATTR(version, 0644, pisnd_version_show, NULL);

static struct attribute *attrs[] = {
	&pisnd_serial_attribute.attr,
	&pisnd_id_attribute.attr,
	&pisnd_version_attribute.attr,
	NULL
};

static struct attribute_group attr_group = { .attrs = attrs };

static int pisnd_probe(struct platform_device *pdev)
{
	int ret = 0;

	ret = pisnd_spi_init(&pdev->dev);
	if (ret < 0)
	{
		printe("pisnd_spi_init failed: %d\n", ret);
		return ret;
	}

	printi("Detected pisound card:\n");
	printi("\tSerial:  %s\n", pisnd_spi_get_serial());
	printi("\tVersion: %s\n", pisnd_spi_get_version());
	printi("\tId:      %s\n", pisnd_spi_get_id());

	pisnd_kobj = kobject_create_and_add("pisound", kernel_kobj);
	if (!pisnd_kobj)
	{
		pisnd_spi_uninit();
		return -ENOMEM;
	}

	ret = sysfs_create_group(pisnd_kobj, &attr_group);
	if (ret < 0)
	{
		pisnd_spi_uninit();
		kobject_put(pisnd_kobj);
		return -ENOMEM;
	}

	pisnd_init_gpio(&pdev->dev);
	pisnd_card.dev = &pdev->dev;

	if (pdev->dev.of_node)
	{
		struct device_node *i2s_node;
		i2s_node = of_parse_phandle(pdev->dev.of_node, "i2s-controller", 0);

		int i;

		for (i=0; i<pisnd_card.num_links; ++i)
		{
			struct snd_soc_dai_link *dai = &pisnd_dai[i];

			if (i2s_node)
			{
				dai->cpu_dai_name = NULL;
				dai->cpu_of_node = i2s_node;
				dai->platform_name = NULL;
				dai->platform_of_node = i2s_node;
				dai->stream_name = pisnd_spi_get_serial();
			}
		}
	}

	ret = snd_soc_register_card(&pisnd_card);

	if (ret < 0)
	{
		printe("snd_soc_register_card() failed: %d\n", ret);
		pisnd_uninit_gpio();
		kobject_put(pisnd_kobj);
		pisnd_spi_uninit();
	}

	return ret;
}

static int pisnd_remove(struct platform_device *pdev)
{
	if (pisnd_kobj)
	{
		kobject_put(pisnd_kobj);
		pisnd_kobj = NULL;
	}

	pisnd_spi_uninit();

	// Turn off
	gpiod_set_value(reset, false);
	pisnd_uninit_gpio();

	return snd_soc_unregister_card(&pisnd_card);
}

MODULE_DEVICE_TABLE(of, pisound_of_match);

static struct platform_driver pisnd_driver = {
	.driver = {
		.name           = "snd-rpi-pisound",
		.owner          = THIS_MODULE,
		.of_match_table = pisound_of_match,
	},
	.probe              = pisnd_probe,
	.remove             = pisnd_remove,
};

module_platform_driver(pisnd_driver);

MODULE_AUTHOR("Giedrius Trainavicius <giedrius@blokas.io>");
MODULE_DESCRIPTION("ASoC Driver for pisound, http://blokas.io/pisound");
MODULE_LICENSE("GPL v2");
