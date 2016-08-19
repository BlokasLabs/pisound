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

#include <linux/gpio.h>
#include <linux/spi/spi.h>
#include <linux/interrupt.h>
#include <linux/kfifo.h>
#include <linux/delay.h>

#include "pisound_spi.h"
#include "pisound_debug.h"

static void *g_recvData;
static pisnd_spi_recv_cb g_recvCallback;

#define FIFO_SIZE 512

static char g_serial_num[11];
static char g_id[25];
static char g_version[5];

DEFINE_KFIFO(spi_fifo_in,  uint8_t, FIFO_SIZE);
DEFINE_KFIFO(spi_fifo_out, uint8_t, FIFO_SIZE);

static struct gpio_desc *data_available;
static struct gpio_desc *spi_reset;

static struct spi_device *pisnd_spi_device;

static struct workqueue_struct *pisnd_workqueue;
static struct work_struct pisnd_work_process;

static void pisnd_work_handler(struct work_struct *work);

static uint16_t spi_transfer16(uint16_t val);

static int pisnd_init_workqueues(void)
{
	pisnd_workqueue = create_singlethread_workqueue("pisnd_workqueue");
	INIT_WORK(&pisnd_work_process, pisnd_work_handler);

	return 0;
}

static void pisnd_uninit_workqueues(void)
{
	flush_workqueue(pisnd_workqueue);
	destroy_workqueue(pisnd_workqueue);

	pisnd_workqueue = NULL;
}

static bool pisnd_spi_has_more(void)
{
	return gpiod_get_value(data_available);
}

enum task_e
{
	TASK_PROCESS = 0,
};

static void pisnd_schedule_process(enum task_e task)
{
	if (pisnd_spi_device != NULL && pisnd_workqueue != NULL && !work_pending(&pisnd_work_process))
	{
		printd("schedule: has more = %d\n", pisnd_spi_has_more());
		if (task == TASK_PROCESS)
		{
			queue_work(pisnd_workqueue, &pisnd_work_process);
		}
	}
}

irqreturn_t data_available_interrupt_handler(int irq, void *dev_id)
{
	if (irq == gpiod_to_irq(data_available) && pisnd_spi_has_more())
	{
		printd("schedule from irq\n");
		pisnd_schedule_process(TASK_PROCESS);
	}

	return IRQ_HANDLED;
}

static DEFINE_SPINLOCK(spilock);
static unsigned long spilockflags;

static uint16_t spi_transfer16(uint16_t val)
{
	struct spi_transfer transfer;
	struct spi_message msg;
	uint8_t txbuf[2];
	uint8_t rxbuf[2];

	if (!pisnd_spi_device)
	{
		printe("pisnd_spi_device null, returning\n");
		return 0;
	}

	spi_message_init(&msg);

	memset(&transfer, 0, sizeof(transfer));
	memset(&rxbuf, 0, sizeof(rxbuf));

	txbuf[0] = val >> 8;
	txbuf[1] = val & 0xff;

	transfer.tx_buf = &txbuf;
	transfer.rx_buf = &rxbuf;
	transfer.len = sizeof(txbuf);
	transfer.speed_hz = 125000;
	transfer.delay_usecs = 100;
	spi_message_add_tail(&transfer, &msg);

	spin_lock_irqsave(&spilock, spilockflags);
	int err = spi_sync(pisnd_spi_device, &msg);
	spin_unlock_irqrestore(&spilock, spilockflags);

	if (err < 0)
	{
		printe("spi_sync error %d\n", err);
		return 0;
	}

	printd("received: %02x%02x\n", rxbuf[0], rxbuf[1]);
	printd("hasMore %d\n", pisnd_spi_has_more());

	uint16_t result = (rxbuf[0] << 8) | rxbuf[1];

	return result;
}

