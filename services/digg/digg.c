/*
 * libsocialweb - social data store
 * Copyright (C) 2009 Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU Lesser General Public License,
 * version 2.1, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <config.h>

#include <stdlib.h>
#include <string.h>
#include <glib/gi18n.h>
#include <gnome-keyring.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <json-glib/json-glib.h>

#include <libsocialweb/sw-service.h>
#include <libsocialweb/sw-item.h>
#include <libsocialweb/sw-utils.h>
#include <libsocialweb/sw-web.h>
#include <libsocialweb/sw-online.h>
#include <libsocialweb/sw-client-monitor.h>
#include <libsocialweb-keyfob/sw-keyfob.h>
#include <libsocialweb-keystore/sw-keystore.h>

#include <rest/oauth-proxy.h>
#include <rest/oauth-proxy-call.h>
#include <rest/rest-xml-parser.h>

#include <interfaces/sw-query-ginterface.h>

#include "digg.h"
#include "digg-item-view.h"

static void initable_iface_init (gpointer g_iface, gpointer iface_data);
static void query_iface_init (gpointer g_iface, gpointer iface_data);

G_DEFINE_TYPE_WITH_CODE (SwServiceDigg, sw_service_digg, SW_TYPE_SERVICE,
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE, initable_iface_init)
                         G_IMPLEMENT_INTERFACE (SW_TYPE_QUERY_IFACE, query_iface_init));

#define GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), SW_TYPE_SERVICE_DIGG, SwServiceDiggPrivate))

struct _SwServiceDiggPrivate {
  RestProxy *proxy;
  gboolean inited; /* For GInitable */
  gboolean configured; /* Set if we have user tokens */
  gboolean authorised; /* Set if the tokens are valid */
};

static const char **
get_static_caps (SwService *service)
{
  static const char * caps[] = {
    CAN_VERIFY_CREDENTIALS,
    HAS_BANISHABLE_IFACE,
    HAS_QUERY_IFACE,

    NULL
  };

  return caps;
}

static const char **
get_dynamic_caps (SwService *service)
{
  SwServiceDiggPrivate *priv = GET_PRIVATE (service);

  static const char *unconfigured_caps[] = {
    NULL,
  };

  static const char *authorised_caps[] = {
    IS_CONFIGURED,
    CREDENTIALS_VALID,
    NULL
  };

  static const char *unauthorised_caps[] = {
    IS_CONFIGURED,
    CREDENTIALS_INVALID,
    NULL
  };

  if (priv->configured) {
    if (priv->authorised) {
      return authorised_caps;
    } else {
      return unauthorised_caps;
    }
  } else {
    return unconfigured_caps;
  }
}

static void
got_tokens_cb (RestProxy *proxy,
               gboolean   got_tokens,
               gpointer   user_data)
{
  SwService *service = SW_SERVICE (user_data);
  SwServiceDiggPrivate *priv = GET_PRIVATE (service);

  priv->configured = got_tokens;
  sw_service_emit_capabilities_changed (service, get_dynamic_caps (service));

  if (got_tokens && sw_is_online ()) {
    priv->authorised = TRUE;
    sw_service_emit_capabilities_changed (service, get_dynamic_caps (service));
  }
}

static void
credentials_updated (SwService *service)
{
  SwServiceDiggPrivate *priv = GET_PRIVATE (service);

  priv->configured = FALSE;
  priv->authorised = FALSE;

  sw_keyfob_oauth (OAUTH_PROXY (priv->proxy),
                   got_tokens_cb,
                   service);

  sw_service_emit_user_changed (service);
  sw_service_emit_capabilities_changed (service, get_dynamic_caps (service));
}

static void
online_notify (gboolean online, gpointer user_data)
{
  SwService *service = SW_SERVICE (user_data);
  SwServiceDiggPrivate *priv = GET_PRIVATE (service);

  if (online) {
    sw_keyfob_oauth (OAUTH_PROXY (priv->proxy),
                     got_tokens_cb,
                     service);
  } else {
    priv->authorised = FALSE;

    sw_service_emit_capabilities_changed (service, get_dynamic_caps (service));
  }
}

