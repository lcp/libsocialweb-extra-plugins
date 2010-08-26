#include <libsocialweb/sw-module.h>
#include "plurk.h"

const gchar *
sw_module_get_name (void)
{
  return "plurk";
}

const GType
sw_module_get_type (void)
{
  return SW_TYPE_SERVICE_PLURK;
}
