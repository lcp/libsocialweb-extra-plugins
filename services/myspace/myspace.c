/*
 * libsocialweb MySpace service support
 * Copyright (C) 2008 - 2009 Intel Corporation.
 * Copyright (C) 2010 Novell, Inc.
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

#include <time.h>
#include <string.h>
#include <gio/gio.h>
#include <glib/gi18n.h>
#include <gnome-keyring.h>
#include <dbus/dbus-glib-lowlevel.h>

#include <libsocialweb/sw-item.h>
#include <libsocialweb/sw-set.h>
#include <libsocialweb/sw-online.h>
#include <libsocialweb/sw-debug.h>
#include <libsocialweb/sw-utils.h>
#include <libsocialweb/sw-web.h>
#include <libsocialweb/sw-client-monitor.h>
#include <libsocialweb-keyfob/sw-keyfob.h>
#include <libsocialweb-keystore/sw-keystore.h>

#include <rest/oauth-proxy.h>
#include <rest/rest-xml-parser.h>

#include <interfaces/sw-query-ginterface.h>
#include <interfaces/sw-avatar-ginterface.h>
#include <interfaces/sw-status-update-ginterface.h>

#include "myspace.h"
#include "myspace-item-view.h"

static void initable_iface_init (gpointer g_iface, gpointer iface_data);
static void query_iface_init (gpointer g_iface, gpointer iface_data);
static void avatar_iface_init (gpointer g_iface, gpointer iface_data);
static void status_update_iface_init (gpointer g_iface, gpointer iface_data);

G_DEFINE_TYPE_WITH_CODE (SwServiceMySpace,
                         sw_service_myspace,
                         SW_TYPE_SERVICE,
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE,
                                                initable_iface_init)
                         G_IMPLEMENT_INTERFACE (SW_TYPE_QUERY_IFACE,
                                                query_iface_init)
                         G_IMPLEMENT_INTERFACE (SW_TYPE_AVATAR_IFACE,
                                                avatar_iface_init)
                         G_IMPLEMENT_INTERFACE (SW_TYPE_STATUS_UPDATE_IFACE,
                                                status_update_iface_init));


#define GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), SW_TYPE_SERVICE_MYSPACE, SwServiceMySpacePrivate))

struct _SwServiceMySpacePrivate {
  gboolean inited;
  enum {
    OFFLINE,
    CREDS_INVALID,
    CREDS_VALID
  } credentials;
  RestProxy *proxy;
  char *user_id;
  char *image_url;
};

static RestXmlNode *
node_from_call (RestProxyCall *call)
{
  static RestXmlParser *parser = NULL;
  RestXmlNode *root;

  if (call == NULL)
    return NULL;

  if (parser == NULL)
    parser = rest_xml_parser_new ();

  if (!SOUP_STATUS_IS_SUCCESSFUL (rest_proxy_call_get_status_code (call))) {
    g_message ("Error from MySpace: %s (%d)",
               rest_proxy_call_get_status_message (call),
               rest_proxy_call_get_status_code (call));
    return NULL;
  }

  root = rest_xml_parser_parse_from_data (parser,
                                          rest_proxy_call_get_payload (call),
                                          rest_proxy_call_get_payload_length (call));

  if (root == NULL) {
    g_message ("Invalid XML from MySpace: %s",
               rest_proxy_call_get_payload (call));
    return NULL;
  }

  if (strcmp (root->name, "error") != 0) {
    return root;
  } else {
    RestXmlNode *node;
    node = rest_xml_node_find (root, "statusdescription");
    if (node) {
      g_message ("Error: %s", node->content);
    } else {
      g_message ("Error from MySpace: %s",
                 rest_proxy_call_get_payload (call));
    }
    rest_xml_node_unref (root);
    return NULL;
  }
}

/*
 * For a given parent @node, get the child node called @name and return a copy
 * of the content, or NULL. If the content is the empty string, NULL is
 * returned.
 */
static char *
get_child_node_value (RestXmlNode *node, const char *name)
{
  RestXmlNode *subnode;

  g_assert (node);
  g_assert (name);

  subnode = rest_xml_node_find (node, name);
  if (!subnode)
    return NULL;

  if (subnode->content && subnode->content[0])
    return g_strdup (subnode->content);
  else
    return NULL;
}

gboolean
account_is_configured ()
{
  RestProxy *proxy;
  gboolean configured = FALSE;
  const char *key = NULL, *secret = NULL;

  sw_keystore_get_key_secret ("myspace", &key, &secret);
  proxy = oauth_proxy_new (key, secret, "http://api.myspace.com/", FALSE);

  configured = sw_keyfob_oauth_sync ((OAuthProxy *)proxy);

  g_object_unref (proxy);

  return configured;
}

static const char **
get_static_caps (SwService *service)
{
  static const char * caps[] = {
    CAN_VERIFY_CREDENTIALS,
    CAN_UPDATE_STATUS,
    CAN_REQUEST_AVATAR,
    IS_CONFIGURED,
    NULL
  };

  return caps;
}

