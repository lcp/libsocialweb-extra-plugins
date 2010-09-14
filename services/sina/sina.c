#include <config.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include "sina.h"
#include <libsocialweb/sw-item.h>
#include <libsocialweb/sw-set.h>
#include <libsocialweb/sw-online.h>
#include <libsocialweb/sw-utils.h>
#include <libsocialweb/sw-web.h>
#include <libsocialweb/sw-debug.h>
#include <libsocialweb-keyfob/sw-keyfob.h>
#include <libsocialweb-keystore/sw-keystore.h>
//#include <rest/oauth-proxy.h>
//#include <rest/oauth-proxy-call.h>
#include <gconf/gconf-client.h>
#include <rest/rest-xml-parser.h>
#include <libsoup/soup.h>

#include <interfaces/sw-query-ginterface.h>
#include <interfaces/sw-avatar-ginterface.h>
#include <interfaces/sw-status-update-ginterface.h>

#include "sina-item-view.h"

static void initable_iface_init (gpointer g_iface, gpointer iface_data);
static void query_iface_init (gpointer g_iface, gpointer iface_data);
static void avatar_iface_init (gpointer g_iface, gpointer iface_data);
static void status_update_iface_init (gpointer g_iface, gpointer iface_data);

G_DEFINE_TYPE_WITH_CODE (SwServiceSina,
                         sw_service_sina,
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
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), SW_TYPE_SERVICE_SINA, SwServiceSinaPrivate))

struct _SwServiceSinaPrivate {
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
  char *image_url;
  char *username, *password;
  char *api_key;
  char *authentication;
  GConfClient *gconf;
  guint gconf_notify_id[2];
};

#define KEY_BASE "/apps/libsocialweb/services/sina"
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
  SwServiceSina *sina = SW_SERVICE_SINA (service);
  SwServiceSinaPrivate *priv = GET_PRIVATE (sina);
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

static void
sina_basic_auth_set_params (SwServiceSina *sina, RestProxyCall *call)
{
  SwServiceSinaPrivate *priv = GET_PRIVATE (sina);
  char *basic_auth;

  if (priv->authentication) {
    basic_auth = g_strconcat ("Basic ", priv->authentication, NULL);
    rest_proxy_call_add_header (call, "Authorization", basic_auth);
    g_free (basic_auth);
    rest_proxy_call_add_params (call,
                                "source", priv->api_key,
                                NULL);
  }
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
    g_message ("Error from Sina: %s (%d)",
               rest_proxy_call_get_status_message (call),
               rest_proxy_call_get_status_code (call));
    return NULL;
  }

  root = rest_xml_parser_parse_from_data (parser,
                                          rest_proxy_call_get_payload (call),
                                          rest_proxy_call_get_payload_length (call));

  if (root == NULL) {
    g_message ("Error from Sina: %s",
               rest_proxy_call_get_payload (call));
    return NULL;
  }

  return root;
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

static const char **
get_static_caps (SwService *service)
{
  static const char * caps[] = {
    CAN_VERIFY_CREDENTIALS,
    CAN_UPDATE_STATUS,
    CAN_REQUEST_AVATAR,
    NULL
  };

  return caps;
}

static const char **
get_dynamic_caps (SwService *service)
{
  SwServiceSinaPrivate *priv = SW_SERVICE_SINA (service)->priv;
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
  SwServiceSina *sina = (SwServiceSina*)service;

  sina->priv->running = TRUE;
}

static char*
make_date (const char *s)
{
  /* TODO Take care of timezone */
  /* Fri Dec 25 13:07:17 +0800 2009 */
  struct tm tm;
  strptime (s, "%A %h %d %T%Z %Y", &tm);
  return sw_time_t_to_string (timegm (&tm));
}

