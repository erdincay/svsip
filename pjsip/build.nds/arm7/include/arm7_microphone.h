/*
 * Copyright (C) 2007-2009  Samuel Vinson <samuelv0304@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef ARM7_MICROPHONE_H_
#define ARM7_MICROPHONE_H_

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

void arm7_microStart(void);
void arm7_microStop(void);
void arm7_microProcess(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /*ARM7_MICROPHONE_H_*/
