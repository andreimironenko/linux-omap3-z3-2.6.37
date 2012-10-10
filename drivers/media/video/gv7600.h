#ifndef _GV7600_H
#define _GV7600_H


#ifdef __KERNEL__
#include <linux/spi/spi.h>
#include <linux/device.h>
#include <linux/videodev.h>
#include <linux/videodev2.h>
#endif				/* __KERNEL__ */


#define GV7600_NUM_CHANNELS  1

#define GV7600_MAX_NO_CONTROLS  0
#define GV7600_MAX_NO_INPUTS    1
#define GV7600_MAX_NO_STANDARDS 11

#endif
