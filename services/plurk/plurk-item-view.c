#include <config.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <libsocialweb/sw-utils.h>

#include <rest/rest-proxy.h>
#include <rest/rest-xml-parser.h>
#include <libsoup/soup.h>
#include <json-glib/json-glib.h>

#include <libsocialweb/sw-debug.h>
#include <libsocialweb/sw-item.h>

#include "plurk-item-view.h"

G_DEFINE_TYPE (SwPlurkItemView,
               sw_plurk_item_view,
               SW_TYPE_ITEM_VIEW)

#define GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), SW_TYPE_PLURK_ITEM_VIEW, SwPlurkItemViewPrivate))

typedef struct _SwPlurkItemViewPrivate SwPlurkItemViewPrivate;

struct _SwPlurkItemViewPrivate {
  RestProxy *proxy;
  char *api_key;
  guint timeout_id;
};

enum
{
  PROP_0,
  PROP_PROXY,
  PROP_APIKEY,
};

#define UPDATE_TIMEOUT 5 * 60

static void
sw_plurk_item_view_get_property (GObject *object, guint property_id,
                                 GValue *value, GParamSpec *pspec)
{
  SwPlurkItemViewPrivate *priv = GET_PRIVATE (object);

  switch (property_id) {
    case PROP_PROXY:
      g_value_set_object (value, priv->proxy);
      break;
    case PROP_APIKEY:
      g_value_set_string (value, priv->api_key);
      break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
sw_plurk_item_view_set_property (GObject *object, guint property_id,
                                 const GValue *value, GParamSpec *pspec)
{
  SwPlurkItemViewPrivate *priv = GET_PRIVATE (object);

  switch (property_id) {
    case PROP_PROXY:
      if (priv->proxy)
      {
        g_object_unref (priv->proxy);
      }
      priv->proxy = g_value_dup_object (value);
      break;
    case PROP_APIKEY:
      if (priv->api_key)
      {
        g_object_unref (priv->api_key);
      }
      priv->api_key = g_value_dup_string (value);
      break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
sw_plurk_item_view_dispose (GObject *object)
{
  SwPlurkItemViewPrivate *priv = GET_PRIVATE (object);

  if (priv->proxy)
  {
    g_object_unref (priv->proxy);
    priv->proxy = NULL;
  }

  if (priv->timeout_id)
  {
    g_source_remove (priv->timeout_id);
    priv->timeout_id = 0;
  }

  G_OBJECT_CLASS (sw_plurk_item_view_parent_class)->dispose (object);
}

static void
sw_plurk_item_view_finalize (GObject *object)
{
  SwPlurkItemViewPrivate *priv = GET_PRIVATE (object);
  if (priv->api_key)
    g_free (priv->api_key);

  G_OBJECT_CLASS (sw_plurk_item_view_parent_class)->finalize (object);
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
_got_status_updates_cb (RestProxyCall *call,
                        const GError  *error,
                        GObject       *weak_object,
                        gpointer       userdata)
{
  SwPlurkItemView *item_view = SW_PLURK_ITEM_VIEW (weak_object);
  SwService *service;
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

  service = sw_item_view_get_service (SW_ITEM_VIEW (item_view));
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
}

static void
_get_status_updates (SwPlurkItemView *item_view)
{
  SwPlurkItemViewPrivate *priv = GET_PRIVATE (item_view);
  RestProxyCall *call;

  call = rest_proxy_new_call (priv->proxy);

  rest_proxy_call_set_function (call, "Timeline/getPlurks");

  rest_proxy_call_add_params (call,
                              "api_key", priv->api_key,
                              "limit", "20",
                              NULL);
  rest_proxy_call_async (call, _got_status_updates_cb, (GObject*)item_view, NULL, NULL);
}

static gboolean
_update_timeout_cb (gpointer data)
{
  SwPlurkItemView *item_view = SW_PLURK_ITEM_VIEW (data);

  _get_status_updates (item_view);

  return TRUE;
}

static void
plurk_item_view_start (SwItemView *item_view)
{
  SwPlurkItemViewPrivate *priv = GET_PRIVATE (item_view);

  if (priv->timeout_id)
  {
    g_warning (G_STRLOC ": View already started.");
  } else {
    priv->timeout_id = g_timeout_add_seconds (UPDATE_TIMEOUT,
                                              (GSourceFunc)_update_timeout_cb,
                                              item_view);
    _get_status_updates ((SwPlurkItemView *)item_view);
  }
}

static void
sw_plurk_item_view_class_init (SwPlurkItemViewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  SwItemViewClass *item_view_class = SW_ITEM_VIEW_CLASS (klass);
  GParamSpec *pspec;

  g_type_class_add_private (klass, sizeof (SwPlurkItemViewPrivate));

  object_class->get_property = sw_plurk_item_view_get_property;
  object_class->set_property = sw_plurk_item_view_set_property;
  object_class->dispose = sw_plurk_item_view_dispose;
  object_class->finalize = sw_plurk_item_view_finalize;

  item_view_class->start = plurk_item_view_start;

  pspec = g_param_spec_object ("proxy",
                               "proxy",
                               "proxy",
                               REST_TYPE_PROXY,
                               G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
  g_object_class_install_property (object_class, PROP_PROXY, pspec);
}

static void
sw_plurk_item_view_init (SwPlurkItemView *self)
{
  SwPlurkItemViewPrivate *priv = GET_PRIVATE (self);
  priv->api_key = NULL;
}
