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
  /* What else? */
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
  /* unref private variables */

  G_OBJECT_CLASS (sw_sina_item_view_parent_class)->dispose (object);
}

static void
sw_sina_item_view_finalize (GObject *object)
{
  /* free private variables */
  G_OBJECT_CLASS (sw_sina_item_view_parent_class)->finalize (object);
}

static RestXmlNode *
_make_node_from_call (RestProxyCall *call)
{
  static RestXmlParser *parser = NULL;
  RestXmlNode *root;

  if (call == NULL)
    return NULL;

  if (parser == NULL)
    parser = rest_xml_parser_new ();

  if (!SOUP_STATUS_IS_SUCCESSFUL (rest_proxy_call_get_status_code (call))) {
    g_warning (G_STRLOC ": Error from Sina: %s (%d)",
               rest_proxy_call_get_status_message (call),
               rest_proxy_call_get_status_code (call));
    return NULL;
  }

  root = rest_xml_parser_parse_from_data (parser,
                                          rest_proxy_call_get_payload (call),
                                          rest_proxy_call_get_payload_length (call));

  if (root == NULL) {
    g_warning (G_STRLOC ": Error parsing payload from Sina: %s",
               rest_proxy_call_get_payload (call));
    return NULL;
  }

  return root;
}

static gboolean
_update_timeout_cb (gpointer data)
{
  /* Update status */

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
    /* Update status */
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
