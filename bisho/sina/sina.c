/*
 * (Branched from bisho oauth pane)
 * Copyright (C) 2010 Novell, Inc.
 *
 * Author: Gary Ching-Pang Lin <glin@novell.com>
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <config.h>
#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>
#include <gnome-keyring.h>
#include <libsoup/soup.h>
#include <rest/oauth-proxy.h>
#include <rest/oauth-proxy-call.h>
#include <libsocialweb-keystore/sw-keystore.h>
#include <bisho/service-info.h>
#include "sina.h"

/* TODO: use sw-keyring */
static const GnomeKeyringPasswordSchema oauth_schema = {
  GNOME_KEYRING_ITEM_GENERIC_SECRET,
  {
    { "server", GNOME_KEYRING_ATTRIBUTE_TYPE_STRING },
    { "consumer-key", GNOME_KEYRING_ATTRIBUTE_TYPE_STRING },
    { NULL, 0 }
  }
};

struct _BishoPaneSinaPrivate {
  const char *consumer_key;
  const char *consumer_secret;
  char *base_url;
  char *request_token_function;
  char *authorize_function;
  char *access_token_function;
  char *callback;
  RestProxy *proxy;
  GtkWidget *pin_label;
  GtkWidget *pin_entry;
  GtkWidget *button;
};

typedef enum {
  LOGGED_OUT,
  WORKING,
  CONTINUE_AUTH_10,
  CONTINUE_AUTH_10a,
  LOGGED_IN,
} ButtonState;

#define GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), BISHO_TYPE_PANE_SINA, BishoPaneSinaPrivate))
G_DEFINE_DYNAMIC_TYPE (BishoPaneSina, bisho_pane_sina, BISHO_TYPE_PANE);

static void update_widgets (BishoPaneSina *pane, ButtonState state);

static char *
create_url (BishoPaneSina *pane, const char *token)
{
  SoupURI *base, *uri;
  char *s;

  g_assert (pane);
  g_assert (token);

  base = soup_uri_new (pane->priv->base_url);
  uri = soup_uri_new_with_base (base, pane->priv->authorize_function);
  soup_uri_free (base);

  soup_uri_set_query_from_fields (uri,
                                  "oauth_token", token,
                                  "oauth_callback", pane->priv->callback ?: "",
                                  NULL);

  s = soup_uri_to_string (uri, FALSE);
  soup_uri_free (uri);
  return s;
}

G_GNUC_UNUSED static const char * unused_for_now[] = {
  N_("You don't seem to have a network connection, this won't work."),
  N_("You could check that the computer's clock is correct."),
  N_("You could try again.")
};

static void
request_token_cb (OAuthProxy   *proxy,
                  const GError *error,
                  GObject      *weak_object,
                  gpointer      user_data)
{
  BishoPaneSina *pane = BISHO_PANE_SINA (user_data);
  BishoPaneSinaPrivate *priv = pane->priv;
  ServiceInfo *info = BISHO_PANE (pane)->info;
  char *url;

  if (error) {
    update_widgets (pane, LOGGED_OUT);

    g_message ("Error from %s: %s", info->name, error->message);
    bisho_pane_set_banner_error (BISHO_PANE (pane), error);
    return;
  }

  url = create_url (pane, oauth_proxy_get_token (OAUTH_PROXY (priv->proxy)));
  gtk_show_uri (gtk_widget_get_screen (GTK_WIDGET (pane)), url, GDK_CURRENT_TIME, NULL);

  if (priv->callback == NULL) {
    update_widgets (pane, CONTINUE_AUTH_10);
  } else {
    /* TODO: insert check for 1.0a? */
    if (strcmp (priv->callback, "oob") == 0) {
      update_widgets (pane, CONTINUE_AUTH_10a);
    } else {
      update_widgets (pane, CONTINUE_AUTH_10);
      /* TODO: should be
         update_widgets (pane, WORKING);
         but myspace breaks this at the moment */
    }
  }
}

