#ifndef _SW_PLURK_ITEM_VIEW
#define _SW_PLURK_ITEM_VIEW

#include <glib-object.h>
#include <libsocialweb/sw-item-view.h>

G_BEGIN_DECLS

#define SW_TYPE_PLURK_ITEM_VIEW sw_plurk_item_view_get_type()

#define SW_PLURK_ITEM_VIEW(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), SW_TYPE_PLURK_ITEM_VIEW, SwPlurkItemView))

#define SW_PLURK_ITEM_VIEW_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), SW_TYPE_PLURK_ITEM_VIEW, SwPlurkItemViewClass))

#define SW_IS_PLURK_ITEM_VIEW(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SW_TYPE_PLURK_ITEM_VIEW))

#define SW_IS_PLURK_ITEM_VIEW_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), SW_TYPE_PLURK_ITEM_VIEW))

#define SW_PLURK_ITEM_VIEW_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), SW_TYPE_PLURK_ITEM_VIEW, SwPlurkItemViewClass))

typedef struct {
  SwItemView parent;
} SwPlurkItemView;

typedef struct {
  SwItemViewClass parent_class;
} SwPlurkItemViewClass;

GType sw_plurk_item_view_get_type (void);

G_END_DECLS

#endif /* _SW_PLURK_ITEM_VIEW */