static void
_populate_set_from_node (SwService   *service,
                         SwSet       *set,
                         RestXmlNode *root)
{
  RestXmlNode *node;

  if (!root)
    return;

  node = rest_xml_node_find (root, "status");
  while (node) {
    SwItem *item;
    RestXmlNode *user;
    char *id, *date, *uid, *url;

    item = sw_item_new ();
    sw_item_set_service (item, service);

    user = rest_xml_node_find (node, "user");

    id = g_strconcat ("sina-",
                      get_child_node_value (node, "id"),
                      NULL);
    sw_item_take (item, "id", id);

    date = get_child_node_value (node, "created_at");
    sw_item_take (item, "date", make_date (date));
    g_free (date);

    sw_item_take (item,
                  "author",
                  get_child_node_value (user, "screen_name"));

    url = get_child_node_value (user, "profile_image_url");
    sw_item_request_image_fetch (item, FALSE, "authoricon", url);
    g_free (url);

    sw_item_take (item,
                  "content",
                  get_child_node_value (node, "text"));

    uid = get_child_node_value (user, "id");
    url = g_strconcat ("http://t.sina.com.cn/", uid, NULL);
    sw_item_take (item, "url", url);
    g_free (uid);

    sw_set_add (set, G_OBJECT (item));
    g_object_unref (item);

    /* Next node */
    node = node->next;
  }
}

static void _get_user_status_updates (SwServiceSina *sina, SwSet *set);

static void
_got_user_status_cb (RestProxyCall *call,
                     const GError  *error,
                     GObject       *weak_object,
                     gpointer       userdata)
{
  SwService *service = SW_SERVICE (weak_object);
  RestXmlNode *root;
  SwSet *set = (SwSet *)userdata;

  if (error) {
    g_message ("Error: %s", error->message);
    return;
  }

  root = node_from_call (call);
  _populate_set_from_node (service, set, root);
  rest_xml_node_unref (root);

  if (!sw_set_is_empty (set))
    sw_service_emit_refreshed (service, set);

  sw_set_unref (set);
}

static void
_got_friends_status_cb (RestProxyCall *call,
                        const GError  *error,
                        GObject       *weak_object,
                        gpointer       userdata)
{
  SwService *service = SW_SERVICE (weak_object);
  SwServiceSina *sina = SW_SERVICE_SINA (service);
  SwServiceSinaPrivate *priv = GET_PRIVATE (sina);
  RestXmlNode *root;
  SwSet *set = (SwSet *)userdata;

  if (error) {
    g_message ("Error: %s", error->message);
    return;
  }

  root = node_from_call (call);
  _populate_set_from_node (service, set, root);
  rest_xml_node_unref (root);

  if (priv->type == BOTH)
  {
    _get_user_status_updates (sina, set);
    return;
  }

  if (!sw_set_is_empty (set))
    sw_service_emit_refreshed (service, set);

  sw_set_unref (set);
}

static void
_get_user_status_updates (SwServiceSina *sina,
                          SwSet         *set)
{
  SwServiceSinaPrivate *priv = GET_PRIVATE (sina);
  RestProxyCall *call;

  call = rest_proxy_new_call (priv->proxy);
  rest_proxy_call_set_function (call, "statuses/user_timeline.xml");
  sina_basic_auth_set_params (sina, call);
  rest_proxy_call_add_params(call,
                             "count", "10",
                             NULL);
  rest_proxy_call_async (call, _got_user_status_cb, (GObject*)sina, set, NULL);
}

static void
_get_friends_status_update (SwServiceSina *sina,
                            SwSet         *set)
{
  SwServiceSinaPrivate *priv = GET_PRIVATE (sina);
  RestProxyCall *call;

  call = rest_proxy_new_call (priv->proxy);
  rest_proxy_call_set_function (call, "statuses/friends_timeline.xml");
  sina_basic_auth_set_params (sina, call);
  rest_proxy_call_add_params(call,
                             "count", "10",
                             NULL);
  rest_proxy_call_async (call, _got_friends_status_cb, (GObject*)sina, set, NULL);
}

static void
get_status_updates (SwServiceSina *sina)
{
  SwServiceSinaPrivate *priv = GET_PRIVATE (sina);
  SwSet *set;

  if (!priv->authentication)
    return;

  set = sw_item_set_new ();

  if (sw_service_get_param ((SwService *)sina, "own")) {
    priv->type = OWN;
  } else if (sw_service_get_param ((SwService *)sina, "friends")){
    priv->type = FRIENDS;
  } else {
    priv->type = BOTH;
  }

  if (priv->type == OWN) {
    _get_user_status_updates (sina, set);
  } else {
    /* For BOTH this triggers into user */
    _get_friends_status_update (sina, set);
  }
}