static void
log_in_clicked (GtkWidget *button, gpointer user_data)
{
  BishoPaneSina *pane = BISHO_PANE_SINA (user_data);
  BishoPaneSinaPrivate *priv = pane->priv;
  ServiceInfo *info = BISHO_PANE (pane)->info;
  GError *error = NULL;

  if (oauth_proxy_request_token_async (OAUTH_PROXY (priv->proxy),
                                       priv->request_token_function,
                                       priv->callback,
                                       request_token_cb,
                                       NULL,
                                       pane,
                                       &error)) {
    update_widgets (pane, WORKING);
  } else {
    update_widgets (pane, LOGGED_OUT);

    g_message ("Error from %s: %s", info->name, error->message);
    bisho_pane_set_banner_error (BISHO_PANE (pane), error);
    g_error_free (error);
    return;
  }
}


static void
delete_done_cb (GnomeKeyringResult result, gpointer user_data)
{
  BishoPaneSina *pane = BISHO_PANE_SINA (user_data);
  SwClientService *service;

  if (result == GNOME_KEYRING_RESULT_OK){
    update_widgets (pane, LOGGED_OUT);
    service = sw_client_get_service (BISHO_PANE (pane)->socialweb, BISHO_PANE (pane)->info->name);
    sw_client_service_credentials_updated (service);
  } else {
    update_widgets (pane, LOGGED_IN);
  }
}

static void
log_out_clicked (GtkButton *button, gpointer user_data)
{
  BishoPaneSina *pane = BISHO_PANE_SINA (user_data);
  BishoPaneSinaPrivate *priv = pane->priv;

  update_widgets (pane, WORKING);

  gnome_keyring_delete_password (&oauth_schema, delete_done_cb, user_data, NULL,
                                 "server", priv->base_url,
                                 "consumer-key", priv->consumer_key,
                                 NULL);
}

static char *
encode_tokens (const char *token, const char *secret)
{
  char *encoded_token, *encoded_secret;
  char *string;

  g_assert (token);
  g_assert (secret);

  encoded_token = g_base64_encode ((guchar*)token, strlen (token));
  encoded_secret = g_base64_encode ((guchar*)secret, strlen (secret));

  string = g_strconcat (encoded_token, " ", encoded_secret, NULL);

  g_free (encoded_token);
  g_free (encoded_secret);

  return string;
}

static void
access_token_cb (OAuthProxy   *proxy,
                 const GError *error,
                 GObject      *weak_object,
                 gpointer      user_data)
{
  BishoPaneSina *pane = BISHO_PANE_SINA (user_data);
  ServiceInfo *info = BISHO_PANE (pane)->info;
  BishoPaneSinaPrivate *priv = pane->priv;
  char *encoded;
  SwClientService *service;

  if (error) {
    update_widgets (pane, LOGGED_OUT);
    g_message ("Error from %s: %s", info->name, error->message);
    bisho_pane_set_banner_error (BISHO_PANE (pane), error);
    return;
  }

  encoded = encode_tokens
    (oauth_proxy_get_token (OAUTH_PROXY (priv->proxy)),
     oauth_proxy_get_token_secret (OAUTH_PROXY (priv->proxy)));

  /* TODO async */
  GnomeKeyringResult result;
  GnomeKeyringAttributeList *attrs;
  guint32 id;
  attrs = gnome_keyring_attribute_list_new ();
  gnome_keyring_attribute_list_append_string (attrs, "server", priv->base_url);
  gnome_keyring_attribute_list_append_string (attrs, "consumer-key", priv->consumer_key);

  result = gnome_keyring_item_create_sync (NULL,
                                           GNOME_KEYRING_ITEM_GENERIC_SECRET,
                                           info->display_name,
                                           attrs, encoded,
                                           TRUE, &id);

  if (result == GNOME_KEYRING_RESULT_OK) {
    gnome_keyring_item_grant_access_rights_sync (NULL,
                                                 "libsocialweb",
                                                 LIBEXECDIR "/libsocialweb-core",
                                                 id, GNOME_KEYRING_ACCESS_READ);
    update_widgets (pane, LOGGED_IN);
    service = sw_client_get_service (BISHO_PANE (pane)->socialweb, info->name);
    sw_client_service_credentials_updated (service);
  } else {
    g_message ("Cannot update keyring: %s", gnome_keyring_result_to_message (result));
    update_widgets (pane, LOGGED_OUT);
  }
}

