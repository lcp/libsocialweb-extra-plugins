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

#include <glib.h>
#include <string.h>

char *
encode_tokens (const char *token, const char *secret)
{
  char *encoded_token, *encoded_secret;
  char *string;

  g_assert (token);
  g_assert (secret);

  encoded_token = g_base64_encode ((guchar*)token, strlen (token));
  encoded_secret = g_base64_encode ((guchar*)secret, strlen (secret));

  string = g_strconcat (encoded_token, " ", encoded_secret, NULL);

  g_free (encoded_token);
  g_free (encoded_secret);

  return string;
}
