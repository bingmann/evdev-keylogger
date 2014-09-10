/*******************************************************************************
 * keymap.h
 *
 * Translate keyboard scan codes into keys, modifiers, and compose them.
 *
 *******************************************************************************
 * Copyright (C) 2012 Jason A. Donenfeld <Jason@zx2c4.com>
 * Copyright (C) 2014 Timo Bingmann <tb@panthema.net>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place, Suite 330, Boston, MA 02111-1307 USA
 ******************************************************************************/

#include <linux/input.h>

struct input_event_state {
    int altgr:1;
    int alt:1;
    int shift:1;
    int ctrl:1;
    int meta:1;
};
size_t translate_event(struct input_event *event, struct input_event_state *state, char *buffer, size_t buffer_len);
int load_system_keymap();