static void
bisho_pane_sina_continue_auth (BishoPane *_pane, GHashTable *params)
{
  BishoPaneSina *pane = BISHO_PANE_SINA (_pane);
  BishoPaneSinaPrivate *priv = pane->priv;
  ServiceInfo *info = BISHO_PANE (pane)->info;
  GError *error = NULL;
  const char *verifier;

  /* TODO: check the current state */
  /* TODO: handle the arguments */

  /*
   * If the server is using 1.0a then we need to provide a verifier.  If the
   * callback is "oob" then we need to ask for the verifier, otherwise it's in
   * the parameters we've been passed.
   */
  /* 
   * Sina's OAuth 1.0a is buggy and it never sets oauth_callback_confirmed to true
   * so there is no way to tell whether Sina uses OAuth 1.0 or OAuth 1.0a.
   */
  if (strcmp (priv->callback, "oob") == 0) {
    verifier = gtk_entry_get_text (GTK_ENTRY (priv->pin_entry));
    gtk_widget_hide (priv->pin_label);
    gtk_widget_hide (priv->pin_entry);
  } else {
    verifier = g_hash_table_lookup (params, "oauth_verifier");
  }

  if (oauth_proxy_access_token_async (OAUTH_PROXY (priv->proxy),
                                      priv->access_token_function,
                                      verifier,
                                      access_token_cb,
                                      NULL,
                                      pane,
                                      &error)) {
    update_widgets (pane, WORKING);
  } else {
    update_widgets (pane, LOGGED_OUT);
    g_message ("Error from %s: %s", info->name, error->message);
    bisho_pane_set_banner_error (BISHO_PANE (pane), error);
    return;
  }
}

static void
continue_clicked (GtkWidget *button, gpointer user_data)
{
  bisho_pane_sina_continue_auth (BISHO_PANE (user_data), NULL);
}

static void
update_widgets (BishoPaneSina *pane, ButtonState state)
{
  BishoPaneSinaPrivate *priv;
  ServiceInfo *info;

  g_assert (BISHO_IS_PANE_SINA (pane));
  priv = pane->priv;
  info = BISHO_PANE (pane)->info;

  g_signal_handlers_disconnect_by_func (priv->button, log_out_clicked, pane);
  g_signal_handlers_disconnect_by_func (priv->button, continue_clicked, pane);
  g_signal_handlers_disconnect_by_func (priv->button, log_in_clicked, pane);

  switch (state) {
  case LOGGED_OUT:
    bisho_pane_set_banner (BISHO_PANE (pane), NULL);
    gtk_widget_show (priv->button);
    gtk_button_set_label (GTK_BUTTON (priv->button), _("Log me in"));
    g_signal_connect (priv->button, "clicked", G_CALLBACK (log_in_clicked), pane);
    break;
  case WORKING:
    bisho_pane_set_banner (BISHO_PANE (pane), _("Connecting..."));
    gtk_widget_hide (priv->button);
    break;
  case CONTINUE_AUTH_10:
    {
      char *s;

      gtk_widget_show (priv->button);

      s = g_strdup_printf (_("Once you have logged in to %s, press Continue."),
                           info->display_name);
      bisho_pane_set_banner (BISHO_PANE (pane), s);
      g_free (s);

      gtk_button_set_label (GTK_BUTTON (priv->button), _("Continue"));
      g_signal_connect (priv->button, "clicked", G_CALLBACK (continue_clicked), pane);
    }
    break;
  case CONTINUE_AUTH_10a:
    {
      char *s;

      gtk_widget_show (priv->pin_label);
      gtk_widget_show (priv->pin_entry);
      gtk_widget_show (priv->button);

      s = g_strdup_printf (_("Once you have logged in to %s, enter the code they give you and press Continue."),
                           info->display_name);
      bisho_pane_set_banner (BISHO_PANE (pane), s);
      g_free (s);

      gtk_button_set_label (GTK_BUTTON (priv->button), _("Continue"));
      g_signal_connect (priv->button, "clicked", G_CALLBACK (continue_clicked), pane);
    }
    break;
  case LOGGED_IN:
    bisho_pane_set_banner (BISHO_PANE (pane), _("Log in succeeded. You'll see new items in a couple of minutes."));
    gtk_widget_show (priv->button);
    gtk_button_set_label (GTK_BUTTON (priv->button), _("Log me out"));
    g_signal_connect (priv->button, "clicked", G_CALLBACK (log_out_clicked), pane);
    break;
  }
}