static void
refresh (SwService *service)
{
  SwServiceSina *sina = SW_SERVICE_SINA (service);
  SwServiceSinaPrivate *priv = GET_PRIVATE (sina);

  if (priv->running && priv->proxy && priv->authentication)
    get_status_updates (sina);
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
  SwServiceSinaPrivate *priv = GET_PRIVATE (service);

  if (priv->image_url) {
    sw_web_download_image_async (priv->image_url,
                                 avatar_downloaded_cb,
                                 service);
  }
}

static void
got_user_cb (RestProxyCall *call,
             const GError  *error,
             GObject       *weak_object,
             gpointer       userdata)
{
  SwService *service = SW_SERVICE (weak_object);
  SwServiceSina *sina = SW_SERVICE_SINA (service);
  SwServiceSinaPrivate *priv = GET_PRIVATE (sina);

  RestXmlNode *root;

  if (error) {
    priv->credentials = CREDS_INVALID;
    g_message ("Error: %s", error->message);
    g_message ("Sina: %s", rest_proxy_call_get_payload (call));
    return;
  }

  root = node_from_call (call);
  if (!root) {
    priv->credentials = CREDS_INVALID;
    return;
  }

  priv->credentials = CREDS_VALID;
  priv->image_url = get_child_node_value (root, "profile_image_url");

  rest_xml_node_unref (root);

  sw_service_emit_capabilities_changed
    (service, get_dynamic_caps (service));

  if (priv->running)
    get_status_updates (sina);
}

