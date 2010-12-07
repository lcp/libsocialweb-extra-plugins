/*
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

#ifndef __BISHO_PANE_DUMMY_H__
#define __BISHO_PANE_DUMMY_H__

#include <bisho/bisho-pane.h>

G_BEGIN_DECLS

#define BISHO_TYPE_PANE_DUMMY (bisho_pane_dummy_get_type())
#define BISHO_PANE_DUMMY(obj)                                           \
   (G_TYPE_CHECK_INSTANCE_CAST ((obj),                                  \
                                BISHO_TYPE_PANE_DUMMY,                  \
                                BishoPaneDummy))
#define BISHO_PANE_DUMMY_CLASS(klass)                                   \
   (G_TYPE_CHECK_CLASS_CAST ((klass),                                   \
                             BISHO_TYPE_PANE_DUMMY,                     \
                             BishoPaneDummyClass))
#define BISHO_IS_PANE_DUMMY(obj)                                        \
   (G_TYPE_CHECK_INSTANCE_TYPE ((obj),                                  \
                                BISHO_TYPE_PANE_DUMMY))
#define BISHO_IS_PANE_DUMMY_CLASS(klass)                                \
   (G_TYPE_CHECK_CLASS_TYPE ((klass),                                   \
                             BISHO_TYPE_PANE_DUMMY))
#define BISHO_PANE_DUMMY_GET_CLASS(obj)                                 \
   (G_TYPE_INSTANCE_GET_CLASS ((obj),                                   \
                               BISHO_TYPE_PANE_DUMMY,                   \
                               BishoPaneDummyClass))

typedef struct _BishoPaneDummyPrivate BishoPaneDummyPrivate;
typedef struct _BishoPaneDummy      BishoPaneDummy;
typedef struct _BishoPaneDummyClass BishoPaneDummyClass;

struct _BishoPaneDummy {
  BishoPane parent;
  BishoPaneDummyPrivate *priv;
};

struct _BishoPaneDummyClass {
  BishoPaneClass parent_class;
};

GType bisho_pane_dummy_get_type (void) G_GNUC_CONST;

G_END_DECLS

#endif /* __BISHO_PANE_DUMMY_H__ */