static void
find_key_cb (GnomeKeyringResult result,
             const char *string,
             gpointer user_data)
{
  BishoPaneSina *pane = BISHO_PANE_SINA (user_data);

  if (result == GNOME_KEYRING_RESULT_OK)
    update_widgets (pane, LOGGED_IN);
  else
    update_widgets (pane, LOGGED_OUT);
}

static const char *
bisho_pane_sina_get_auth_type (BishoPaneClass *klass)
{
  return "sina";
}

static void
bisho_pane_sina_constructed (GObject *object)
{
  BishoPaneSina *pane = BISHO_PANE_SINA (object);
  BishoPaneSinaPrivate *priv = pane->priv;
  ServiceInfo *info = BISHO_PANE (pane)->info;

  priv->base_url = "http://api.t.sina.com.cn/";
  priv->request_token_function = "oauth/request_token";
  priv->authorize_function = "oauth/authorize";
  priv->access_token_function = "oauth/access_token";
  priv->callback = "oob";

  bisho_pane_follow_connected (BISHO_PANE (pane), priv->button);

  /* TODO: use GInitable */
  if (!sw_keystore_get_key_secret (info->name,
                                       &priv->consumer_key,
                                       &priv->consumer_secret)) {
    return;
  }

  priv->proxy = oauth_proxy_new (priv->consumer_key,
                                priv->consumer_secret,
                                priv->base_url, FALSE);
  rest_proxy_set_user_agent (priv->proxy, "Bisho/" VERSION);

  update_widgets (pane, WORKING);

  gnome_keyring_find_password (&oauth_schema, find_key_cb, pane, NULL,
                               "server", priv->base_url,
                               "consumer-key", priv->consumer_key,
                               NULL);
}

static void
bisho_pane_sina_class_init (BishoPaneSinaClass *klass)
{
  GObjectClass *o_class = G_OBJECT_CLASS (klass);
  BishoPaneClass *pane_class = BISHO_PANE_CLASS (klass);

  o_class->constructed = bisho_pane_sina_constructed;
  pane_class->get_auth_type = bisho_pane_sina_get_auth_type;
  pane_class->continue_auth = bisho_pane_sina_continue_auth;

  g_type_class_add_private (klass, sizeof (BishoPaneSinaPrivate));
}

static void
bisho_pane_sina_class_finalize (BishoPaneSinaClass *klass)
{
}

static void
bisho_pane_sina_init (BishoPaneSina *pane)
{
  BishoPaneSinaPrivate *priv;
  GtkWidget *content, *align, *box;

  pane->priv = GET_PRIVATE (pane);
  priv = pane->priv;

  content = BISHO_PANE (pane)->content;

  align = gtk_alignment_new (0.5, 0.5, 0.0, 0.0);
  gtk_widget_show (align);
  gtk_container_add (GTK_CONTAINER (content), align);

  box = gtk_hbox_new (FALSE, 8);
  gtk_widget_show (box);
  gtk_container_add (GTK_CONTAINER (align), box);

  priv->pin_label = gtk_label_new (_("Code:"));
  gtk_box_pack_start (GTK_BOX (box), priv->pin_label, FALSE, FALSE, 0);

  priv->pin_entry = gtk_entry_new ();
  gtk_box_pack_start (GTK_BOX (box), priv->pin_entry, FALSE, FALSE, 0);

  priv->button = gtk_button_new ();
  gtk_widget_show (priv->button);
  gtk_box_pack_start (GTK_BOX (box), priv->button, FALSE, FALSE, 0);
}

void
bisho_module_load (GTypeModule *module)
{
  bisho_pane_sina_register_type (module);
}
