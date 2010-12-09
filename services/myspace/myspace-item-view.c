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
#include <stdlib.h>
#include <string.h>
#include <pango/pango.h>

#include <rest/rest-proxy.h>
#include <rest/rest-xml-parser.h>
#include <libsoup/soup.h>

#include <libsocialweb/sw-utils.h>
#include <libsocialweb/sw-debug.h>
#include <libsocialweb/sw-item.h>
#include <libsocialweb/sw-cache.h>

#include "myspace-item-view.h"
#include "myspace.h"

G_DEFINE_TYPE (SwMySpaceItemView,
               sw_myspace_item_view,
               SW_TYPE_ITEM_VIEW)

#define GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), SW_TYPE_MYSPACE_ITEM_VIEW, SwMySpaceItemViewPrivate))

typedef struct _SwMySpaceItemViewPrivate SwMySpaceItemViewPrivate;

struct _SwMySpaceItemViewPrivate {
  RestProxy *proxy;
  guint timeout_id;
  /* TODO remove user_id */
  gchar *user_id;
  GHashTable *params;
  gchar *query;
};

enum
{
  PROP_0,
  PROP_PROXY,
  PROP_USERID,
  PROP_DISPLAYNAME,
  PROP_PROFILEURL,
  PROP_PARAMS,
  PROP_QUERY
};

#define UPDATE_TIMEOUT 5 * 60

static void _service_item_hidden_cb (SwService   *service,
                                     const gchar *uid,
                                     SwItemView  *item_view);
static void _service_user_changed_cb (SwService  *service,
                                      SwItemView *item_view);
static void _service_capabilities_changed_cb (SwService    *service,
                                              const gchar **caps,
                                              SwItemView   *item_view);