static int spi_read_bytes(char *dst, size_t length, uint8_t *bytesRead)
{
	memset(dst, 0, length);
	*bytesRead = 0;

	uint16_t rx = spi_transfer16(0);
	if (!(rx >> 8))
		return -EINVAL;

	uint8_t size = rx & 0xff;

	if (size > length)
		return -EINVAL;

	uint8_t i;
	for (i=0; i<size; ++i)
	{
		rx = spi_transfer16(0);
		if (!(rx >> 8))
			return -EINVAL;

		dst[i] = rx & 0xff;
	}

	*bytesRead = i;

	return 0;
}

static int spi_device_match(struct device *dev, void *data)
{
	struct spi_device *spi = container_of(dev, struct spi_device, dev);

	printd("      %s %s %dkHz %d bits mode=0x%02X\n",
		spi->modalias, dev_name(dev), spi->max_speed_hz/1000,
		spi->bits_per_word, spi->mode);

	if (strcmp("pisound-spi", spi->modalias) == 0)
	{
		printi("\tFound!\n");
		return 1;
	}

	printe("\tNot found!\n");
	return 0;
}

static struct spi_device * pisnd_spi_find_device(void)
{
	printi("Searching for spi device...\n");
	struct device *dev = bus_find_device(&spi_bus_type, NULL, NULL, spi_device_match);
	if (dev != NULL)
	{
		return container_of(dev, struct spi_device, dev);
	}
	else return NULL;
}

static void pisnd_work_handler(struct work_struct *work)
{
	if (work == &pisnd_work_process)
	{
		if (pisnd_spi_device == NULL)
			return;

		uint16_t rx;

		do
		{
			uint8_t val = 0;
			uint16_t tx = 0;

			if (kfifo_get(&spi_fifo_out, &val))
				tx = 0x0f00 | val;

			rx = spi_transfer16(tx);

			if (rx & 0xff00)
			{
				kfifo_put(&spi_fifo_in, rx & 0xff);
				if (kfifo_len(&spi_fifo_in) > 16 && g_recvCallback)
					g_recvCallback(g_recvData);
			}
		} while (rx != 0 || !kfifo_is_empty(&spi_fifo_out) || pisnd_spi_has_more());

		if (!kfifo_is_empty(&spi_fifo_in) && g_recvCallback)
			g_recvCallback(g_recvData);
	}
}

static int pisnd_spi_gpio_init(struct device *dev)
{
	spi_reset = gpiod_get_index(dev, "reset", 1, GPIOD_ASIS);
	data_available = gpiod_get_index(dev, "data_available", 0, GPIOD_ASIS);

	gpiod_direction_output(spi_reset, 1);
	gpiod_direction_input(data_available);

	// Reset the slave.
	gpiod_set_value(spi_reset, false);
	mdelay(1);
	gpiod_set_value(spi_reset, true);

	// Give time for spi slave to start.
	mdelay(64);

	return 0;
}

static void pisnd_spi_gpio_uninit(void)
{
	gpiod_set_value(spi_reset, false);
	gpiod_put(spi_reset);
	spi_reset = NULL;

	gpiod_put(data_available);
	data_available = NULL;
}

static int pisnd_spi_gpio_irq_init(struct device *dev)
{
	return request_irq(gpiod_to_irq(data_available), data_available_interrupt_handler, IRQF_TIMER | IRQF_TRIGGER_RISING, "data_available_int", NULL);
}

static void pisnd_spi_gpio_irq_uninit(void)
{
	free_irq(gpiod_to_irq(data_available), NULL);
}

