#ifndef SPACEMAP_H
#define SPACEMAP_H

#include "key_generator.h"

uint8_t* spacemap_create();
void spacemap_destroy(uint8_t* map);
const int* spacemap_get_offsets();
int spacemap_get_key(Point pt);

#endif
