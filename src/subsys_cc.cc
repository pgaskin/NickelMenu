#include <dlfcn.h>

#include "subsys_cc.h"
#include "util.h"

extern "C" int nmi_subsys_nickelsetting(const char *arg, char **err_out) {
    #define NMI_ERR_RET 1
    NMI_RETURN_ERR("not implemented yet"); // TODO
    #undef NMI_ERR_RET
}