static int spi_read_info(void)
{
	memset(g_serial_num, 0, sizeof(g_serial_num));
	memset(g_version, 0, sizeof(g_version));
	memset(g_id, 0, sizeof(g_id));

	uint16_t tmp = spi_transfer16(0);

	if (!(tmp >> 8))
		return -EINVAL;

	uint8_t count = tmp & 0xff;

	char buffer[257];
	uint8_t n;
	uint8_t i;
	for (i=0; i<count; ++i)
	{
		memset(buffer, 0, sizeof(buffer));
		int ret = spi_read_bytes(buffer, sizeof(buffer)-1, &n);

		if (ret < 0)
			return ret;

		switch (i)
		{
		case 0:
			if (n != 2)
				return -EINVAL;

			snprintf(g_version, sizeof(g_version), "%x.%02x", buffer[0], buffer[1]);
			break;
		case 1:
			if (n >= sizeof(g_serial_num))
				return -EINVAL;

			memcpy(g_serial_num, buffer, sizeof(g_serial_num));
			break;
		case 2:
			{
				if (n >= sizeof(g_id))
					return -EINVAL;

				char *p = g_id;
				uint8_t j;
				for (j=0; j<n; ++j)
				{
					p += sprintf(p, "%02x", buffer[j]);
				}
			}
			break;
		default:
			break;
		}
	}

	return 0;
}

int pisnd_spi_init(struct device *dev)
{
	int ret;

	memset(g_serial_num, 0, sizeof(g_serial_num));
	memset(g_id, 0, sizeof(g_id));
	memset(g_version, 0, sizeof(g_version));

	struct spi_device *spi = pisnd_spi_find_device();

	if (spi != NULL)
	{
		printd("initializing spi!\n");
		pisnd_spi_device = spi;
		ret = spi_setup(pisnd_spi_device);
	}
	else
	{
		printe("SPI device not found, deferring!\n");
		return -EPROBE_DEFER;
	}

	ret = pisnd_spi_gpio_init(dev);

	if (ret < 0)
	{
		printe("SPI GPIO init failed: %d\n", ret);
		spi_dev_put(pisnd_spi_device);
		pisnd_spi_device = NULL;
		pisnd_spi_gpio_uninit();
		return ret;
	}

	ret = spi_read_info();

	if (ret < 0)
	{
		printe("Reading card info failed: %d\n", ret);
		spi_dev_put(pisnd_spi_device);
		pisnd_spi_device = NULL;
		pisnd_spi_gpio_uninit();
		return ret;
	}

	// Flash the LEDs.
	spi_transfer16(0xf000);

	ret = pisnd_spi_gpio_irq_init(dev);
	if (ret < 0)
	{
		printe("SPI irq request failed: %d\n", ret);
		spi_dev_put(pisnd_spi_device);
		pisnd_spi_device = NULL;
		pisnd_spi_gpio_irq_uninit();
		pisnd_spi_gpio_uninit();
	}

	ret = pisnd_init_workqueues();
	if (ret != 0)
	{
		printe("Workqueue initialization failed: %d\n", ret);
		spi_dev_put(pisnd_spi_device);
		pisnd_spi_device = NULL;
		pisnd_spi_gpio_irq_uninit();
		pisnd_spi_gpio_uninit();
		pisnd_uninit_workqueues();
		return ret;
	}

	if (pisnd_spi_has_more())
	{
		printd("data is available, scheduling from init\n");
		pisnd_schedule_process(TASK_PROCESS);
	}

	return 0;
}

void pisnd_spi_uninit(void)
{
	pisnd_uninit_workqueues();

	spi_dev_put(pisnd_spi_device);
	pisnd_spi_device = NULL;

	pisnd_spi_gpio_irq_uninit();
	pisnd_spi_gpio_uninit();
}

void pisnd_spi_send(uint8_t val)
{
	kfifo_put(&spi_fifo_out, val);
	printd("schedule from spi_send\n");
	pisnd_schedule_process(TASK_PROCESS);
}

uint8_t pisnd_spi_recv(uint8_t *buffer, uint8_t length)
{
	return kfifo_out(&spi_fifo_in, buffer, length);
}

void pisnd_spi_set_callback(pisnd_spi_recv_cb cb, void *data)
{
	g_recvData = data;
	g_recvCallback = cb;
}

const char *pisnd_spi_get_serial(void)
{
	if (strlen(g_serial_num))
		return g_serial_num;

	return "";
}

const char *pisnd_spi_get_id(void)
{
	if (strlen(g_id))
		return g_id;

	return "";
}

const char *pisnd_spi_get_version(void)
{
	if (strlen(g_version))
		return g_version;

	return "";
}
