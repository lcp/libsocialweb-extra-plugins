#include <libsocialweb/sw-module.h>
#include "sina.h"

const gchar *
sw_module_get_name (void)
{
  return "sina";
}

const GType
sw_module_get_type (void)
{
  return SW_TYPE_SERVICE_SINA;
}
