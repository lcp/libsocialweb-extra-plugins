/*
 * libsocialweb Youtube service support
 *
 * Copyright (C) 2010 Novell, Inc.
 *
 * Author: Gary Ching-Pang Lin <glin@novell.com>
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
#include "youtube.h"
#include <libsocialweb/sw-item.h>
#include <libsocialweb/sw-set.h>
#include <libsocialweb/sw-online.h>
#include <libsocialweb/sw-utils.h>
#include <libsocialweb/sw-web.h>
#include <libsocialweb/sw-debug.h>
#include <libsocialweb-keyfob/sw-keyfob.h>
#include <libsocialweb-keystore/sw-keystore.h>
#include <gconf/gconf-client.h>
#include <rest/oauth-proxy.h>
#include <rest/oauth-proxy-call.h>
#include <rest/rest-xml-parser.h>
#include <libsoup/soup.h>

#include <interfaces/sw-query-ginterface.h>
#include <interfaces/sw-avatar-ginterface.h>
#include <interfaces/sw-status-update-ginterface.h>

static void initable_iface_init (gpointer g_iface, gpointer iface_data);

G_DEFINE_TYPE_WITH_CODE (SwServiceYoutube,
                         sw_service_youtube,
                         SW_TYPE_SERVICE,
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE,
                                                initable_iface_init));

#define GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), SW_TYPE_SERVICE_YOUTUBE, SwServiceYoutubePrivate))

#define KEY_BASE "/apps/libsocialweb/services/youtube"
#define KEY_USER KEY_BASE "/user"
#define KEY_PASS KEY_BASE "/password"

struct _SwServiceYoutubePrivate {
  gboolean inited;
  enum {
    OWN,
    FRIENDS,
    BOTH
  } type;
  enum {
    OFFLINE,
    CREDS_INVALID,
    CREDS_VALID
  } credentials;
  gboolean running;
  RestProxy *proxy;
  char *username;
  char *password;
  char *developer_key;
  char *user_auth;
  char *nickname;
  GConfClient *gconf;
  guint gconf_notify_id[2];
  GHashTable *thumb_map;
};

static void online_notify (gboolean online, gpointer user_data);
static void credentials_updated (SwService *service);

RestXmlNode *
node_from_call (RestProxyCall *call)
{
  static RestXmlParser *parser = NULL;
  RestXmlNode *root;

  if (call == NULL)
    return NULL;

  if (parser == NULL)
    parser = rest_xml_parser_new ();

  if (!SOUP_STATUS_IS_SUCCESSFUL (rest_proxy_call_get_status_code (call))) {
    g_message ("Error from Youtube: %s (%d)",
               rest_proxy_call_get_status_message (call),
               rest_proxy_call_get_status_code (call));
    return NULL;
  }

  root = rest_xml_parser_parse_from_data (parser,
                                          rest_proxy_call_get_payload (call),
                                          rest_proxy_call_get_payload_length (call));

  if (root == NULL) {
    g_message ("Error from Youtube: %s",
               rest_proxy_call_get_payload (call));
    return NULL;
  }

  if (strcmp (root->name, "error_response") == 0) {
    RestXmlNode *node;
    node = rest_xml_node_find (root, "error_msg");
    g_message ("Error response from Youtube: %s\n", node->content);
    rest_xml_node_unref (root);
    return NULL;
  } else {
    return root;
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

  if (!node || !name)
    return NULL;

  subnode = rest_xml_node_find (node, name);
  if (!subnode)
    return NULL;

  if (subnode->content && subnode->content[0])
    return g_strdup (subnode->content);
  else
    return NULL;
}

static void
start (SwService *service)
{
  SwServiceYoutubePrivate *priv = SW_SERVICE_YOUTUBE (service)->priv;

  priv->running = TRUE;
}

static char *
get_author_icon_url (SwService *service, const char *author)
{
  SwServiceYoutubePrivate *priv = SW_SERVICE_YOUTUBE (service)->priv;
  RestProxyCall *call;
  RestXmlNode *root, *node;
  char *function, *url;

  url = g_hash_table_lookup (priv->thumb_map, author);
  if (url)
    return g_strdup (url);

  call = rest_proxy_new_call (priv->proxy);
  function = g_strdup_printf ("users/%s", author);
  rest_proxy_call_set_function (call, function);
  rest_proxy_call_sync (call, NULL);

  root = node_from_call (call);
  if (!root)
    return NULL;

  node = rest_xml_node_find (root, "media:thumbnail");
  if (!node)
    return NULL;

  url = g_strdup (rest_xml_node_get_attr (node, "url"));

  g_free (function);

  if (url)
    g_hash_table_insert(priv->thumb_map, (gpointer)author, (gpointer)g_strdup (url));

  return url;
}

static char *
get_utc_date (const char *s)
{
  struct tm tm;

  if (s == NULL)
    return NULL;

  strptime (s, "%Y-%m-%dT%T", &tm);

  return sw_time_t_to_string (mktime (&tm));
}

static void
got_video_list_cb (RestProxyCall *call,
                   const GError  *error,
                   GObject       *weak_object,
                   gpointer       user_data)
{
  SwService *service = SW_SERVICE (weak_object);
  RestXmlNode *root, *node;
  SwSet *set;

  root = node_from_call (call);
  if (!root)
    return;

  node = rest_xml_node_find (root, "channel");
  if (!node)
    return;

  node = rest_xml_node_find (node, "item");

  set = sw_item_set_new ();

  while (node){
    /*
    <rss>
      <channel>
        <item>
          <guid isPermaLink="false">http://gdata.youtube.com/feeds/api/videos/<videoid></guid>
          <atom:updated>2010-02-13T06:17:32.000Z</atom:updated>
          <title>Video Title</title>
          <author>Author Name</author>
          <link>http://www.youtube.com/watch?v=<videoid>&amp;feature=youtube_gdata</link>
          <media:group>
            <media:thumbnail url="http://i.ytimg.com/vi/<videoid>/default.jpg" height="90" width="120" time="00:03:00.500"/>
          </media:group>
        </item>
      </channel>
    </rss>
    */
    SwItem *item;
    char *author, *date, *url;
    RestXmlNode *subnode, *thumb_node;

    item = sw_item_new ();
    sw_item_set_service (item, service);

    sw_item_put (item, "id", get_child_node_value (node, "guid"));

    date = get_child_node_value (node, "atom:updated");
    if (date != NULL)
      sw_item_put (item, "date", get_utc_date(date));

    sw_item_put (item, "title", get_child_node_value (node, "title"));
    sw_item_put (item, "url", get_child_node_value (node, "link"));
    author = get_child_node_value (node, "author");
    sw_item_put (item, "author", author);

    /* media:group */
    subnode = rest_xml_node_find (node, "media:group");
    if (subnode){
      thumb_node = rest_xml_node_find (subnode, "media:thumbnail");
      url = (char *) rest_xml_node_get_attr (thumb_node, "url");
      sw_item_request_image_fetch (item, TRUE, "thumbnail", url);
    }

    url = get_author_icon_url (service, author);
    sw_item_request_image_fetch (item, FALSE, "authoricon", url);
    g_free (url);

    sw_set_add (set, G_OBJECT (item));
    g_object_unref (item);

    node = node->next;
  }

  rest_xml_node_unref (root);

  if (!sw_set_is_empty (set))
    sw_service_emit_refreshed (service, set);

  sw_set_unref (set);
}

