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

#ifndef __AUTH_BROWSER_H__
#define __AUTH_BROWSER_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define AUTH_TYPE_BROWSER auth_browser_get_type()

#define AUTH_BROWSER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), AUTH_TYPE_BROWSER, AuthBrowser))

#define AUTH_BROWSER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), AUTH_TYPE_BROWSER, AuthBrowserClass))

#define AUTH_IS_BROWSER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), AUTH_TYPE_BROWSER))

#define AUTH_IS_BROWSER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), AUTH_TYPE_BROWSER))

#define AUTH_BROWSER_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), AUTH_TYPE_BROWSER, AuthBrowserClass))

typedef struct {
  GObject parent;
} AuthBrowser;

typedef struct {
  GObjectClass parent_class;
} AuthBrowserClass;

GType auth_browser_get_type (void);

AuthBrowser* auth_browser_new (void);

void auth_browser_open_url (AuthBrowser *browser,
                            const char  *url,
                            const char  *stop_url,
                            void       (*callback) (const char *url));
void auth_browser_hide     (AuthBrowser *browser);

G_BEGIN_DECLS

#endif /* __AUTH_BROWSER_H__ */
