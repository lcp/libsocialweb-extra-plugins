#ifndef _SW_SINA_ITEM_VIEW
#define _SW_SINA_ITEM_VIEW

#include <glib-object.h>
#include <libsocialweb/sw-item-view.h>

G_BEGIN_DECLS

#define SW_TYPE_SINA_ITEM_VIEW sw_sina_item_view_get_type()

#define SW_SINA_ITEM_VIEW(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), SW_TYPE_SINA_ITEM_VIEW, SwSinaItemView))

#define SW_SINA_ITEM_VIEW_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), SW_TYPE_SINA_ITEM_VIEW, SwSinaItemViewClass))

#define SW_IS_SINA_ITEM_VIEW(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SW_TYPE_SINA_ITEM_VIEW))

#define SW_IS_SINA_ITEM_VIEW_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), SW_TYPE_SINA_ITEM_VIEW))

#define SW_SINA_ITEM_VIEW_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), SW_TYPE_SINA_ITEM_VIEW, SwSinaItemViewClass))

typedef struct {
  SwItemView parent;
} SwSinaItemView;

typedef struct {
  SwItemViewClass parent_class;
} SwSinaItemViewClass;

GType sw_sina_item_view_get_type (void);

G_END_DECLS

#endif /* _SW_SINA_ITEM_VIEW */