static void
refresh (SwService *service)
{
  SwServiceYoutubePrivate *priv = SW_SERVICE_YOUTUBE (service)->priv;
  RestProxyCall *call;
  char *function, *user_auth, *devkey;

  if (!priv->running || !priv->username || 
      !priv->proxy   || !priv->user_auth) {
    return;
  }

  call = rest_proxy_new_call (priv->proxy);
  function = g_strdup_printf ("users/default/newsubscriptionvideos");
  user_auth = g_strdup_printf ("GoogleLogin auth=%s", priv->user_auth);
  devkey = g_strdup_printf ("key=%s", priv->developer_key);
  rest_proxy_call_add_header (call, "Authorization", user_auth);
  rest_proxy_call_add_header (call, "X-GData-Key", devkey);
  rest_proxy_call_set_function (call, function);
  rest_proxy_call_add_params (call,
                              "max-results", "10",
                              "alt", "rss",
                              NULL);
  rest_proxy_call_async (call,
                         (RestProxyCallAsyncCallback)got_video_list_cb,
                         (GObject*)service,
                         NULL,
                         NULL);
  g_free (function);
  g_free (user_auth);
  g_free (devkey);
}

static const char **
get_static_caps (SwService *service)
{
  static const char * caps[] = {
    CAN_VERIFY_CREDENTIALS,
    NULL
  };

  return caps;
}

