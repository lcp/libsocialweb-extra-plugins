#include <config.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include "plurk.h"
#include <libsocialweb/sw-item.h>
#include <libsocialweb/sw-set.h>
#include <libsocialweb/sw-online.h>
#include <libsocialweb/sw-utils.h>
#include <libsocialweb/sw-web.h>
#include <libsocialweb/sw-debug.h>
#include <libsocialweb-keyfob/sw-keyfob.h>
#include <libsocialweb-keystore/sw-keystore.h>
#include <gconf/gconf-client.h>
#include <rest/rest-xml-parser.h>
#include <libsoup/soup.h>
#include <json-glib/json-glib.h>

#include <interfaces/sw-query-ginterface.h>
#include <interfaces/sw-avatar-ginterface.h>
#include <interfaces/sw-status-update-ginterface.h>

#include "plurk-item-view.h"

static void initable_iface_init (gpointer g_iface, gpointer iface_data);
static void query_iface_init (gpointer g_iface, gpointer iface_data);
static void avatar_iface_init (gpointer g_iface, gpointer iface_data);
static void status_update_iface_init (gpointer g_iface, gpointer iface_data);

G_DEFINE_TYPE_WITH_CODE (SwServicePlurk,
                         sw_service_plurk,
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
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), SW_TYPE_SERVICE_PLURK, SwServicePlurkPrivate))

struct _SwServicePlurkPrivate {
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
  char *user_id;
  char *image_url;
  char *username, *password;
  char *api_key;
  GConfClient *gconf;
  guint gconf_notify_id[2];
};


#define KEY_BASE "/apps/libsocialweb/services/plurk"
#define KEY_USER KEY_BASE "/user"
#define KEY_PASS KEY_BASE "/password"

static void online_notify (gboolean online, gpointer user_data);
static void credentials_updated (SwService *service);

static void
auth_changed_cb (GConfClient *client,
                 guint        cnxn_id,
                 GConfEntry  *entry,
                 gpointer     user_data)
{
  SwService *service = SW_SERVICE (user_data);
  SwServicePlurk *plurk = SW_SERVICE_PLURK (service);
  SwServicePlurkPrivate *priv = GET_PRIVATE (plurk);
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
}


static JsonNode *
node_from_call (RestProxyCall *call, JsonParser *parser)
{
  JsonNode *root;
  GError *error;
  gboolean ret = FALSE;

  if (call == NULL)
    return NULL;

  if (!SOUP_STATUS_IS_SUCCESSFUL (rest_proxy_call_get_status_code (call))) {
    g_message ("Error from Plurk: %s (%d)",
               rest_proxy_call_get_status_message (call),
               rest_proxy_call_get_status_code (call));
    return NULL;
  }

  ret = json_parser_load_from_data (parser,
                                    rest_proxy_call_get_payload (call),
                                    rest_proxy_call_get_payload_length (call),
                                    &error);
  root = json_parser_get_root (parser);

  if (root == NULL) {
    g_message ("Error from Plurk: %s",
               rest_proxy_call_get_payload (call));
    return NULL;
  }

  return root;
}

static char *
construct_image_url (const char *uid,
                     const gint64 avatar,
                     const gint64 has_profile)
{
  char *url = NULL;

  if (has_profile == 1 && avatar <= 0)
    url = g_strdup_printf ("http://avatars.plurk.com/%s-medium.gif", uid);
  else if (has_profile == 1 && avatar > 0)
    url = g_strdup_printf ("http://avatars.plurk.com/%s-medium%lld.gif", uid, avatar);
  else
    url = g_strdup_printf ("http://www.plurk.com/static/default_medium.gif");

  return url;
}

static gchar *
base36_encode (const gchar *source)
{
  gchar *encoded = NULL, *tmp, c;
  gint64 dividend, quotient;
  const gint64 divisor = 36;

  dividend = g_ascii_strtoll (source, NULL, 10);

  while (dividend > 0) {
    quotient = dividend % divisor;
    dividend = dividend / divisor;

    if (quotient < 10)
      c = '0' + quotient;
    else
      c = 'a' + quotient - 10;

    if (encoded != NULL) {
      tmp = g_strdup_printf ("%c%s", c, encoded);
      g_free (encoded);
      encoded = tmp;
    } else {
      encoded = g_strdup_printf ("%c", c);
    }
  }
  return encoded;
}