static const char **
get_dynamic_caps (SwService *service)
{
  SwServiceMySpace *myspace = SW_SERVICE_MYSPACE (service);
  SwServiceMySpacePrivate *priv = GET_PRIVATE (myspace);
  gboolean configured = FALSE;
  static const char * caps[] = {
    IS_CONFIGURED,
    CREDENTIALS_VALID,
    CAN_UPDATE_STATUS,
    CAN_REQUEST_AVATAR,
    NULL
  };
  static const char * no_caps[] = { NULL };
  static const char * configured_caps[] = {
    IS_CONFIGURED,
    NULL
  };

  if (myspace->priv->user_id)
    return caps;

  configured = account_is_configured ((OAuthProxy *)priv->proxy);

  if (configured) 
    return configured_caps;
  else
    return no_caps;
}

static void
got_user_cb (RestProxyCall *call,
             const GError  *error,
             GObject       *weak_object,
             gpointer       userdata)
{
  SwService *service = SW_SERVICE (weak_object);
  SwServiceMySpace *myspace = SW_SERVICE_MYSPACE (service);
  SwServiceMySpacePrivate *priv = myspace->priv;
  RestXmlNode *node;

  if (error) {
    g_message ("Error: %s", error->message);
    return;
  }

  node = node_from_call (call);
  if (!node)
    return;

  priv->user_id = get_child_node_value (node, "userid");
  priv->image_url = get_child_node_value (node, "imageuri");

  rest_xml_node_unref (node);

  sw_service_emit_capabilities_changed (service, get_dynamic_caps (service));
}

static void
got_tokens_cb (RestProxy *proxy, gboolean authorised, gpointer user_data)
{
  SwServiceMySpace *myspace = SW_SERVICE_MYSPACE (user_data);
  SwServiceMySpacePrivate *priv = myspace->priv;
  RestProxyCall *call;

  if (authorised) {
    call = rest_proxy_new_call (priv->proxy);
    rest_proxy_call_set_function (call, "v1/user");
    rest_proxy_call_async (call, got_user_cb, (GObject*)myspace, NULL, NULL);
  }
}

static const char *
sw_service_myspace_get_name (SwService *service)
{
  return "myspace";
}

static void
online_notify (gboolean online, gpointer user_data)
{
  SwServiceMySpace *service = (SwServiceMySpace *) user_data;
  SwServiceMySpacePrivate *priv = service->priv;

  if (online) {
    sw_keyfob_oauth ((OAuthProxy *)priv->proxy, got_tokens_cb, service);
  } else {
    g_free (priv->user_id);
    priv->user_id = NULL;

    g_free (priv->image_url);
    priv->image_url = NULL;

    sw_service_emit_capabilities_changed ((SwService *)service,
                                          get_dynamic_caps ((SwService *)service));
  }
}

static void
refresh_credentials (SwServiceMySpace *myspace)
{
  SwServiceMySpacePrivate *priv = myspace->priv;
  sw_keyfob_oauth ((OAuthProxy *)priv->proxy, got_tokens_cb, myspace);
}

static void
credentials_updated (SwService *service)
{
  refresh_credentials (SW_SERVICE_MYSPACE (service));
}

static void
sw_service_myspace_dispose (GObject *object)
{
  SwServiceMySpacePrivate *priv = SW_SERVICE_MYSPACE (object)->priv;

  sw_online_remove_notify (online_notify, object);

  if (priv->proxy) {
    g_object_unref (priv->proxy);
    priv->proxy = NULL;
  }

  g_free (priv->user_id);
  g_free (priv->image_url);

  G_OBJECT_CLASS (sw_service_myspace_parent_class)->dispose (object);
}

static void
sw_service_myspace_finalize (GObject *object)
{
  SwServiceMySpacePrivate *priv = SW_SERVICE_MYSPACE (object)->priv;

  g_free (priv->user_id);

  G_OBJECT_CLASS (sw_service_myspace_parent_class)->finalize (object);
}

static void
sw_service_myspace_class_init (SwServiceMySpaceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  SwServiceClass *service_class = SW_SERVICE_CLASS (klass);

  g_type_class_add_private (klass, sizeof (SwServiceMySpacePrivate));

  object_class->dispose = sw_service_myspace_dispose;
  object_class->finalize = sw_service_myspace_finalize;

  service_class->get_name = sw_service_myspace_get_name;
  service_class->get_static_caps = get_static_caps;
  service_class->get_dynamic_caps = get_dynamic_caps;
  service_class->credentials_updated = credentials_updated;
}

static void
sw_service_myspace_init (SwServiceMySpace *self)
{
  self->priv = GET_PRIVATE (self);
  self->priv->inited = FALSE;
}

/* Initable interface */

