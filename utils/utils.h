/*
 * Copyright (C) 2010 Novell Inc.
 *
 * Author: Gary Ching-Pang Lin <glin@novell.com>
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <json-glib/json-glib.h>

#ifndef _UTILS_H_
#define _UTILS_H_

char     *encode_tokens       (const char          *token,
                               const char          *secret);
JsonNode *json_node_from_call (const RestProxyCall *call,
                               const char          *name);

#endif /* _UTILS_H_ */
