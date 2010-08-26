#include <libsocialweb/sw-module.h>
#include "youtube.h"

const gchar *
sw_module_get_name (void)
{
  return "youtube";
}

const GType
sw_module_get_type (void)
{
  return SW_TYPE_SERVICE_YOUTUBE;
}