static char *
make_date (const char *s)
{
  struct tm tm;
  strptime (s, "%A, %d %h %Y %H:%M:%S GMT", &tm);
  return sw_time_t_to_string (timegm (&tm));
}

static void
plurk_cb (RestProxyCall *call,
          const GError  *error,
          GObject       *weak_object,
          gpointer       userdata)
{
  SwService *service = SW_SERVICE (weak_object);
  JsonParser *parser = NULL;
  JsonNode *root, *plurks, *plurk_users, *node;
  JsonArray *j_array;
  JsonObject *object;
  SwSet *set;
  GList *plurks_ids = NULL, *list = NULL;

  if (error) {
    g_message ("Error: %s", error->message);
    g_message ("Error: %s", rest_proxy_call_get_payload(call));
    return;
  }

  parser = json_parser_new ();

  root = node_from_call (call, parser);
  if (!root)
    return;

  object = json_node_get_object (root);
  if (!json_object_has_member (object, "plurks") ||
      !json_object_has_member (object, "plurk_users"))
    return;

  set = sw_item_set_new ();
  plurks = json_object_get_member (object, "plurks");
  plurk_users = json_object_get_member (object, "plurk_users");

  /* Parser the data and file the set */
  j_array = json_node_get_array (plurks);
  plurks_ids = json_array_get_elements (j_array);

  for (list=plurks_ids; list ;list = g_list_next (list)) {
    JsonObject *plurk, *user;
    char *uid, *pid, *url, *date, *base36, *content;
    const char *name, *qualifier;
    gint64 id, avatar, has_profile;
    SwItem *item;

    item = sw_item_new ();
    sw_item_set_service (item, service);

    /* Get the plurk object */
    node = (JsonNode *) list->data;
    plurk = json_node_get_object (node);

    if (!json_object_has_member (plurk, "owner_id"))
      continue;

    /* Get the user object */
    id = json_object_get_int_member (plurk, "owner_id");
    uid = g_strdup_printf ("%lld", id);
    object = json_node_get_object (plurk_users);
    node = json_object_get_member (object, uid);
    user = json_node_get_object (node);

    if (!user)
      continue;

    /* authorid */
    sw_item_take (item, "authorid", uid);

    /* Construct the id of sw_item */
    id = json_object_get_int_member (plurk, "plurk_id");
    pid = g_strdup_printf ("%lld", id);
    sw_item_take (item, "id", g_strconcat ("plurk-", pid, NULL));

    /* Get the display name of the user */
    name = json_object_get_string_member (user, "full_name");
    sw_item_put (item, "author", name);

    /* Construct the avatar url */
    avatar = json_object_get_int_member (user, "avatar");
    has_profile = json_object_get_int_member (user, "has_profile_image");
    url = construct_image_url (uid, avatar, has_profile);
    sw_item_request_image_fetch (item, FALSE, "authoricon", url);
    g_free (url);

    /* Construct the content of the plurk*/
    if (json_object_has_member (plurk, "qualifier_translated"))
      qualifier = json_object_get_string_member (plurk, "qualifier_translated");
    else
      qualifier = json_object_get_string_member (plurk, "qualifier");
    content = g_strdup_printf ("%s %s",
                               qualifier,
                               json_object_get_string_member (plurk, "content_raw"));
    sw_item_take (item, "content", content);

    /* Get the post date of this plurk*/
    date = make_date (json_object_get_string_member (plurk, "posted"));
    sw_item_take (item, "date", date);

    /* Construt the link of the user */
    base36 = base36_encode (pid);
    url = g_strconcat ("http://www.plurk.com/p/", base36, NULL);
    g_free (base36);
    sw_item_take (item, "url", url);

    /* Add the item into the set */
    sw_set_add (set, G_OBJECT (item));
    g_object_unref (item);
  }

  if (!sw_set_is_empty (set))
    sw_service_emit_refreshed ((SwService *)service, set);

  g_list_free (plurks_ids);
  g_object_unref (parser);
  g_object_unref (call);
}

