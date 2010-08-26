#ifndef _SW_SERVICE_YOUTUBE
#define _SW_SERVICE_YOUTUBE

#include <libsocialweb/sw-service.h>

G_BEGIN_DECLS

#define SW_TYPE_SERVICE_YOUTUBE sw_service_youtube_get_type()

#define SW_SERVICE_YOUTUBE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), SW_TYPE_SERVICE_YOUTUBE, SwServiceYoutube))

#define SW_SERVICE_YOUTUBE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), SW_TYPE_SERVICE_YOUTUBE, SwServiceYoutubeClass))

#define SW_IS_SERVICE_YOUTUBE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SW_TYPE_SERVICE_YOUTUBE))

#define SW_IS_SERVICE_YOUTUBE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), SW_TYPE_SERVICE_YOUTUBE))

#define SW_SERVICE_YOUTUBE_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), SW_TYPE_SERVICE_YOUTUBE, SwServiceYoutubeClass))

typedef struct _SwServiceYoutubePrivate SwServiceYoutubePrivate;

typedef struct {
  SwService parent;
  SwServiceYoutubePrivate *priv;
} SwServiceYoutube;

typedef struct {
  SwServiceClass parent_class;
} SwServiceYoutubeClass;

GType sw_service_youtube_get_type (void);

G_END_DECLS

#endif /* _SW_SERVICE_YOUTUBE */
