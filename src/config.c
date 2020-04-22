#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "menu.h"
#include "util.h"
#include "subsys_c.h"
#include "subsys_cc.h"

// TODO: actually parse it

nmi_menu_entry_t *nmi_config_parse(size_t *n, char **err_out) {
    #define NMI_ERR_RET NULL
    NMI_ASSERT(n, "required argument is null");
    nmi_menu_entry_t *me = calloc(5, sizeof(nmi_menu_entry_t));

    me[0].loc = NMI_MENU_LOCATION_MAIN_MENU;
    me[0].lbl = strdup("Test Log (main)");
    me[0].arg = strdup("It worked!");
    me[0].execute = nmi_subsys_dbgsyslog;

    me[1].loc = NMI_MENU_LOCATION_READER_MENU;
    me[1].lbl = strdup("Test Log (reader)");
    me[1].arg = strdup("It worked!");
    me[1].execute = nmi_subsys_dbgsyslog;

    me[2].loc = NMI_MENU_LOCATION_MAIN_MENU;
    me[2].lbl = strdup("Test Error (main)");
    me[2].arg = strdup("It's a fake error!");
    me[2].execute = nmi_subsys_dbgerror;

    me[3].loc = NMI_MENU_LOCATION_READER_MENU;
    me[3].lbl = strdup("Test Error (reader)");
    me[3].arg = strdup("It's a fake error!");
    me[3].execute = nmi_subsys_dbgerror;

    me[4].loc = NMI_MENU_LOCATION_READER_MENU;
    me[4].lbl = strdup("Invert Screen");
    me[4].arg = "invert";
    me[4].execute = nmi_subsys_nickelsetting;

    *n = 5;
    NMI_RETURN_OK(me);
    #undef NMI_ERR_RET
}