static void
get_status_updates (SwServicePlurk *plurk)
{
  SwServicePlurkPrivate *priv = GET_PRIVATE (plurk);
  RestProxyCall *call;

  if (!priv->user_id || !priv->running)
    return;

  call = rest_proxy_new_call (priv->proxy);
  switch (priv->type) {
  case OWN:
  case FRIENDS:
  case BOTH:
    rest_proxy_call_set_function (call, "Timeline/getPlurks");
    break;
  }

  rest_proxy_call_add_params (call,
                              "api_key", priv->api_key,
                              "limit", "20",
                              NULL);
  rest_proxy_call_async (call, plurk_cb, (GObject*)plurk, NULL, NULL);
}

static const char **
get_static_caps (SwService *service)
{
  static const char * caps[] = {
    CAN_VERIFY_CREDENTIALS,
    CAN_UPDATE_STATUS,
    CAN_REQUEST_AVATAR,
    CAN_GEOTAG,
    NULL
  };

  return caps;
}

static const char **
get_dynamic_caps (SwService *service)
{
  SwServicePlurkPrivate *priv = GET_PRIVATE (service);
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
    CAN_UPDATE_STATUS,
    CAN_REQUEST_AVATAR,
    NULL
  };

  /* Check the conditions and determine which caps array to return */
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
start (SwService *service)
{
  SwServicePlurk *plurk = (SwServicePlurk*)service;

  plurk->priv->running = TRUE;
}

static void
refresh (SwService *service)
{
  SwServicePlurk *plurk = (SwServicePlurk*)service;
  SwServicePlurkPrivate *priv = GET_PRIVATE (plurk);

  if (priv->running && priv->username && priv->password && priv->proxy)
    get_status_updates (plurk);
}

static void
avatar_downloaded_cb (const gchar *uri,
                      gchar       *local_path,
                      gpointer     userdata)
{
  SwService *service = SW_SERVICE (userdata);

  sw_service_emit_avatar_retrieved (service, local_path);
  g_free (local_path);
}

static void
request_avatar (SwService *service)
{
  SwServicePlurkPrivate *priv = GET_PRIVATE (service);

  if (priv->image_url) {
    sw_web_download_image_async (priv->image_url,
                                     avatar_downloaded_cb,
                                     service);
  }
}

static void
construct_user_data (SwServicePlurk* plurk, JsonNode *root)
{
  SwServicePlurkPrivate *priv = GET_PRIVATE (plurk);
  JsonNode *node;
  JsonObject *object;
  const gchar *uid;
  gint64 id, avatar, has_profile;

  object = json_node_get_object (root);
  node = json_object_get_member (object, "user_info");

  if (!node)
    return;

  object = json_node_get_object (node);
  if (json_object_get_null_member (object, "uid"))
    return;

  id = json_object_get_int_member (object, "uid");

  avatar = json_object_get_int_member (object, "avatar");

  has_profile = json_object_get_int_member (object, "has_profile_image");

  uid = g_strdup_printf ("%lld", id);

  priv->user_id = (char *) uid;
  priv->image_url = construct_image_url (uid, avatar, has_profile);
}

static void
got_login_data (RestProxyCall *call,
                const GError  *error,
                GObject       *weak_object,
                gpointer       userdata)
{
  SwService *service = SW_SERVICE (weak_object);
  SwServicePlurk *plurk = SW_SERVICE_PLURK (service);
  JsonParser *parser = NULL;
  JsonNode *root;

  if (error) {
    // TODO sanity_check_date (call);
    g_message ("Error: %s", error->message);

    plurk->priv->credentials = CREDS_INVALID;
    sw_service_emit_capabilities_changed (service, get_dynamic_caps (service));

    return;
  }

  plurk->priv->credentials = CREDS_VALID;

  sw_service_emit_capabilities_changed (service, get_dynamic_caps (service));

  parser = json_parser_new ();
  root = node_from_call (call, parser);
  construct_user_data (plurk, root);
  g_object_unref (root);

  g_object_unref (call);
  refresh (service);
}