static void
sw_myspace_item_view_get_property (GObject    *object,
                                guint       property_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  SwMySpaceItemViewPrivate *priv = GET_PRIVATE (object);

  switch (property_id) {
    case PROP_PROXY:
      g_value_set_object (value, priv->proxy);
      break;
    case PROP_USERID:
      g_value_set_string (value, priv->user_id);
      break;
    case PROP_PARAMS:
      g_value_set_boxed (value, priv->params);
      break;
    case PROP_QUERY:
      g_value_set_string (value, priv->query);
      break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
sw_myspace_item_view_set_property (GObject      *object,
                                   guint         property_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  SwMySpaceItemViewPrivate *priv = GET_PRIVATE (object);

  switch (property_id) {
    case PROP_PROXY:
      if (priv->proxy)
      {
        g_object_unref (priv->proxy);
      }
      priv->proxy = g_value_dup_object (value);
      break;
    case PROP_USERID:
      priv->user_id = g_value_dup_string (value);
      break;
    case PROP_PARAMS:
      priv->params = g_value_dup_boxed (value);
      break;
    case PROP_QUERY:
      priv->query = g_value_dup_string (value);
      break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
sw_myspace_item_view_dispose (GObject *object)
{
  SwItemView *item_view = SW_ITEM_VIEW (object);
  SwMySpaceItemViewPrivate *priv = GET_PRIVATE (object);

  if (priv->proxy) {
    g_object_unref (priv->proxy);
    priv->proxy = NULL;
  }

  if (priv->timeout_id) {
    g_source_remove (priv->timeout_id);
    priv->timeout_id = 0;
  }

  g_signal_handlers_disconnect_by_func (sw_item_view_get_service (item_view),
                                        _service_item_hidden_cb,
                                        item_view);
  g_signal_handlers_disconnect_by_func (sw_item_view_get_service (item_view),
                                        _service_user_changed_cb,
                                        item_view);
  g_signal_handlers_disconnect_by_func (sw_item_view_get_service (item_view),
                                        _service_capabilities_changed_cb,
                                        item_view);

  G_OBJECT_CLASS (sw_myspace_item_view_parent_class)->dispose (object);
}

static void
sw_myspace_item_view_finalize (GObject *object)
{
  SwMySpaceItemViewPrivate *priv = GET_PRIVATE (object);

  /* free private variables */
  g_free (priv->query);
  g_hash_table_unref (priv->params);

  G_OBJECT_CLASS (sw_myspace_item_view_parent_class)->finalize (object);
}

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

static void
_populate_set_from_node (SwService   *service,
                         SwSet       *set,
                         RestXmlNode *root)
{
  RestXmlNode *node;

  if (!root)
    return;

  /*
   * The result of /status is a <user> node, whereas /friends/status is
   * <user><friends><user>. Look closely to find out what data we have
   */
  node = rest_xml_node_find (root, "friends");
  if (node) {
    node = rest_xml_node_find (node, "user");
  } else {
    node = root;
  }

  while (node) {
    /*
      <user>
      <userid>188488921</userid>
      <imageurl>http://c3.ac-images.myspacecdn.com/images02/110/s_768909a648e740939422bdc875ff2bf2.jpg</imageurl>
      <profileurl>http://www.myspace.com/cwiiis</profileurl>
      <name>Cwiiis</name>
      <mood>neutral</mood>
      <moodimageurl>http://x.myspacecdn.com/images/blog/moods/iBrads/amused.gif</moodimageurl>
      <moodlastupdated>15/04/2009 04:20:59</moodlastupdated>
      <status>haha, Ross has myspace</status>
      </user>
    */
    SwItem *item;
    char *id, *status;
    RestXmlNode *subnode;
    gint64 date;

    item = sw_item_new ();
    sw_item_set_service (item, service);

    id = g_strconcat ("myspace-",
                      rest_xml_node_find (node, "userid")->content,
                      "-",
                      rest_xml_node_find (node, "moodlastupdated")->content,
                      NULL);
    sw_item_take (item, "id", id);

    date = atoi (rest_xml_node_find (node, "moodlastupdated")->content);
    /* Time correction. This is bad... */
    date += (15*60*60);
    sw_item_take (item, "date", sw_time_t_to_string (date));

    sw_item_put (item, "authorid", rest_xml_node_find (node, "userid")->content);
    subnode = rest_xml_node_find (node, "name");
    if (subnode && subnode->content)
      sw_item_put (item, "author", subnode->content);

    subnode = rest_xml_node_find (node, "imageurl");
    if (subnode && subnode->content)
      sw_item_request_image_fetch (item, FALSE, "authoricon", subnode->content);

    pango_parse_markup (rest_xml_node_find (node, "status")->content,
                        -1,
                        0,
                        NULL,
                        &status,
                        NULL,
                        NULL);
    sw_item_put (item, "content", status);
    /* TODO: if mood is not "(none)" then append that to the status message */

    subnode = rest_xml_node_find (node, "profileurl");
    if (subnode && subnode->content)
      sw_item_put (item, "url", subnode->content);

    if (!sw_service_is_uid_banned (service, sw_item_get (item, "id"))) {
      sw_set_add (set, G_OBJECT (item));
    }
    g_object_unref (item);

    node = node->next;
  }
}

static void _get_user_status_updates (SwMySpaceItemView *item_view, SwSet *set);

static void
_got_user_status_cb (RestProxyCall *call,
                     const GError  *error,
                     GObject       *weak_object,
                     gpointer       userdata)
{
  SwMySpaceItemView *item_view = SW_MYSPACE_ITEM_VIEW (weak_object);
  SwMySpaceItemViewPrivate *priv = GET_PRIVATE (item_view);
  SwSet *set = (SwSet *)userdata;
  RestXmlNode *root;
  SwService *service;

  if (error) {
    g_message ("Error: %s", error->message);
    return;
  }

  service = sw_item_view_get_service (SW_ITEM_VIEW (item_view));

  root = node_from_call (call);
  _populate_set_from_node (service, set, root);
  rest_xml_node_unref (root);

  g_object_unref (call);

  sw_item_view_set_from_set (SW_ITEM_VIEW (item_view), set);

  /* Save the results of this set to the cache */
  sw_cache_save (service,
                 priv->query,
                 priv->params,
                 set);

  sw_set_unref (set);
}

static void
_got_friends_status_cb (RestProxyCall *call,
                        const GError  *error,
                        GObject       *weak_object,
                        gpointer       userdata)
{
  SwMySpaceItemView *item_view = SW_MYSPACE_ITEM_VIEW (weak_object);
  SwSet *set = (SwSet *)userdata;
  RestXmlNode *root;
  SwService *service;

  if (error) {
    g_message ("Error: %s", error->message);
    return;
  }

  service = sw_item_view_get_service (SW_ITEM_VIEW (item_view));

  root = node_from_call (call);
  _populate_set_from_node (service, set, root);
  rest_xml_node_unref (root);

  g_object_unref (call);

  _get_user_status_updates (item_view, set);
}

static void
_get_user_status_updates (SwMySpaceItemView *item_view,
                          SwSet             *set)
{
  SwMySpaceItemViewPrivate *priv = GET_PRIVATE (item_view);
  RestProxyCall *call;
  char *function;

  call = rest_proxy_new_call (priv->proxy);

  rest_proxy_call_add_params(call,
                             "dateFormat", "utc",
                             "timeZone", "0",
                             NULL);
  function = g_strdup_printf ("v1/users/%s/status", priv->user_id);
  rest_proxy_call_set_function (call, function);
  g_free (function);

  rest_proxy_call_async (call, _got_user_status_cb, (GObject*)item_view, set, NULL);
}

static void
_get_friends_status_updates (SwMySpaceItemView *item_view,
                             SwSet             *set)
{
  SwMySpaceItemViewPrivate *priv = GET_PRIVATE (item_view);
  RestProxyCall *call;
  char *function;

  call = rest_proxy_new_call (priv->proxy);

  rest_proxy_call_add_params(call,
                             "dateFormat", "utc",
                             "timeZone", "0",
                             NULL);
  function = g_strdup_printf ("v1/users/%s/friends/status", priv->user_id);
  rest_proxy_call_set_function (call, function);
  g_free (function);

  rest_proxy_call_async (call, _got_friends_status_cb, (GObject*)item_view, set, NULL);
}

static void
_get_status_updates (SwMySpaceItemView *item_view)
{
  SwMySpaceItemViewPrivate *priv = GET_PRIVATE (item_view);
  GHashTable *params = NULL;
  SwSet *set;

  g_assert (priv->user_id);

  set = sw_item_set_new ();

  if (g_str_equal (priv->query, "own"))
    _get_user_status_updates (item_view, set);
  else if (g_str_equal (priv->query, "feed")) 
    _get_friends_status_updates (item_view, set);
  else
    g_error (G_STRLOC ": Unexpected query '%s'", priv->query);

  if (params)
    g_hash_table_unref (params);
}

static gboolean
_update_timeout_cb (gpointer data)
{
  SwMySpaceItemView *item_view = SW_MYSPACE_ITEM_VIEW (data);

  _get_status_updates (item_view);

  return TRUE;
}

static void
_load_from_cache (SwMySpaceItemView *item_view)
{
  SwMySpaceItemViewPrivate *priv = GET_PRIVATE (item_view);
  SwSet *set;

  set = sw_cache_load (sw_item_view_get_service (SW_ITEM_VIEW (item_view)),
                       priv->query,
                       priv->params);

  if (set)
  {
    sw_item_view_set_from_set (SW_ITEM_VIEW (item_view),
                               set);
    sw_set_unref (set);
  }
}

static void
myspace_item_view_start (SwItemView *item_view)
{
  SwMySpaceItemViewPrivate *priv = GET_PRIVATE (item_view);

  if (priv->timeout_id)
  {
    g_warning (G_STRLOC ": View already started.");
  } else {
    priv->timeout_id = g_timeout_add_seconds (UPDATE_TIMEOUT,
                                              (GSourceFunc)_update_timeout_cb,
                                              item_view);
    _load_from_cache ((SwMySpaceItemView *)item_view);
    _get_status_updates ((SwMySpaceItemView *)item_view);
  }
}

static void
myspace_item_view_stop (SwItemView *item_view)
{
  SwMySpaceItemViewPrivate *priv = GET_PRIVATE (item_view);

  if (!priv->timeout_id)
  {
    g_warning (G_STRLOC ": View not running");
  } else {
    g_source_remove (priv->timeout_id);
    priv->timeout_id = 0;
  }
}

static void
myspace_item_view_refresh (SwItemView *item_view)
{
  _get_status_updates ((SwMySpaceItemView *)item_view);
}

static void
_service_item_hidden_cb (SwService   *service,
                         const gchar *uid,
                         SwItemView  *item_view)
{
  sw_item_view_remove_by_uid (item_view, uid);
}

static void
_service_user_changed_cb (SwService  *service,
                          SwItemView *item_view)
{
  SwSet *set;

  /* We need to empty the set */
  set = sw_item_set_new ();
  sw_item_view_set_from_set (SW_ITEM_VIEW (item_view),
                             set);
  sw_set_unref (set);

  /* And drop the cache */
  sw_cache_drop_all (service);
}

static void
_service_capabilities_changed_cb (SwService    *service,
                                  const gchar **caps,
                                  SwItemView   *item_view)
{
  SwMySpaceItemViewPrivate *priv = GET_PRIVATE ((SwMySpaceItemView*) item_view);

  if (sw_service_has_cap (caps, CREDENTIALS_VALID))
  {
    myspace_item_view_refresh (item_view);

    if (!priv->timeout_id)
    {
      priv->timeout_id = g_timeout_add_seconds (UPDATE_TIMEOUT,
                                                (GSourceFunc)_update_timeout_cb,
                                                item_view);
    }
  } else {
    if (priv->timeout_id)
    {
      g_source_remove (priv->timeout_id);
      priv->timeout_id = 0;
    }
  }
}

static void
sw_myspace_item_view_constructed (GObject *object)
{
  SwItemView *item_view = SW_ITEM_VIEW (object);

  g_signal_connect (sw_item_view_get_service (item_view),
                    "item-hidden",
                    (GCallback)_service_item_hidden_cb,
                    item_view);

  g_signal_connect (sw_item_view_get_service (item_view),
                    "user-changed",
                    (GCallback)_service_user_changed_cb,
                    item_view);

  g_signal_connect (sw_item_view_get_service (item_view),
                    "capabilities-changed",
                    (GCallback)_service_capabilities_changed_cb,
                    item_view);

  if (G_OBJECT_CLASS (sw_myspace_item_view_parent_class)->constructed)
    G_OBJECT_CLASS (sw_myspace_item_view_parent_class)->constructed (object);
}

static void
sw_myspace_item_view_class_init (SwMySpaceItemViewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  SwItemViewClass *item_view_class = SW_ITEM_VIEW_CLASS (klass);
  GParamSpec *pspec;

  g_type_class_add_private (klass, sizeof (SwMySpaceItemViewPrivate));

  object_class->get_property = sw_myspace_item_view_get_property;
  object_class->set_property = sw_myspace_item_view_set_property;
  object_class->dispose = sw_myspace_item_view_dispose;
  object_class->finalize = sw_myspace_item_view_finalize;
  object_class->constructed = sw_myspace_item_view_constructed;

  item_view_class->start = myspace_item_view_start;
  item_view_class->stop = myspace_item_view_stop;
  item_view_class->refresh = myspace_item_view_refresh;

  pspec = g_param_spec_object ("proxy",
                               "proxy",
                               "proxy",
                               REST_TYPE_PROXY,
                               G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
  g_object_class_install_property (object_class, PROP_PROXY, pspec);

  pspec = g_param_spec_string ("user_id",
                               "user_id",
                               "user_id",
                               NULL,
                               G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
  g_object_class_install_property (object_class, PROP_USERID, pspec);

  pspec = g_param_spec_string ("query",
                               "query",
                               "query",
                               NULL,
                               G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
  g_object_class_install_property (object_class, PROP_QUERY, pspec);

  pspec = g_param_spec_boxed ("params",
                              "params",
                              "params",
                              G_TYPE_HASH_TABLE,
                              G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
  g_object_class_install_property (object_class, PROP_PARAMS, pspec);
}

static void
sw_myspace_item_view_init (SwMySpaceItemView *self)
{
  SwMySpaceItemViewPrivate *priv = GET_PRIVATE (self);

  /* Initialize private variables */
  priv->user_id = NULL;
}