static void
online_notify (gboolean online, gpointer user_data)
{
  SwServiceSina *sina = SW_SERVICE_SINA (user_data);
  SwServiceSinaPrivate *priv = GET_PRIVATE (sina);
  RestProxyCall *call;

  if (online) {
    if (priv->username && priv->password) {
      char *str = g_strconcat (priv->username, ":", priv->password, NULL);
      priv->authentication = g_base64_encode (str, strlen (str));
      g_free (str);

      priv->proxy = rest_proxy_new ("http://api.t.sina.com.cn/", FALSE);
      call = rest_proxy_new_call (priv->proxy);
      rest_proxy_call_set_function (call, "account/verify_credentials.xml");
      sina_basic_auth_set_params (sina, call);
      rest_proxy_call_async (call, got_user_cb, (GObject*)sina, NULL, NULL);

      priv->credentials = OFFLINE;
    } else {
      priv->credentials = OFFLINE;
      sw_service_emit_refreshed ((SwService *)sina, NULL);
    }
  } else {
    if (priv->proxy) {
      g_object_unref (priv->proxy);
      priv->proxy = NULL;
    }

    g_free (priv->image_url);
    priv->image_url = NULL;

    g_free (priv->authentication);
    priv->authentication = NULL;

    sw_service_emit_capabilities_changed ((SwService *)sina,
                                          get_dynamic_caps ((SwService *)sina));
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
sw_service_sina_get_name (SwService *service)
{
  return "sina";
}

static void
sw_service_sina_dispose (GObject *object)
{
  SwServiceSinaPrivate *priv = SW_SERVICE_SINA (object)->priv;

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

  G_OBJECT_CLASS (sw_service_sina_parent_class)->dispose (object);
}

static void
sw_service_sina_finalize (GObject *object)
{
  SwServiceSinaPrivate *priv = SW_SERVICE_SINA (object)->priv;

  g_free (priv->image_url);
  g_free (priv->username);
  g_free (priv->password);
  g_free (priv->api_key);
  g_free (priv->authentication);

  G_OBJECT_CLASS (sw_service_sina_parent_class)->finalize (object);
}

static void
sw_service_sina_class_init (SwServiceSinaClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  SwServiceClass *service_class = SW_SERVICE_CLASS (klass);

  g_type_class_add_private (klass, sizeof (SwServiceSinaPrivate));

  object_class->dispose = sw_service_sina_dispose;
  object_class->finalize = sw_service_sina_finalize;

  service_class->get_name = sw_service_sina_get_name;
  service_class->start = start;
  service_class->refresh = refresh;
  service_class->get_static_caps = get_static_caps;
  service_class->get_dynamic_caps = get_dynamic_caps;
  service_class->request_avatar = request_avatar;
  service_class->credentials_updated = credentials_updated;
}

static void
sw_service_sina_init (SwServiceSina *self)
{
  self->priv = GET_PRIVATE (self);
  self->priv->inited = FALSE;
}

/* Initable interface */

static gboolean
sw_service_sina_initable (GInitable    *initable,
                          GCancellable *cancellable,
                          GError      **error)
{
  /* Initialize the service and return TRUE if everything is OK.
     Otherwise, return FALSE */
  SwServiceSina *sina = SW_SERVICE_SINA (initable);
  SwServiceSinaPrivate *priv = GET_PRIVATE (sina);
  const char *key = NULL, *secret = NULL;

  if (priv->inited)
    return TRUE;

  sw_keystore_get_key_secret ("sina", &key, &secret);
  if (key == NULL || secret == NULL) {
    g_set_error_literal (error,
                         SW_SERVICE_ERROR,
                         SW_SERVICE_ERROR_NO_KEYS,
                         "No API key configured");
    return FALSE;
  }

  priv->api_key = g_strdup (key);

  if (sw_service_get_param ((SwService *)sina, "own")) {
    priv->type = OWN;
  } else if (sw_service_get_param ((SwService *)sina, "friends")){
    priv->type = FRIENDS;
  } else {
    priv->type = BOTH;
  }

  priv->credentials = OFFLINE;

  priv->gconf = gconf_client_get_default ();
  gconf_client_add_dir (priv->gconf, KEY_BASE,
                        GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);
  priv->gconf_notify_id[0] = gconf_client_notify_add (priv->gconf, KEY_USER,
                                                      auth_changed_cb, sina,
                                                      NULL, NULL);
  priv->gconf_notify_id[1] = gconf_client_notify_add (priv->gconf, KEY_PASS,
                                                      auth_changed_cb, sina,
                                                      NULL, NULL);
  gconf_client_notify (priv->gconf, KEY_USER);
  gconf_client_notify (priv->gconf, KEY_PASS);

  sw_online_add_notify (online_notify, sina);

  priv->inited = TRUE;

  return TRUE;
}

static void
initable_iface_init (gpointer g_iface, gpointer iface_data)
{
  GInitableIface *klass = (GInitableIface *)g_iface;

  klass->init = sw_service_sina_initable;
}

/* Query interface */
static void
_sina_query_open_view (SwQueryIface          *self,
                       GHashTable            *params,
                       DBusGMethodInvocation *context)
{
  SwServiceSinaPrivate *priv = GET_PRIVATE (self);
  SwItemView *item_view;
  const gchar *object_path;

  item_view = g_object_new (SW_TYPE_SINA_ITEM_VIEW,
                            "proxy", priv->proxy,
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
                                      _sina_query_open_view);
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
_sina_avatar_request_avatar (SwAvatarIface         *self,
                             DBusGMethodInvocation *context)
{
  SwServiceSinaPrivate *priv = GET_PRIVATE (self);

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
                                            _sina_avatar_request_avatar);
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
_sina_status_update_update_status (SwStatusUpdateIface   *self,
                                   const gchar           *msg,
                                   GHashTable            *fields,
                                   DBusGMethodInvocation *context)
{
  SwServiceSina *sina = SW_SERVICE_SINA (self);
  SwServiceSinaPrivate *priv = GET_PRIVATE (sina);
  RestProxyCall *call;

  call = rest_proxy_new_call (priv->proxy);
  rest_proxy_call_set_method (call, "POST");
  rest_proxy_call_set_function (call, "statuses/update.xml");
  sina_basic_auth_set_params (sina, call);

  rest_proxy_call_add_params (call,
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
                                                  _sina_status_update_update_status);
}

