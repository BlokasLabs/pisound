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

#include <linux/module.h>
#include <linux/delay.h>

#include <sound/core.h>
#include <sound/rawmidi.h>
#include <sound/asequencer.h>

#include "pisound_midi.h"
#include "pisound_spi.h"
#include "pisound_debug.h"

static int pisnd_output_open(struct snd_rawmidi_substream *substream)
{
	return 0;
}

static int pisnd_output_close(struct snd_rawmidi_substream *substream)
{
	return 0;
}

static void pisnd_output_trigger(struct snd_rawmidi_substream *substream, int up)
{
	if (!up)
		return;

	uint8_t data;
	while (snd_rawmidi_transmit_peek(substream, &data, 1))
	{
		pisnd_spi_send(data);
		snd_rawmidi_transmit_ack(substream, 1);
	}
}

static void pisnd_output_drain(struct snd_rawmidi_substream *substream)
{
	uint8_t data;
	while (snd_rawmidi_transmit_peek(substream, &data, 1))
	{
		pisnd_spi_send(data);
		
		snd_rawmidi_transmit_ack(substream, 1);
	}
}

static int pisnd_input_open(struct snd_rawmidi_substream *substream)
{
	return 0;
}

static int pisnd_input_close(struct snd_rawmidi_substream *substream)
{
	return 0;
}

static void pisnd_midi_recv_callback(void *substream)
{
	uint8_t data[128];
	uint8_t n = 0;
	while ((n = pisnd_spi_recv(data, sizeof(data))))
	{
		int res = snd_rawmidi_receive(substream, data, n);
		(void)res;
		printd("midi recv 0x%02x, res = %d\n", data, res);
	}
}

static void pisnd_input_trigger(struct snd_rawmidi_substream *substream, int up)
{
	if (up)
	{
		pisnd_spi_set_callback(pisnd_midi_recv_callback, substream);
		pisnd_midi_recv_callback(substream);
	}
	else
	{
		pisnd_spi_set_callback(NULL, NULL);
	}
}

static struct snd_rawmidi *g_rmidi;

static struct snd_rawmidi_ops pisnd_output_ops = {
	.open = pisnd_output_open,
	.close = pisnd_output_close,
	.trigger = pisnd_output_trigger,
	.drain = pisnd_output_drain,
};

static struct snd_rawmidi_ops pisnd_input_ops = {
	.open = pisnd_input_open,
	.close = pisnd_input_close,
	.trigger = pisnd_input_trigger,
};

static void pisnd_get_port_info(struct snd_rawmidi *rmidi, int number, struct snd_seq_port_info *seq_port_info)
{
	seq_port_info->type = SNDRV_SEQ_PORT_TYPE_MIDI_GENERIC | SNDRV_SEQ_PORT_TYPE_HARDWARE | SNDRV_SEQ_PORT_TYPE_PORT;
	seq_port_info->midi_voices = 0;
}

static struct snd_rawmidi_global_ops pisnd_global_ops = {
	.get_port_info = pisnd_get_port_info,
};

int pisnd_midi_init(struct snd_card *card)
{
	int err = snd_rawmidi_new(card, "pisound MIDI", 0, 1, 1, &g_rmidi);

	if (err < 0)
	{
		printe("snd_rawmidi_new failed: %d\n", err);
		return err;
	}

	strcpy(g_rmidi->name, "pisound MIDI ");
	strcat(g_rmidi->name, pisnd_spi_get_serial());

	g_rmidi->info_flags =
		SNDRV_RAWMIDI_INFO_OUTPUT |
		SNDRV_RAWMIDI_INFO_INPUT |
		SNDRV_RAWMIDI_INFO_DUPLEX;

	g_rmidi->ops = &pisnd_global_ops;

	g_rmidi->private_data = (void*)0;

	snd_rawmidi_set_ops(g_rmidi, SNDRV_RAWMIDI_STREAM_OUTPUT, &pisnd_output_ops);
	snd_rawmidi_set_ops(g_rmidi, SNDRV_RAWMIDI_STREAM_INPUT, &pisnd_input_ops);

	return 0;
}

void pisnd_midi_uninit(void)
{
}
