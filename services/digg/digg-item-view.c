/*
 * libsocialweb Digg service support
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
#include <json-glib/json-glib.h>

#include <libsocialweb/sw-debug.h>
#include <libsocialweb/sw-utils.h>
#include <libsocialweb/sw-item.h>
#include <libsocialweb-keyfob/sw-keyfob.h>
#include <libsocialweb/sw-cache.h>

#include <rest/rest-proxy.h>
#include <rest/rest-xml-parser.h>
#include <libsoup/soup.h>

#include "digg-item-view.h"
#include "digg.h"

G_DEFINE_TYPE (SwDiggItemView, sw_digg_item_view, SW_TYPE_ITEM_VIEW)

#define GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), SW_TYPE_DIGG_ITEM_VIEW, SwDiggItemViewPrivate))

typedef struct _SwDiggItemViewPrivate SwDiggItemViewPrivate;

struct _SwDiggItemViewPrivate {
  RestProxy *proxy;
  guint timeout_id;
  GHashTable *params;
  gchar *query;
};

enum
{
  PROP_0,
  PROP_PROXY,
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
sw_digg_item_view_get_property (GObject    *object,
                                  guint       property_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  SwDiggItemViewPrivate *priv = GET_PRIVATE (object);

  switch (property_id) {
    case PROP_PROXY:
      g_value_set_object (value, priv->proxy);
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
sw_digg_item_view_set_property (GObject      *object,
                                  guint         property_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  SwDiggItemViewPrivate *priv = GET_PRIVATE (object);

  switch (property_id) {
    case PROP_PROXY:
      priv->proxy = g_value_dup_object (value);
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
sw_digg_item_view_dispose (GObject *object)
{
  SwItemView *item_view = SW_ITEM_VIEW (object);
  SwDiggItemViewPrivate *priv = GET_PRIVATE (object);

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

  g_signal_handlers_disconnect_by_func (sw_item_view_get_service (item_view),
                                        _service_item_hidden_cb,
                                        item_view);
  g_signal_handlers_disconnect_by_func (sw_item_view_get_service (item_view),
                                        _service_user_changed_cb,
                                        item_view);
  g_signal_handlers_disconnect_by_func (sw_item_view_get_service (item_view),
                                        _service_capabilities_changed_cb,
                                        item_view);

  G_OBJECT_CLASS (sw_digg_item_view_parent_class)->dispose (object);
}

static void
sw_digg_item_view_finalize (GObject *object)
{
  SwDiggItemViewPrivate *priv = GET_PRIVATE (object);

  g_free (priv->query);
  g_hash_table_unref (priv->params);

  G_OBJECT_CLASS (sw_digg_item_view_parent_class)->finalize (object);
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
    g_message ("Error from Digg: %s (%d)",
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
    g_message ("Error from Digg: %s",
               rest_proxy_call_get_payload (call));
    return NULL;
  }

  return root;
}

static SwItem *
make_item (SwService *service, JsonNode *entry)
{
  JsonObject *story, *thumbnails, *submiter;
  JsonNode *node;
  SwItem *item;
  const gchar *str;
  time_t date;

  item = sw_item_new ();
  sw_item_set_service (item, service);

  story = json_node_get_object (entry);

  node = json_object_get_member (story, "submiter");
  submiter = json_node_get_object (node);

  node = json_object_get_member (story, "thumbnails");
  thumbnails = json_node_get_object (node);

  /* id */
  str = json_object_get_string_member (story, "story_id");
  sw_item_take (item, "id", g_strconcat ("digg-", str, NULL));   

  /* link */
  str = json_object_get_string_member (story, "permalink");
  sw_item_put (item, "url", str);

  /* title */
  str = json_object_get_string_member (story, "title");
  sw_item_put (item, "title", str);

  /* date */
  date = (gulong) json_object_get_int_member (story, "date_created");
  sw_item_take (item,
                "date",
                sw_time_t_to_string (date));

  /* author */
  str = json_object_get_string_member (submiter, "name");
  sw_item_put (item, "author", str);

  /* authorid */
  str = json_object_get_string_member (submiter, "user_id");
  sw_item_put (item, "authorid", str);

  str = json_object_get_string_member (story, "description");
  if (str) {
    /* content */
    sw_item_put (item, "content", str);

    /*authoricon */
    str = json_object_get_string_member (thumbnails, "large");
    sw_item_request_image_fetch (item, TRUE, "authoricon", str);
  } else {
    /* thumbnail */
    str = json_object_get_string_member (thumbnails, "large");
    sw_item_request_image_fetch (item, TRUE, "thumbnail", str);

    /* authoricon */
    str = json_object_get_string_member (submiter, "icon");
    sw_item_request_image_fetch (item, TRUE, "authoricon", str);
  }

  return item;
}

