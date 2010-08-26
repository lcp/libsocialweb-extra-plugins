#ifndef _SW_SERVICE_PLURK
#define _SW_SERVICE_PLURK

#include <libsocialweb/sw-service.h>

G_BEGIN_DECLS

#define SW_TYPE_SERVICE_PLURK sw_service_plurk_get_type()

#define SW_SERVICE_PLURK(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), SW_TYPE_SERVICE_PLURK, SwServicePlurk))

#define SW_SERVICE_PLURK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), SW_TYPE_SERVICE_PLURK, SwServicePlurkClass))

#define SW_IS_SERVICE_PLURK(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SW_TYPE_SERVICE_PLURK))

#define SW_IS_SERVICE_PLURK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), SW_TYPE_SERVICE_PLURK))

#define SW_SERVICE_PLURK_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), SW_TYPE_SERVICE_PLURK, SwServicePlurkClass))

typedef struct _SwServicePlurkPrivate SwServicePlurkPrivate;

typedef struct {
  SwService parent;
  SwServicePlurkPrivate *priv;
} SwServicePlurk;

typedef struct {
  SwServiceClass parent_class;
} SwServicePlurkClass;

GType sw_service_plurk_get_type (void);

G_END_DECLS

#endif /* _SW_SERVICE_PLURK */
