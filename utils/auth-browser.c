/*
 * Copyright (C) 2010 Novell Inc.
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

#include <gtk/gtk.h>
#include <webkit/webkit.h>

#include "auth-browser.h"

#define DEFAULT_WINDOW_WIDTH 1024
#define DEFAULT_WINDOW_HEIGHT 600

G_DEFINE_TYPE (AuthBrowser, auth_browser, G_TYPE_OBJECT)

#define GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), AUTH_TYPE_BROWSER, AuthBrowserPrivate))

typedef struct _AuthBrowserPrivate AuthBrowserPrivate;

struct _AuthBrowserPrivate {
  GtkWidget *window;
  WebKitWebView *webview;
  char *title;
  int progress;
  char *stop_url;
  void (*stop_handler) (const char *url);
};

static void
update_title (GtkWindow   *window,
              AuthBrowser *browser)
{
  AuthBrowserPrivate *priv = GET_PRIVATE (browser);
  char *title = NULL;

  if (priv->progress < 100) {
    title = g_strdup_printf ("%s (%d%%)", priv->title, priv->progress);
  } else {
    title = g_strdup (priv->title);
  }

  if (title) {
    gtk_window_set_title (GTK_WINDOW (priv->window), title);
    g_free (title);
  }
}

static void
title_change_cb (WebKitWebView  *page,
                 WebKitWebFrame *web_frame,
                 const gchar    *title,
                 gpointer        data)
{
  AuthBrowser *browser = AUTH_BROWSER (data);
  AuthBrowserPrivate *priv = GET_PRIVATE (browser);

  g_free (priv->title);
  priv->title = g_strdup (title);

  update_title (GTK_WINDOW (priv->window), browser);
}

static void
progress_change_cb (WebKitWebView *page,
                    gint           progress,
                    gpointer       data)
{
  AuthBrowser *browser = AUTH_BROWSER (data);
  AuthBrowserPrivate *priv = GET_PRIVATE (browser);

  priv->progress = progress;
  update_title (GTK_WINDOW (priv->window), browser);
}

static void
load_commit_cb (WebKitWebView  *page,
                WebKitWebFrame *frame,
                gpointer        data)
{
  AuthBrowser *browser = AUTH_BROWSER (data);
  AuthBrowserPrivate *priv = GET_PRIVATE (browser);

  const gchar *uri = webkit_web_frame_get_uri(frame);

g_print ("%s\n", uri);

  if (!priv->stop_url)
    return;

  if (g_strrstr (uri, priv->stop_url)) {
    webkit_web_view_stop_loading (page);
    gtk_widget_hide (GTK_WIDGET (priv->window));
    if (priv->stop_handler)
      priv->stop_handler (uri);
  }
}

/* Show the authentication browser */
void
auth_browser_open_url (AuthBrowser *browser,
                       const char  *url,
                       const char  *stop_url,
                       void         (*stop_handler)(const char *url))
{
  AuthBrowserPrivate *priv = GET_PRIVATE (browser);

  g_free (priv->stop_url);
  priv->stop_url = g_strdup (stop_url);
  if (stop_handler)
    priv->stop_handler = stop_handler;

  webkit_web_view_open (priv->webview, url);

  gtk_widget_grab_focus (GTK_WIDGET (priv->webview));
  gtk_widget_show_all (priv->window);
}

static void
auth_browser_dispose (GObject *object)
{
  AuthBrowser *browser = AUTH_BROWSER (object);
  AuthBrowserPrivate *priv = GET_PRIVATE (browser);

  if (priv->window)
    g_object_unref (priv->window);

  if (priv->webview)
    g_object_unref (priv->webview);

  G_OBJECT_CLASS (auth_browser_parent_class)->dispose (object);
}

static void
auth_browser_finalize (GObject *object)
{
  AuthBrowser *browser = AUTH_BROWSER (object);
  AuthBrowserPrivate *priv = GET_PRIVATE (browser);

  g_free (priv->title);
  g_free (priv->stop_url);

  G_OBJECT_CLASS (auth_browser_parent_class)->finalize (object);
}

static void
auth_browser_class_init (AuthBrowserClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (AuthBrowserPrivate));

  object_class->dispose = auth_browser_dispose;
  object_class->finalize = auth_browser_finalize;
}

static void
auth_browser_init (AuthBrowser *self)
{
  AuthBrowserPrivate *priv = GET_PRIVATE (self);
  GtkWidget *vbox, *scrolled_window;

  priv->title = NULL;
  priv->progress = 0;
  priv->stop_url = NULL;
  priv->stop_handler = NULL;

  vbox = gtk_vbox_new (FALSE, 0);
  scrolled_window = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  priv->webview = WEBKIT_WEB_VIEW (webkit_web_view_new ());
  gtk_container_add (GTK_CONTAINER (scrolled_window), GTK_WIDGET (priv->webview));

  gtk_box_pack_start (GTK_BOX (vbox), scrolled_window, TRUE, TRUE, 0);

  priv->window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_default_size (GTK_WINDOW (priv->window), DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT);
  gtk_widget_set_name (priv->window, "AuthBrowser");
  gtk_window_set_screen (GTK_WINDOW (priv->window), gdk_screen_get_default ());
  //gtk_window_set_modal (window, TRUE);
 
  gtk_container_add (GTK_CONTAINER (priv->window), vbox);

  g_signal_connect (G_OBJECT (priv->webview), "title-changed", G_CALLBACK (title_change_cb), self);
  g_signal_connect (G_OBJECT (priv->webview), "load-progress-changed", G_CALLBACK (progress_change_cb), self);
  g_signal_connect (G_OBJECT (priv->webview), "load-committed", G_CALLBACK (load_commit_cb), self);
}

AuthBrowser*
auth_browser_new (void)
{
  return g_object_new (AUTH_TYPE_BROWSER, NULL);
}