static void
online_notify (gboolean online, gpointer user_data)
{
  SwServicePlurk *plurk = (SwServicePlurk *)user_data;
  SwServicePlurkPrivate *priv = GET_PRIVATE (plurk);

  if (online) {
    /* Whatever have to do while online */
    if (priv->username && priv->password) {
      RestProxyCall *call;

      priv->proxy = rest_proxy_new ("http://www.plurk.com/API/", FALSE);

      call = rest_proxy_new_call (priv->proxy);
      rest_proxy_call_set_function (call, "Users/login");
      rest_proxy_call_add_params (call,
                                  "api_key", priv->api_key,
                                  "username", priv->username,
                                  "password", priv->password,
                                  NULL);
      rest_proxy_call_async (call, got_login_data, (GObject*)plurk, NULL, NULL);
      /* Set offline for now and wait for access_token_cb to return */
      priv->credentials = OFFLINE;
    } else {
      priv->credentials = OFFLINE;
      sw_service_emit_refreshed ((SwService *)plurk, NULL);
    }
  } else {
    /* Whatever have to do while offline */
    if (priv->proxy) {
      g_object_unref (priv->proxy);
      priv->proxy = NULL;
    }
    if (priv->user_id) {
      g_free (priv->user_id);
      priv->user_id = NULL;
    }

    priv->credentials = OFFLINE;

    sw_service_emit_capabilities_changed ((SwService *)plurk,
                                          get_dynamic_caps ((SwService *)plurk));
  }
}

