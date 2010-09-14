#include <config.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <libsocialweb/sw-utils.h>

#include <rest/rest-proxy.h>
#include <rest/rest-xml-parser.h>
#include <libsoup/soup.h>

#include <libsocialweb/sw-debug.h>
#include <libsocialweb/sw-item.h>

#include "sina-item-view.h"

G_DEFINE_TYPE (SwSinaItemView,
               sw_sina_item_view,
               SW_TYPE_ITEM_VIEW)

#define GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), SW_TYPE_SINA_ITEM_VIEW, SwSinaItemViewPrivate))

typedef struct _SwSinaItemViewPrivate SwSinaItemViewPrivate;

struct _SwSinaItemViewPrivate {
  RestProxy *proxy;
  guint timeout_id;
};

enum
{
  PROP_0,
  PROP_PROXY
};

#define UPDATE_TIMEOUT 5 * 60

static void
sw_sina_item_view_get_property (GObject *object, guint property_id,
                                     GValue *value, GParamSpec *pspec)
{
  SwSinaItemViewPrivate *priv = GET_PRIVATE (object);

  switch (property_id) {
    case PROP_PROXY:
      g_value_set_object (value, priv->proxy);
      break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
sw_sina_item_view_set_property (GObject *object, guint property_id,
                                     const GValue *value, GParamSpec *pspec)
{
  SwSinaItemViewPrivate *priv = GET_PRIVATE (object);

  switch (property_id) {
    case PROP_PROXY:
      if (priv->proxy)
      {
        g_object_unref (priv->proxy);
      }
      priv->proxy = g_value_dup_object (value);
      break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
sw_sina_item_view_dispose (GObject *object)
{
  SwSinaItemViewPrivate *priv = GET_PRIVATE (object);

  if (priv->proxy) {
    g_object_unref (priv->proxy);
    priv->proxy = NULL;
  }

  if (priv->timeout_id) {
    g_source_remove (priv->timeout_id);
    priv->timeout_id = 0;
  }

  G_OBJECT_CLASS (sw_sina_item_view_parent_class)->dispose (object);
}

static void
sw_sina_item_view_finalize (GObject *object)
{
  /* free private variables */
  G_OBJECT_CLASS (sw_sina_item_view_parent_class)->finalize (object);
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

static void _get_user_status_updates (SwSinaItemView *item_view, SwSet *set);

static void
_got_user_status_cb (RestProxyCall *call,
                     const GError  *error,
                     GObject       *weak_object,
                     gpointer       userdata)
{
  SwSinaItemView *item_view = SW_SINA_ITEM_VIEW (weak_object);
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
  SwSinaItemView *item_view = SW_SINA_ITEM_VIEW (weak_object);
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

  _get_user_status_updates (item_view, set);
}

static void
_get_user_status_updates (SwSinaItemView *item_view,
                          SwSet          *set)
{
  SwSinaItemViewPrivate *priv = GET_PRIVATE (item_view);
  RestProxyCall *call;

  call = rest_proxy_new_call (priv->proxy);
  rest_proxy_call_set_function (call, "statuses/user_timeline.xml");
  rest_proxy_call_add_params(call,
                             "count", "10",
                             NULL);
  rest_proxy_call_async (call, _got_user_status_cb, (GObject*)item_view, set, NULL);
}

static void
_get_friends_status_update (SwSinaItemView *item_view,
                            SwSet          *set)
{
  SwSinaItemViewPrivate *priv = GET_PRIVATE (item_view);
  RestProxyCall *call;

  call = rest_proxy_new_call (priv->proxy);
  rest_proxy_call_set_function (call, "statuses/friends_timeline.xml");
  rest_proxy_call_add_params(call,
                             "count", "10",
                             NULL);
  rest_proxy_call_async (call, _got_friends_status_cb, (GObject*)item_view, set, NULL);
}

static void
_get_status_updates (SwSinaItemView *item_view)
{
  SwSet *set;

  set = sw_item_set_new ();
  _get_friends_status_update (item_view, set);
}


static gboolean
_update_timeout_cb (gpointer data)
{
  SwSinaItemView *item_view = SW_SINA_ITEM_VIEW (data);

  _get_status_updates (item_view);

  return TRUE;
}

static void
sina_item_view_start (SwItemView *item_view)
{
  SwSinaItemViewPrivate *priv = GET_PRIVATE (item_view);

  if (priv->timeout_id)
  {
    g_warning (G_STRLOC ": View already started.");
  } else {
    priv->timeout_id = g_timeout_add_seconds (UPDATE_TIMEOUT,
                                              (GSourceFunc)_update_timeout_cb,
                                              item_view);
    _get_status_updates ((SwSinaItemView *)item_view);
  }
}

static void
sw_sina_item_view_class_init (SwSinaItemViewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  SwItemViewClass *item_view_class = SW_ITEM_VIEW_CLASS (klass);
  GParamSpec *pspec;

  g_type_class_add_private (klass, sizeof (SwSinaItemViewPrivate));

  object_class->get_property = sw_sina_item_view_get_property;
  object_class->set_property = sw_sina_item_view_set_property;
  object_class->dispose = sw_sina_item_view_dispose;
  object_class->finalize = sw_sina_item_view_finalize;

  item_view_class->start = sina_item_view_start;

  pspec = g_param_spec_object ("proxy",
                               "proxy",
                               "proxy",
                               REST_TYPE_PROXY,
                               G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
  g_object_class_install_property (object_class, PROP_PROXY, pspec);
}

static void
sw_sina_item_view_init (SwSinaItemView *self)
{
  /* Initialize private variables */
}
