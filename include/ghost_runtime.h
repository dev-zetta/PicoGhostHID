#ifndef GHOST_RUNTIME_H_
#define GHOST_RUNTIME_H_

#include <stdint.h>

#include "usb_descriptors.h"

void ghost_handle_feature_report(uint8_t const *buffer, uint16_t bufsize);
uint16_t ghost_fill_feature_report(uint8_t *buffer, uint16_t reqlen);

#endif