static const char **
get_dynamic_caps (SwService *service)
{
  SwServiceYoutubePrivate *priv = GET_PRIVATE (service);
  static const char *no_caps[] = { NULL };
  static const char *configured_caps[] = {
    IS_CONFIGURED,
    NULL
  };
  static const char *invalid_caps[] = {
    IS_CONFIGURED,
    CREDENTIALS_INVALID,
    NULL
  };
  static const char *full_caps[] = {
    IS_CONFIGURED,
    CREDENTIALS_VALID,
    NULL
  };

  switch (priv->credentials) {
  case CREDS_VALID:
    return full_caps;
  case CREDS_INVALID:
    return invalid_caps;
  case OFFLINE:
    if (priv->username && priv->password)
      return configured_caps;
    else
      return no_caps;
  }

  /* Just in case we fell through that switch */
  g_warning ("Unhandled credential state %d", priv->credentials);
  return no_caps;
}

static void
_got_user_auth (RestProxyCall *call,
               const GError  *error,
               GObject       *weak_object,
               gpointer       user_data)
{
  SwService *service = SW_SERVICE (weak_object);
  SwServiceYoutubePrivate *priv = SW_SERVICE_YOUTUBE (service)->priv;
  const char *message = rest_proxy_call_get_payload (call);
  char **tokens;

  if (error) {
    g_message ("Error: %s", error->message);
    g_message ("Error from Youtube: %s", message);
    priv->credentials = CREDS_INVALID;
    sw_service_emit_capabilities_changed (service, get_dynamic_caps (service));
    return;
  }

  /* Parse the message */
  tokens = g_strsplit_set (message, "=\n", -1);
  if (g_strcmp0 (tokens[0], "Auth") == 0 &&
      g_strcmp0 (tokens[2], "YouTubeUser") == 0) {
    priv->user_auth = g_strdup (tokens[1]);
    /*Alright! we got the auth!!!*/
    g_object_unref (priv->proxy);
    priv->proxy = rest_proxy_new ("http://gdata.youtube.com/feeds/api/", FALSE);

    priv->nickname = g_strdup (tokens[3]);
    priv->credentials = CREDS_VALID;
  } else {
    priv->credentials = CREDS_INVALID;
  }

  g_strfreev(tokens);

  sw_service_emit_capabilities_changed (service, get_dynamic_caps (service));

  if (priv->user_auth)
    refresh (service);
}

static void
online_notify (gboolean online, gpointer user_data)
{
  SwServiceYoutube *youtube = (SwServiceYoutube *)user_data;
  SwServiceYoutubePrivate *priv = GET_PRIVATE (youtube);

  if (online) {
    if (priv->username && priv->password) {
      RestProxyCall *call;

      /* request user_auth */
      /* http://code.google.com/intl/zh-TW/apis/youtube/2.0/developers_guide_protocol_clientlogin.html */
      char *function;
      priv->proxy = rest_proxy_new ("https://www.google.com/youtube/accounts/", FALSE);
      call = rest_proxy_new_call (priv->proxy);
      function = g_strdup_printf ("ClientLogin");
      rest_proxy_call_set_method (call, "POST");
      rest_proxy_call_set_function (call, function);
      rest_proxy_call_add_params (call,
                                  "Email", priv->username,
                                  "Passwd", priv->password,
                                  "service", "youtube",
                                  "source", "SUSE MeeGo",
                                  NULL);
      rest_proxy_call_add_header (call,
                                  "Content-Type",
                                  "application/x-www-form-urlencoded");
      rest_proxy_call_async (call,
                             (RestProxyCallAsyncCallback)_got_user_auth,
                             (GObject*)youtube,
                             NULL,
                             NULL);
      g_free (function);
      priv->credentials = OFFLINE;
    } else {
      priv->credentials = OFFLINE;
      sw_service_emit_refreshed ((SwService *)youtube, NULL);
    }
  } else {
    if (priv->proxy) {
      g_object_unref (priv->proxy);
      priv->proxy = NULL;
    }
    if (priv->user_auth) {
      g_free (priv->user_auth);
      priv->user_auth = NULL;
    }
    if (priv->nickname) {
      g_free (priv->nickname);
      priv->nickname = NULL;
    }
    priv->credentials = OFFLINE;
    sw_service_emit_capabilities_changed ((SwService *)youtube,
                                          get_dynamic_caps ((SwService *)youtube));
  }
}

static void
credentials_updated (SwService *service)
{
  sw_service_emit_user_changed (service);

  /* If we're online, force a reconnect to fetch new credentials */
  if (sw_is_online ()) {
    online_notify (FALSE, service);
    online_notify (TRUE, service);
  }

  sw_service_emit_user_changed (service);
  sw_service_emit_capabilities_changed ((SwService *)service,
                                        get_dynamic_caps (service));
}

static const char *
sw_service_youtube_get_name (SwService *service)
{
  return "youtube";
}

