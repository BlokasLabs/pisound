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

#ifndef PISOUND_SPI_H
#define PISOUND_SPI_H

int pisnd_spi_init(struct device *dev);
void pisnd_spi_uninit(void);

void pisnd_spi_send(uint8_t val);
uint8_t pisnd_spi_recv(uint8_t *buffer, uint8_t length);

typedef void (*pisnd_spi_recv_cb)(void *data);
void pisnd_spi_set_callback(pisnd_spi_recv_cb cb, void *data);

const char *pisnd_spi_get_serial(void);
const char *pisnd_spi_get_id(void);
const char *pisnd_spi_get_version(void);

#endif // PISOUND_SPI_H
