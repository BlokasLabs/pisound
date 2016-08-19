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

#ifndef PISOUND_DEBUG_H
#define PISOUND_DEBUG_H

#define PISOUND_LOG_PREFIX "pisound: "

#ifdef DEBUG
#	define printd(...) pr_alert(PISOUND_LOG_PREFIX __VA_ARGS__)
#else
#	define printd(...) do {} while (0)
#endif

#define printe(...) pr_err(PISOUND_LOG_PREFIX __VA_ARGS__)
#define printi(...) pr_info(PISOUND_LOG_PREFIX __VA_ARGS__)

#endif // PISOUND_DEBUG_H