static void
_got_diggs_cb (RestProxyCall *call,
               const GError  *error,
               GObject       *weak_object,
               gpointer       userdata)
{
  SwItemView *item_view = SW_ITEM_VIEW (weak_object);
  SwDiggItemViewPrivate *priv = GET_PRIVATE (item_view);
  SwService *service;
  SwSet *set;

  JsonParser *parser = NULL;
  JsonNode *root, *node;
  JsonArray *stories_array;
  JsonObject *object;
  guint i, length;

  if (error) {
    g_message ("Error: %s", error->message);
    return;
  }

  parser = json_parser_new ();
  root = node_from_call (call, parser);
  if (!root)
    return;

  /*
  stories : [
    {
         "status": "top",
         "permalink": "http://digg.com/news/science/stem_cell_transplant_has_cured_hiv_infection_in_berlin_patient_say_doctors",
         "description": "Doctors who carried out a stem cell transplant on an HIV-infected man with leukaemia in 2007 say they now believe the man to have been cured of HIV infection as a result of the treatment, which introduced stem cells which happened to be resistant to HIV infection.",
         "title": "Stem cell transplant has CURED HIV infection in 'Berlin patient', say doctors",
         "url": "http://www.aidsmap.com/page/1577949",
         "story_id": "20101214042548:ff70d165-3b3e-43ab-99e7-b92652c43701",
         "diggs": 52,
         "submiter": {
             "username": "ether3al",
             "about": "",
             "user_id": "183849",
             "name": "",
             "icons": [
                 "http://cdn2.diggstatic.com/user/183849/c.3598446662.png",
                 "http://cdn3.diggstatic.com/user/183849/h.3598446662.png",
                 "http://cdn3.diggstatic.com/user/183849/m.3598446662.png",
                 "http://cdn1.diggstatic.com/user/183849/l.3598446662.png",
                 "http://cdn3.diggstatic.com/user/183849/p.3598446662.png",
                 "http://cdn2.diggstatic.com/user/183849/s.3598446662.png",
                 "http://cdn2.diggstatic.com/user/183849/r.3598446662.png"
             ],
             "gender": "",
             "diggs": 2931,
             "comments": 61,
             "followers": 103,
             "location": "",
             "following": 58,
             "submissions": 422,
             "icon": "http://cdn1.diggstatic.com/user/183849/p.3598446662.png"
         },
         "comments": 11,
         "dugg": 0,
         "topic": {
             "clean_name": "science",
             "name": "Science"
         },
         "promote_date": 1292318973,
         "activity": [],
        "date_created": 1292300748,
        "thumbnails": {
            "large": "http://cdn3.diggstatic.com/story/stem_cell_transplant_has_cured_hiv_infection_in_berlin_patient_say_doctors/l.png",
            "small": "http://cdn3.diggstatic.com/story/stem_cell_transplant_has_cured_hiv_infection_in_berlin_patient_say_doctors/s.png",
            "medium": "http://cdn1.diggstatic.com/story/stem_cell_transplant_has_cured_hiv_infection_in_berlin_patient_say_doctors/m.png",
            "thumb": "http://cdn3.diggstatic.com/story/stem_cell_transplant_has_cured_hiv_infection_in_berlin_patient_say_doctors/t.png"
        }
    },
  ]
  */

  object = json_node_get_object (root);
  if (!json_object_has_member (object, "stories"))
    return; 

  set = sw_item_set_new ();
  node = json_object_get_member (object, "stories");

  stories_array = json_node_get_array (node);
  length = json_array_get_length (stories_array);

  service = sw_item_view_get_service (SW_ITEM_VIEW (item_view));

  for (i=0; i<length; i++) {
    JsonNode *entry = json_array_get_element (stories_array, i);
    SwItem *item;

    item = make_item (service, entry);

    if (!sw_service_is_uid_banned (service, sw_item_get (item, "id")))
      sw_set_add (set, (GObject *)item);

    g_object_unref (item);
  }

  g_object_unref (parser);
  g_object_unref (call);

  sw_item_view_set_from_set (item_view, set);
  sw_cache_save (service,
                 priv->query,
                 priv->params,
                 set);

  sw_set_unref (set);
}