static void
sw_service_youtube_dispose (GObject *object)
{
  SwServiceYoutubePrivate *priv = SW_SERVICE_YOUTUBE (object)->priv;

  sw_online_remove_notify (online_notify, object);

  if (priv->proxy) {
    g_object_unref (priv->proxy);
    priv->proxy = NULL;
  }

  G_OBJECT_CLASS (sw_service_youtube_parent_class)->dispose (object);
}

static void
sw_service_youtube_finalize (GObject *object)
{
  SwServiceYoutubePrivate *priv = SW_SERVICE_YOUTUBE (object)->priv;

  if (priv->username)
    g_free (priv->username);

  if (priv->password)
    g_free (priv->password);

  if (priv->user_auth)
    g_free (priv->user_auth);

  if (priv->nickname)
    g_free (priv->nickname);

  g_hash_table_destroy (priv->thumb_map);

  G_OBJECT_CLASS (sw_service_youtube_parent_class)->finalize (object);
}

static void
sw_service_youtube_class_init (SwServiceYoutubeClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  SwServiceClass *service_class = SW_SERVICE_CLASS (klass);

  g_type_class_add_private (klass, sizeof (SwServiceYoutubePrivate));

  object_class->dispose = sw_service_youtube_dispose;
  object_class->finalize = sw_service_youtube_finalize;

  service_class->get_name = sw_service_youtube_get_name;
  service_class->start = start;
  service_class->refresh = refresh;
  service_class->get_static_caps = get_static_caps;
  service_class->get_dynamic_caps = get_dynamic_caps;
  service_class->credentials_updated = credentials_updated;
}

static void
sw_service_youtube_init (SwServiceYoutube *self)
{
  self->priv = GET_PRIVATE (self);
  self->priv->inited = FALSE;
  self->priv->user_auth = NULL;
}

/* Initable interface */

static void
user_changed_cb (GConfClient *client, guint cnxn_id, GConfEntry *entry, gpointer user_data)
{
  SwService *service = SW_SERVICE (user_data);
  SwServiceYoutube *youtube = SW_SERVICE_YOUTUBE (service);
  SwServiceYoutubePrivate *priv = GET_PRIVATE (youtube);
  const char *username = NULL, *password = NULL;
  gboolean updated = FALSE;

  if (g_str_equal (entry->key, KEY_USER)) {
    if (entry->value)
      username = gconf_value_get_string (entry->value);
    if (username && username[0] == '\0')
      username = NULL;

    if (g_strcmp0 (priv->username, username) != 0) {
      priv->username = g_strdup (username);
      updated = TRUE;
    }
  } else if (g_str_equal (entry->key, KEY_PASS)) {
    if (entry->value)
      password = gconf_value_get_string (entry->value);
    if (password && password[0] == '\0')
      password = NULL;

    if (g_strcmp0 (priv->password, password) != 0) {
      priv->password = g_strdup (password);
      updated = TRUE;
    }
  }

  if (updated)
    credentials_updated (service);
  else
    sw_service_emit_refreshed (service, NULL);
}

static gboolean
sw_service_youtube_initable (GInitable    *initable,
                             GCancellable *cancellable,
                             GError      **error)
{
  SwServiceYoutube *youtube = SW_SERVICE_YOUTUBE (initable);
  SwServiceYoutubePrivate *priv = GET_PRIVATE (youtube);
  const char *key = NULL;

  if (priv->inited)
    return TRUE;

  sw_keystore_get_key_secret ("youtube", &key, NULL);
  if (key == NULL) {
    g_set_error_literal (error,
                         SW_SERVICE_ERROR,
                         SW_SERVICE_ERROR_NO_KEYS,
                         "No API key configured");
    return FALSE;
  }

  priv->developer_key = (char *)key;
  priv->credentials = OFFLINE;
  
  priv->gconf = gconf_client_get_default ();
  gconf_client_add_dir (priv->gconf, KEY_BASE,
                        GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);
  priv->gconf_notify_id[0] = gconf_client_notify_add (priv->gconf, KEY_USER,
                                                      user_changed_cb, youtube,
                                                      NULL, NULL);
  priv->gconf_notify_id[1] = gconf_client_notify_add (priv->gconf, KEY_PASS,
                                                      user_changed_cb, youtube,
                                                      NULL, NULL);
  gconf_client_notify (priv->gconf, KEY_USER);
  gconf_client_notify (priv->gconf, KEY_PASS);

  priv->thumb_map = g_hash_table_new(g_str_hash, g_str_equal);

  sw_online_add_notify (online_notify, youtube);

  return TRUE;
}

static void
initable_iface_init (gpointer g_iface, gpointer iface_data)
{
  GInitableIface *klass = (GInitableIface *)g_iface;

  klass->init = sw_service_youtube_initable;
}