static void
credentials_updated (SwService *service)
{
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
sw_service_plurk_get_name (SwService *service)
{
  return "plurk";
}

static void
sw_service_plurk_dispose (GObject *object)
{
  SwServicePlurkPrivate *priv = SW_SERVICE_PLURK (object)->priv;

  sw_online_remove_notify (online_notify, object);

  /* unref private variables */
  if (priv->proxy) {
    g_object_unref (priv->proxy);
    priv->proxy = NULL;
  }

  if (priv->gconf) {
    gconf_client_notify_remove (priv->gconf, priv->gconf_notify_id[0]);
    gconf_client_notify_remove (priv->gconf, priv->gconf_notify_id[1]);
    g_object_unref (priv->gconf);
    priv->gconf = NULL;
  }

  G_OBJECT_CLASS (sw_service_plurk_parent_class)->dispose (object);
}

static void
sw_service_plurk_finalize (GObject *object)
{
  /* Free private variables*/
  SwServicePlurkPrivate *priv = SW_SERVICE_PLURK (object)->priv;

  if (priv->user_id)
    g_free (priv->user_id);

  if (priv->image_url)
    g_free (priv->image_url);

  if (priv->username)
    g_free (priv->username);

  if (priv->password)
    g_free (priv->password);

  G_OBJECT_CLASS (sw_service_plurk_parent_class)->finalize (object);
}

static void
sw_service_plurk_class_init (SwServicePlurkClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  SwServiceClass *service_class = SW_SERVICE_CLASS (klass);

  g_type_class_add_private (klass, sizeof (SwServicePlurkPrivate));

  object_class->dispose = sw_service_plurk_dispose;
  object_class->finalize = sw_service_plurk_finalize;

  service_class->get_name = sw_service_plurk_get_name;
  service_class->start = start;
  service_class->refresh = refresh;
  service_class->get_static_caps = get_static_caps;
  service_class->get_dynamic_caps = get_dynamic_caps;
  service_class->request_avatar = request_avatar;
  service_class->credentials_updated = credentials_updated;
}

static void
sw_service_plurk_init (SwServicePlurk *self)
{
  self->priv = GET_PRIVATE (self);
  self->priv->inited = FALSE;
}

/* Initable interface */

static gboolean
sw_service_plurk_initable (GInitable    *initable,
                           GCancellable *cancellable,
                           GError      **error)
{
  /* Initialize the service and return TRUE if everything is OK.
     Otherwise, return FALSE */
  SwServicePlurk *plurk = SW_SERVICE_PLURK (initable);
  SwServicePlurkPrivate *priv = GET_PRIVATE (plurk);
  const char *key = NULL;

  if (priv->inited)
    return TRUE;

  sw_keystore_get_key_secret ("plurk", &key, NULL);
  if (key == NULL) {
    g_set_error_literal (error,
                         SW_SERVICE_ERROR,
                         SW_SERVICE_ERROR_NO_KEYS,
                         "No API key configured");
    return FALSE;
  }

  priv->api_key = g_strdup (key);

  if (sw_service_get_param ((SwService *)plurk, "own")) {
    priv->type = OWN;
  } else if (sw_service_get_param ((SwService *)plurk, "friends")){
    priv->type = FRIENDS;
  } else {
    priv->type = BOTH;
  }

  priv->credentials = OFFLINE;

  priv->gconf = gconf_client_get_default ();
  gconf_client_add_dir (priv->gconf, KEY_BASE,
                        GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);
  priv->gconf_notify_id[0] = gconf_client_notify_add (priv->gconf, KEY_USER,
                                                      auth_changed_cb, plurk,
                                                      NULL, NULL);
  priv->gconf_notify_id[1] = gconf_client_notify_add (priv->gconf, KEY_PASS,
                                                      auth_changed_cb, plurk,
                                                      NULL, NULL);
  gconf_client_notify (priv->gconf, KEY_USER);
  gconf_client_notify (priv->gconf, KEY_PASS);

  sw_online_add_notify (online_notify, plurk);

  priv->inited = TRUE;

  return TRUE;

}

static void
initable_iface_init (gpointer g_iface, gpointer iface_data)
{
  GInitableIface *klass = (GInitableIface *)g_iface;

  klass->init = sw_service_plurk_initable;
}

/* Query interface */
static void
_plurk_query_open_view (SwQueryIface          *self,
                        GHashTable            *params,
                        DBusGMethodInvocation *context)
{
  SwServicePlurkPrivate *priv = GET_PRIVATE (self);
  SwItemView *item_view;
  const gchar *object_path;

  item_view = g_object_new (SW_TYPE_PLURK_ITEM_VIEW,
                            "proxy", priv->proxy,
                            "api_key", priv->api_key,
                            "service", self,
                            NULL);

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
                                      _plurk_query_open_view);
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
_plurk_avatar_request_avatar (SwAvatarIface         *self,
                              DBusGMethodInvocation *context)
{
  SwServicePlurkPrivate *priv = GET_PRIVATE (self);

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
  SwAvatarIfaceClass *klass = (SwAvatarIfaceClass*)g_iface;

  sw_avatar_iface_implement_request_avatar (klass,
                                            _plurk_avatar_request_avatar);
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
    SW_DEBUG (TWITTER, G_STRLOC ": Status updated.");
    sw_status_update_iface_emit_status_updated (weak_object, TRUE);
  }
}

static void
_plurk_status_update_update_status (SwStatusUpdateIface   *self,
                                    const gchar           *msg,
                                    GHashTable            *fields,
                                    DBusGMethodInvocation *context)
{
  SwServicePlurk *plurk = SW_SERVICE_PLURK (self);
  SwServicePlurkPrivate *priv = GET_PRIVATE (plurk);
  RestProxyCall *call;

  if (!priv->user_id)
    return;

  call = rest_proxy_new_call (priv->proxy);
  rest_proxy_call_set_method (call, "POST");
  rest_proxy_call_set_function (call, "Timeline/plurkAdd");

  rest_proxy_call_add_params (call,
                              "api_key", priv->api_key,
                              "content", msg,
                              "qualifier", ":",
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
                                                  _plurk_status_update_update_status);
}

