#ifndef AMALGAM_ASM_KYANITE_MMU
#define AMALGAM_ASM_KYANITE_MMU

#include "main.h"

/* Complex Mode paging for the Kyanite architecture (depicted in big-engian):
 *
 * Page dir	|1-bit|2-bit|1-bit|1-bit|1-bit|6-bit|20-bit|
 * 		|AVAIL|W/E  |RING |RSVD |0    |RSVD |ADDR  |
 *                                            
 * Page supreme	|1-bit|2-bit|1-bit|1-bit|1-bit|16-bit|10-bit|
 * 		|AVAIL|W/E  |RING |IO   |1    |RSVD  |ADDR  |
 *
 * Page file	|1-bit|2-bit|1-bit|1-bit|7-bit|20-bit|
 * 		|AVAIL|W/E  |RING |IO   |RSVD |ADDR  |
 *
 * Address	|12-bit|10-bit|10-bit|
 * 		|OFFSET|FILE  |DIR   |
 * */


#endif
