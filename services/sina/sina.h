#ifndef _SW_SERVICE_SINA
#define _SW_SERVICE_SINA

#include <libsocialweb/sw-service.h>

G_BEGIN_DECLS

#define SW_TYPE_SERVICE_SINA sw_service_sina_get_type()

#define SW_SERVICE_SINA(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), SW_TYPE_SERVICE_SINA, SwServiceSina))

#define SW_SERVICE_SINA_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), SW_TYPE_SERVICE_SINA, SwServiceSinaClass))

#define SW_IS_SERVICE_SINA(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SW_TYPE_SERVICE_SINA))

#define SW_IS_SERVICE_SINA_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), SW_TYPE_SERVICE_SINA))

#define SW_SERVICE_SINA_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), SW_TYPE_SERVICE_SINA, SwServiceSinaClass))

typedef struct _SwServiceSinaPrivate SwServiceSinaPrivate;

typedef struct {
  SwService parent;
  SwServiceSinaPrivate *priv;
} SwServiceSina;

typedef struct {
  SwServiceClass parent_class;
} SwServiceSinaClass;

GType sw_service_sina_get_type (void);

G_END_DECLS

#endif /* _SW_SERVICE_SINA */
