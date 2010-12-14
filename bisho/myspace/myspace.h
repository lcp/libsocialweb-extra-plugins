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

#ifndef __BISHO_PANE_MYSPACE_H__
#define __BISHO_PANE_MYSPACE_H__

#include <bisho/bisho-pane.h>

G_BEGIN_DECLS

#define BISHO_TYPE_PANE_MYSPACE (bisho_pane_myspace_get_type())
#define BISHO_PANE_MYSPACE(obj)                                         \
   (G_TYPE_CHECK_INSTANCE_CAST ((obj),                                  \
                                BISHO_TYPE_PANE_MYSPACE,                \
                                BishoPaneMySpace))
#define BISHO_PANE_MYSPACE_CLASS(klass)                                 \
   (G_TYPE_CHECK_CLASS_CAST ((klass),                                   \
                             BISHO_TYPE_PANE_MYSPACE,                   \
                             BishoPaneMySpaceClass))
#define BISHO_IS_PANE_MYSPACE(obj)                                      \
   (G_TYPE_CHECK_INSTANCE_TYPE ((obj),                                  \
                                BISHO_TYPE_PANE_MYSPACE))
#define BISHO_IS_PANE_MYSPACE_CLASS(klass)                              \
   (G_TYPE_CHECK_CLASS_TYPE ((klass),                                   \
                             BISHO_TYPE_PANE_MYSPACE))
#define BISHO_PANE_MYSPACE_GET_CLASS(obj)                               \
   (G_TYPE_INSTANCE_GET_CLASS ((obj),                                   \
                               BISHO_TYPE_PANE_MYSPACE,                 \
                               BishoPaneMySpaceClass))

typedef struct _BishoPaneMySpacePrivate BishoPaneMySpacePrivate;
typedef struct _BishoPaneMySpace        BishoPaneMySpace;
typedef struct _BishoPaneMySpaceClass   BishoPaneMySpaceClass;

struct _BishoPaneMySpace {
  BishoPane parent;
  BishoPaneMySpacePrivate *priv;
};

struct _BishoPaneMySpaceClass {
  BishoPaneClass parent_class;
};

GType bisho_pane_myspace_get_type (void) G_GNUC_CONST;

G_END_DECLS

#endif /* __BISHO_PANE_MYSPACE_H__ */
