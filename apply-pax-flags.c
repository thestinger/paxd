#include <stdlib.h>

#include "flags.h"

int main(void) {
    update_attributes("/etc/paxd.conf", apply);
    return EXIT_SUCCESS;
}
