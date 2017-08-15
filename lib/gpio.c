
#include "gpio.h"


//for Banana Pi
#ifdef PLATFORM_ALLWINNER_A20
#include "gpio/a20.c"
#endif

//for Orange Pi
#ifdef PLATFORM_ALLWINNER_H3
#include "gpio/h3.c"
#endif

//for Orange Pi
#ifdef PLATFORM_CORTEX_A5
#include "gpio/cortex_a5.c"
#endif


//debugging mode (for machine with no gpio pins)
#ifdef PLATFORM_ANY
#include "gpio/all.c"
#endif