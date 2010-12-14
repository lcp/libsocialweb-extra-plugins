/*
 * (Branched from bisho oauth pane)
 * Copyright (C) 2010 Novell, Inc.
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

#ifndef __BISHO_PANE_OAUTH_WEBKIT_H__
#define __BISHO_PANE_OAUTH_WEBKIT_H__

#include <bisho/bisho-pane.h>

G_BEGIN_DECLS

#define BISHO_TYPE_PANE_OAUTH_WEBKIT (bisho_pane_oauth_webkit_get_type())
#define BISHO_PANE_OAUTH_WEBKIT(obj)                                         \
   (G_TYPE_CHECK_INSTANCE_CAST ((obj),                                       \
                                BISHO_TYPE_PANE_OAUTH_WEBKIT,                \
                                BishoPaneOauthWebkit))
#define BISHO_PANE_OAUTH_WEBKIT_CLASS(klass)                                 \
   (G_TYPE_CHECK_CLASS_CAST ((klass),                                        \
                             BISHO_TYPE_PANE_OAUTH_WEBKIT,                   \
                             BishoPaneOauthWebkitClass))
#define BISHO_IS_PANE_OAUTH_WEBKIT(obj)                                      \
   (G_TYPE_CHECK_INSTANCE_TYPE ((obj),                                       \
                                BISHO_TYPE_PANE_OAUTH_WEBKIT))
#define BISHO_IS_PANE_OAUTH_WEBKIT_CLASS(klass)                              \
   (G_TYPE_CHECK_CLASS_TYPE ((klass),                                        \
                             BISHO_TYPE_PANE_OAUTH_WEBKIT))
#define BISHO_PANE_OAUTH_WEBKIT_GET_CLASS(obj)                               \
   (G_TYPE_INSTANCE_GET_CLASS ((obj),                                        \
                               BISHO_TYPE_PANE_OAUTH_WEBKIT,                 \
                               BishoPaneOauthWebkitClass))

typedef struct _BishoPaneOauthWebkitPrivate BishoPaneOauthWebkitPrivate;
typedef struct _BishoPaneOauthWebkit        BishoPaneOauthWebkit;
typedef struct _BishoPaneOauthWebkitClass   BishoPaneOauthWebkitClass;

struct _BishoPaneOauthWebkit {
  BishoPane parent;
  BishoPaneOauthWebkitPrivate *priv;
};

struct _BishoPaneOauthWebkitClass {
  BishoPaneClass parent_class;
};

GType bisho_pane_oauth_webkit_get_type (void) G_GNUC_CONST;

G_END_DECLS

#endif /* __BISHO_PANE_OAUTH_WEBKIT_H__ */
