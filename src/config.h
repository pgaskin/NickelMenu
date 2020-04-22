#ifndef NMI_CONFIG_H
#define NMI_CONFIG_H
#ifdef __cplusplus
extern "C" {
#endif

#include "menu.h"

// TODO
nmi_menu_entry_t *nmi_config_parse(size_t *n, char **err_out);

#ifdef __cplusplus
}
#endif
#endif
