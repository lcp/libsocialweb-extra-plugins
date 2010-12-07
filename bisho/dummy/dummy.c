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

#include <config.h>
#include <glib-object.h>
#include <bisho/service-info.h>
#include "dummy.h"

struct _BishoPaneDummyPrivate {
  gpointer dummy;
};

#define GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), BISHO_TYPE_PANE_DUMMY, BishoPaneDummyPrivate))
G_DEFINE_DYNAMIC_TYPE (BishoPaneDummy, bisho_pane_dummy, BISHO_TYPE_PANE);

static const char *
bisho_pane_dummy_get_auth_type (BishoPaneClass *klass)
{
  return "dummy";
}

static void
bisho_pane_dummy_class_finalize (BishoPaneDummyClass *klass)
{
}

static void
bisho_pane_dummy_class_init (BishoPaneDummyClass *klass)
{
  BishoPaneClass *pane_class = BISHO_PANE_CLASS (klass);

  pane_class->get_auth_type = bisho_pane_dummy_get_auth_type;

  g_type_class_add_private (klass, sizeof (BishoPaneDummyPrivate));
}

static void
bisho_pane_dummy_init (BishoPaneDummy *pane)
{
}

void
bisho_module_load (GTypeModule *module)
{
  bisho_pane_dummy_register_type (module);
}