static const char *
get_name (SwService *service)
{
  return "digg";
}

static void
sw_service_digg_dispose (GObject *object)
{
  SwServiceDiggPrivate *priv = ((SwServiceDigg*)object)->priv;

  if (priv->proxy) {
    g_object_unref (priv->proxy);
    priv->proxy = NULL;
  }

  G_OBJECT_CLASS (sw_service_digg_parent_class)->dispose (object);
}

static gboolean
sw_service_digg_initable (GInitable    *initable,
                          GCancellable *cancellable,
                          GError      **error)
{
  SwServiceDigg *digg = SW_SERVICE_DIGG (initable);
  SwServiceDiggPrivate *priv = GET_PRIVATE (digg);
  const char *key = NULL, *secret = NULL;

  if (priv->inited)
    return TRUE;

  sw_keystore_get_key_secret ("digg", &key, &secret);
  if (key == NULL || secret == NULL) {
    g_set_error_literal (error,
                         SW_SERVICE_ERROR,
                         SW_SERVICE_ERROR_NO_KEYS,
                         "No API key configured");
    return FALSE;
  }

  priv->proxy = oauth_proxy_new (key, secret, "http://services.digg.com/", FALSE);

  sw_online_add_notify (online_notify, digg);

  priv->inited = TRUE;

  credentials_updated (SW_SERVICE (digg));

  return TRUE;
}

static void
initable_iface_init (gpointer g_iface,
                     gpointer iface_data)
{
  GInitableIface *klass = (GInitableIface *)g_iface;

  klass->init = sw_service_digg_initable;
}

const static gchar *valid_queries[] = { "feed" };

static gboolean
_check_query_validity (const gchar *query)
{
  gint i = 0;

  for (i = 0; i < G_N_ELEMENTS (valid_queries); i++)
  {
    if (g_str_equal (query, valid_queries[i]))
      return TRUE;
  }

  return FALSE;
}

static void
_digg_query_open_view (SwQueryIface          *self,
                       const gchar           *query,
                       GHashTable            *params,
                       DBusGMethodInvocation *context)
{
  SwServiceDiggPrivate *priv = GET_PRIVATE (self);
  SwItemView *item_view;
  const gchar *object_path;

  if (!_check_query_validity (query))
  {
    dbus_g_method_return_error (context,
                                g_error_new (SW_SERVICE_ERROR,
                                             SW_SERVICE_ERROR_INVALID_QUERY,
                                             "Query '%s' is invalid",
                                             query));
    return;
  }

  item_view = g_object_new (SW_TYPE_DIGG_ITEM_VIEW,
                            "proxy", priv->proxy,
                            "service", self,
                            "query", query,
                            "params", params,
                            NULL);

  /* Ensure the object gets disposed when the client goes away */
  sw_client_monitor_add (dbus_g_method_get_sender (context),
                         (GObject *)item_view);

  object_path = sw_item_view_get_object_path (item_view);
  sw_query_iface_return_from_open_view (context,
                                        object_path);
}

static void
query_iface_init (gpointer g_iface,
                  gpointer iface_data)
{
  SwQueryIfaceClass *klass = (SwQueryIfaceClass*)g_iface;

  sw_query_iface_implement_open_view (klass,
                                      _digg_query_open_view);
}

static void
sw_service_digg_class_init (SwServiceDiggClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  SwServiceClass *service_class = SW_SERVICE_CLASS (klass);

  g_type_class_add_private (klass, sizeof (SwServiceDiggPrivate));

  object_class->dispose = sw_service_digg_dispose;

  service_class->get_name = get_name;
  service_class->credentials_updated = credentials_updated;
  service_class->get_dynamic_caps = get_dynamic_caps;
  service_class->get_static_caps = get_static_caps;
}

static void
sw_service_digg_init (SwServiceDigg *self)
{
  self->priv = GET_PRIVATE (self);
  self->priv->inited = FALSE;
  self->priv->configured = FALSE;
  self->priv->authorised = FALSE;
}