static gboolean
sw_service_myspace_initable (GInitable     *initable,
                             GCancellable  *cancellable,
                             GError       **error)
{
  SwServiceMySpace *myspace = SW_SERVICE_MYSPACE (initable);
  SwServiceMySpacePrivate *priv = myspace->priv;
  const char *key = NULL, *secret = NULL;

  if (priv->inited)
    return TRUE;

  sw_keystore_get_key_secret ("myspace", &key, &secret);
  if (key == NULL || secret == NULL) {
    g_set_error_literal (error,
                         SW_SERVICE_ERROR,
                         SW_SERVICE_ERROR_NO_KEYS,
                         "No API key configured");
    return FALSE;
  }
  priv->proxy = oauth_proxy_new (key, secret, "http://api.myspace.com/", FALSE);

  if (sw_is_online ()) {
    online_notify (TRUE, myspace);
  }
  sw_online_add_notify (online_notify, myspace);

  priv->inited = TRUE;

  return TRUE;
}

static void
initable_iface_init (gpointer g_iface, gpointer iface_data)
{
  GInitableIface *klass = (GInitableIface *)g_iface;

  klass->init = sw_service_myspace_initable;
}

/* Query interface */

static const gchar *valid_queries[] = {"feed",
                                       "own"};
static gboolean
_check_query_validity (const gchar *query)
{
  gint i = 0;

  for (i = 0; i < G_N_ELEMENTS(valid_queries); i++)
  {
    if (g_str_equal (query, valid_queries[i]))
      return TRUE;
  }

  return FALSE;
}

static void
_myspace_query_open_view (SwQueryIface          *self,
                          const gchar           *query,
                          GHashTable            *params,
                          DBusGMethodInvocation *context)
{
  SwServiceMySpacePrivate *priv = GET_PRIVATE (self);
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

  item_view = g_object_new (SW_TYPE_MYSPACE_ITEM_VIEW,
                            "proxy", priv->proxy,
                            "user_id", priv->user_id,
                            "service", self,
                            "query", query,
                            "params", params,
                            NULL);

  object_path = sw_item_view_get_object_path (item_view);
  /* Ensure the object gets disposed when the client goes away */
  sw_client_monitor_add (dbus_g_method_get_sender (context),
                         (GObject *)item_view);
  sw_query_iface_return_from_open_view (context,
                                        object_path);
}

static void
query_iface_init (gpointer g_iface,
                  gpointer iface_data)
{
  SwQueryIfaceClass *klass = (SwQueryIfaceClass*)g_iface;

  sw_query_iface_implement_open_view (klass,
                                      _myspace_query_open_view);
}

/* Avatar interface */

static void
_requested_avatar_downloaded_cb (const gchar *uri,
                                 gchar       *local_path,
                                 gpointer     userdata)
{
  SwService *service = SW_SERVICE (userdata);

  sw_avatar_iface_emit_avatar_retrieved (service, local_path);
  g_free (local_path);
}

static void
_myspace_avatar_request_avatar (SwAvatarIface     *self,
                                DBusGMethodInvocation *context)
{
  SwServiceMySpacePrivate *priv = GET_PRIVATE (self);

  if (priv->image_url) {
    sw_web_download_image_async (priv->image_url,
                                     _requested_avatar_downloaded_cb,
                                     self);
  }

  sw_avatar_iface_return_from_request_avatar (context);
}

static void
avatar_iface_init (gpointer g_iface,
                   gpointer iface_data)
{
  SwAvatarIfaceClass *klass = (SwAvatarIfaceClass *)g_iface;

  sw_avatar_iface_implement_request_avatar (klass, _myspace_avatar_request_avatar);
}

/* Status Update interface */

static void
_update_status_cb (RestProxyCall *call,
                   const GError  *error,
                   GObject       *weak_object,
                   gpointer       userdata)
{
  if (error)
  {
    g_critical (G_STRLOC ": Error updating status: %s",
                error->message);
    sw_status_update_iface_emit_status_updated (weak_object, FALSE);
  } else {
    sw_status_update_iface_emit_status_updated (weak_object, TRUE);
  }
}

static void
_myspace_status_update_update_status (SwStatusUpdateIface   *self,
                                      const gchar           *msg,
                                      GHashTable            *fields,
                                      DBusGMethodInvocation *context)
{
  SwServiceMySpace *myspace = (SwServiceMySpace *)self;
  SwServiceMySpacePrivate *priv = myspace->priv;
  RestProxyCall *call;
  gchar *function;

  if (!priv->user_id)
    return;

  call = rest_proxy_new_call (priv->proxy);
  rest_proxy_call_set_method (call, "PUT");
  function = g_strdup_printf ("v1/users/%s/status", priv->user_id);
  rest_proxy_call_set_function (call, function);
  g_free (function);

  rest_proxy_call_add_params (call,
                              "userId", priv->user_id,
                              "status", msg,
                              NULL);
  rest_proxy_call_async (call, _update_status_cb, (GObject *)self, NULL, NULL);
  sw_status_update_iface_return_from_update_status (context);
}

static void
status_update_iface_init (gpointer g_iface,
                          gpointer iface_data)
{
  SwStatusUpdateIfaceClass *klass = (SwStatusUpdateIfaceClass*)g_iface;

  sw_status_update_iface_implement_update_status (klass,
                                                      _myspace_status_update_update_status);
}
