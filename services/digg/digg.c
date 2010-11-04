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
#include <libsocialweb/sw-service.h>
#include <libsocialweb/sw-item.h>
#include <libsocialweb/sw-utils.h>
#include <libsocialweb/sw-web.h>
#include <libsocialweb-keystore/sw-keystore.h>
#include <rest/rest-proxy.h>
#include <rest/rest-xml-parser.h>
#include <gconf/gconf-client.h>
#include <json-glib/json-glib.h>

#include "digg.h"

G_DEFINE_TYPE (SwServiceDigg, sw_service_digg, SW_TYPE_SERVICE)

#define GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), SW_TYPE_SERVICE_DIGG, SwServiceDiggPrivate))

#define KEY_BASE "/apps/libsocialweb/services/digg"
#define KEY_USER KEY_BASE "/user"

struct _SwServiceDiggPrivate {
  gboolean running;
  RestProxy *proxy;
  GConfClient *gconf;
  char *user_id;
  guint gconf_notify_id;
};

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

static void
got_diggs_cb (RestProxyCall *call,
              const GError  *error,
              GObject       *weak_object,
              gpointer       userdata)
{
  SwService *service = SW_SERVICE (weak_object);
  JsonParser *parser = NULL;
  JsonNode *root, *node;
  JsonArray *json_array;
  JsonObject *object;
  SwSet *set;
  GList *stories = NULL, *list = NULL;

  if (error) {
    g_message ("Error: %s", error->message);
    /* TODO remove this after everything is done */
    g_message ("Error: %s", rest_proxy_call_get_payload(call));
    return;
  }

  parser = json_parser_new ();
  root = node_from_call (call, parser);
  if (!root)
    return;

  /*
  stories : [
    "permalink": "http://digg.com/news/world_news/Pakistan_s_Cricketers_Donate_Bonuses_To_Floods_Relief",
    "title": "Pakistan's Cricketers Donate Bonuses To Floods Relief",
    "url": "http://www.bbc.co.uk/news/world-south-asia-11080123",
    "story_id": "20100825095640:23429676",
    "diggs": 121,
    "submiter": {
      "username": "username",
      "user_id": "12345",
      "name": "Display Name",
      "icon": null
    }
    "date_created": 1282730200,
    "thumbnails": {
      "large": "http://cdn2.diggstatic.com/story/Pakistan_s_Cricketers_Donate_Bonuses_To_Floods_Relief/l.png",
      "small": "http://cdn1.diggstatic.com/story/Pakistan_s_Cricketers_Donate_Bonuses_To_Floods_Relief/s.png",
      "medium": "http://cdn3.diggstatic.com/story/Pakistan_s_Cricketers_Donate_Bonuses_To_Floods_Relief/m.png",
      "thumb": "http://cdn2.diggstatic.com/story/Pakistan_s_Cricketers_Donate_Bonuses_To_Floods_Relief/t.png"
    }
  ]
  */

  object = json_node_get_object (root);
  if (!json_object_has_member (object, "stories"))
    return; 

  set = sw_item_set_new ();
  node = json_object_get_member (object, "stories");

  json_array = json_node_get_array (node);
  stories = json_array_get_elements (json_array);

  for (list=stories; list ;list = g_list_next (list)) {
    JsonObject *story, *thumbnails, *submiter;
    SwItem *item;
    char *str;
    time_t date;

    item = sw_item_new ();
    sw_item_set_service (item, service);

    node = (JsonNode *) list->data;
    story = json_node_get_object (node);

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

    /* the thumbnail */
    str = json_object_get_string_member (thumbnails, "large");
    sw_item_request_image_fetch (item, TRUE, "thumbnail", str);
    
    sw_set_add (set, G_OBJECT (item));
    g_object_unref (item);
  }

  if (!sw_set_is_empty (set))
    sw_service_emit_refreshed (service, set);

  g_list_free (stories);
  g_object_unref (parser);
  g_object_unref (call);
}

static void
get_diggs (SwServiceDigg *digg)
{
  SwServiceDiggPrivate *priv = GET_PRIVATE (digg);
  RestProxyCall *call;

  call = rest_proxy_new_call (priv->proxy);
  rest_proxy_call_set_function (call, "story.getTopNews");

  rest_proxy_call_add_params (call,
                              "limit", "10",
                              NULL);
  rest_proxy_call_async (call, got_diggs_cb, (GObject *)digg, NULL, NULL);
}

static void
refresh (SwService *service)
{
  SwServiceDigg *digg = SW_SERVICE_DIGG (service);
  SwServiceDiggPrivate *priv = GET_PRIVATE (digg);

  if (priv->running && priv->user_id && priv->proxy)
    get_diggs (digg);
}

static void
start (SwService *service)
{
  SwServiceDigg *digg = SW_SERVICE_DIGG (service);
  digg->priv->running = TRUE;
}

static void
user_changed_cb (GConfClient *client,
                 guint        cnxn_id,
                 GConfEntry  *entry,
                 gpointer     user_data)
{
  SwService *service = SW_SERVICE (user_data);
  SwServiceDigg *digg = SW_SERVICE_DIGG (service);
  SwServiceDiggPrivate *priv = digg->priv;
  const char *new_user;

  if (entry->value) {
    new_user = gconf_value_get_string (entry->value);
    if (new_user && new_user[0] == '\0')
      new_user = NULL;
  } else {
    new_user = NULL;
  }

  if (g_strcmp0 (new_user, priv->user_id) != 0) {
    g_free (priv->user_id);
    priv->user_id = g_strdup (new_user);

    if (priv->user_id)
      refresh (service);
    else
      sw_service_emit_refreshed (service, NULL);
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

  if (priv->gconf) {
    gconf_client_notify_remove (priv->gconf,
                                priv->gconf_notify_id);
    gconf_client_remove_dir (priv->gconf, KEY_BASE, NULL);
    g_object_unref (priv->gconf);
    priv->gconf = NULL;
  }

  G_OBJECT_CLASS (sw_service_digg_parent_class)->dispose (object);
}

static void
sw_service_digg_finalize (GObject *object)
{
  G_OBJECT_CLASS (sw_service_digg_parent_class)->finalize (object);
}

static void
sw_service_digg_class_init (SwServiceDiggClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  SwServiceClass *service_class = SW_SERVICE_CLASS (klass);

  g_type_class_add_private (klass, sizeof (SwServiceDiggPrivate));

  object_class->dispose = sw_service_digg_dispose;
  object_class->finalize = sw_service_digg_finalize;

  service_class->get_name = get_name;
  service_class->start = start;
  service_class->refresh = refresh;
}

static void
sw_service_digg_init (SwServiceDigg *self)
{
  SwServiceDiggPrivate *priv;

  priv = self->priv = GET_PRIVATE (self);

  priv->running = FALSE;

  priv->proxy = rest_proxy_new ("http://services.digg.com/2.0/", TRUE);

  /*
   * Digg webservice doesn't responses at all if no User-agent is set in requests
   * http://apidoc.digg.com/BasicConcepts#UserAgents
   */
  rest_proxy_set_user_agent (priv->proxy, "Sw");

  priv->gconf = gconf_client_get_default ();
  gconf_client_add_dir (priv->gconf, KEY_BASE,
                        GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);
  priv->gconf_notify_id = gconf_client_notify_add (priv->gconf,
                                                   KEY_USER,
                                                   user_changed_cb,
                                                   self,
                                                   NULL,
                                                   NULL);
  gconf_client_notify (priv->gconf, KEY_USER);
}