static void
_get_status_updates (SwDiggItemView *item_view)
{
  SwDiggItemViewPrivate *priv = GET_PRIVATE (item_view);
  RestProxyCall *call;

  call = rest_proxy_new_call (priv->proxy);
  rest_proxy_call_set_function (call, "2.0/story.getTopNews");

  rest_proxy_call_add_params (call,
                              "limit", "10",
                              NULL);
  rest_proxy_call_async (call, _got_diggs_cb, (GObject *)item_view, NULL, NULL);
}

static gboolean
_update_timeout_cb (gpointer data)
{
  SwDiggItemView *item_view = SW_DIGG_ITEM_VIEW (data);

  _get_status_updates (item_view);

  return TRUE;
}

static void
_load_from_cache (SwDiggItemView *item_view)
{
  SwDiggItemViewPrivate *priv = GET_PRIVATE (item_view);
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
digg_item_view_start (SwItemView *item_view)
{
  SwDiggItemViewPrivate *priv = GET_PRIVATE (item_view);

  if (priv->timeout_id)
  {
    g_warning (G_STRLOC ": View already started.");
  } else {
    priv->timeout_id = g_timeout_add_seconds (UPDATE_TIMEOUT,
                                              (GSourceFunc)_update_timeout_cb,
                                              item_view);
    _load_from_cache ((SwDiggItemView *)item_view);
    _get_status_updates ((SwDiggItemView *)item_view);
  }
}

static void
digg_item_view_stop (SwItemView *item_view)
{
  SwDiggItemViewPrivate *priv = GET_PRIVATE (item_view);

  if (!priv->timeout_id)
  {
    g_warning (G_STRLOC ": View not running");
  } else {
    g_source_remove (priv->timeout_id);
    priv->timeout_id = 0;
  }
}

static void
digg_item_view_refresh (SwItemView *item_view)
{
  _get_status_updates ((SwDiggItemView *)item_view);
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
  SwDiggItemViewPrivate *priv = GET_PRIVATE ((SwDiggItemView*) item_view);

  if (sw_service_has_cap (caps, CREDENTIALS_VALID))
  {
    digg_item_view_refresh (item_view);

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
sw_digg_item_view_constructed (GObject *object)
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

  if (G_OBJECT_CLASS (sw_digg_item_view_parent_class)->constructed)
    G_OBJECT_CLASS (sw_digg_item_view_parent_class)->constructed (object);
}

static void
sw_digg_item_view_class_init (SwDiggItemViewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  SwItemViewClass *item_view_class = SW_ITEM_VIEW_CLASS (klass);
  GParamSpec *pspec;

  g_type_class_add_private (klass, sizeof (SwDiggItemViewPrivate));

  object_class->get_property = sw_digg_item_view_get_property;
  object_class->set_property = sw_digg_item_view_set_property;
  object_class->dispose = sw_digg_item_view_dispose;
  object_class->finalize = sw_digg_item_view_finalize;
  object_class->constructed = sw_digg_item_view_constructed;

  item_view_class->start = digg_item_view_start;
  item_view_class->stop = digg_item_view_stop;
  item_view_class->refresh = digg_item_view_refresh;

  pspec = g_param_spec_object ("proxy",
                               "proxy",
                               "proxy",
                               REST_TYPE_PROXY,
                               G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
  g_object_class_install_property (object_class, PROP_PROXY, pspec);

  pspec = g_param_spec_boxed ("params",
                              "params",
                              "params",
                              G_TYPE_HASH_TABLE,
                              G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
  g_object_class_install_property (object_class, PROP_PARAMS, pspec);

  pspec = g_param_spec_string ("query",
                               "query",
                               "query",
                               NULL,
                               G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
  g_object_class_install_property (object_class, PROP_QUERY, pspec);
}

static void
sw_digg_item_view_init (SwDiggItemView *self)
{
}

