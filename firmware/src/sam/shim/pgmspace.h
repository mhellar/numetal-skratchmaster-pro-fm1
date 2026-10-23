/* SAM's tables are plain const arrays here */
#ifndef PGMSPACE_SHIM_H
#define PGMSPACE_SHIM_H
#define PROGMEM
#define pgm_read_byte(p) (*(const unsigned char *)(p))
#endif
