/*
 * Copyright (C) 2007-2009 Texas Instruments Inc
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 */
#ifndef _ADV9880_H_
#define _ADV9880_H_

#ifdef __KERNEL__
#include <linux/i2c.h>
#include <linux/device.h>
#include <linux/videodev.h>
#include <linux/videodev2.h>
#endif				/* __KERNEL__ */

#include <media/davinci/videohd.h> // For HD std (V4L2_STD_1080I, etc)

#define V4L2_STD_ADV9880_ALL        (V4L2_STD_720P_60 | \
					V4L2_STD_720P_50 | \
					V4L2_STD_1080I_60 | \
					V4L2_STD_1080I_50 | \
					V4L2_STD_1080P_24 | \
					V4L2_STD_1080P_25 | \
					V4L2_STD_1080P_30 | \
					V4L2_STD_1080P_50 | \
					V4L2_STD_1080P_60 | \
					V4L2_STD_525P_60 | \
					V4L2_STD_625P_50 | \
                                        V4L2_STD_525_60 | \
                                        V4L2_STD_625_50 \
				    )

/* enum */
enum adv9880_mode {
	ADV9880_MODE_480i_30FPS = 0,
	ADV9880_MODE_576i_25FPS,
	ADV9880_MODE_480p_30FPS,
	ADV9880_MODE_576p_25FPS,
	ADV9880_MODE_720p_30FPS,
	ADV9880_MODE_1080i_30FPS,
	ADV9880_MODE_1080p_30FPS,
	ADV9880_MODE_1080i_25FPS
};

enum adv9880_vco_gain {
	VCO_GAIN_ULTRA_LOW = 0,
	VCO_GAIN_LOW,
	VCO_GAIN_MEDIUM,
	VCO_GAIN_HIGH
};

enum adv9880_cp_current {
	CP_CURRENT_SMALL,
	CP_CURRENT_DEFAULT,
	CP_CURRENT_LARGE
};

/*structures*/

struct adv9880_format_params {
	unsigned char hpll_divider_msb;
	unsigned char hpll_divider_lsb;
	enum adv9880_vco_gain hpll_vco_control;
	enum adv9880_cp_current hpll_cp_current;
	unsigned char hpll_phase_select;
//	enum adv9880_post_divider hpll_post_divider;
	unsigned char hpll_control;

	unsigned char avid_start_msb;
	unsigned char avid_start_lsb;
	unsigned char avid_stop_msb;
	unsigned char avid_stop_lsb;
	unsigned char vblk_start_f0_line_offset;
	unsigned char vblk_start_f1_line_offset;
	unsigned char vblk_f0_duration;
	unsigned char vblk_f1_duration;

	unsigned char clamp_start, clamp_width;
	unsigned char hpll_pre_coast;
	unsigned char hpll_post_coast;
	unsigned char reserved;

        unsigned char screen_height_msb;
        unsigned char screen_height_lsb;
        unsigned char sync_filter_control;
        unsigned char vsync_duration;
};

struct adv9880_offset {
	unsigned char blue_fine_offset;
	unsigned char green_fine_offset;
	unsigned char red_fine_offset;
	unsigned char blue_fine_offset_lsb;
	unsigned char green_fine_offset_lsb;
	unsigned char red_fine_offset_lsb;
	unsigned char blue_coarse_offset;
	unsigned char green_coarse_offset;
	unsigned char red_coarse_offset;
};

struct adv9880_gain {
	unsigned char blue_fine_gain;
	unsigned char green_fine_gain;
	unsigned char red_fine_gain;
	unsigned char blue_coarse_gain;
	unsigned char green_coarse_gain;
	unsigned char red_coarse_gain;
};

struct adv9880_params {
	v4l2_std_id std;
	int inputidx;
	struct adv9880_gain gain;
	struct adv9880_offset offset;
	struct adv9880_format_params format;
};

#ifdef __KERNEL__

#define ADV9880_NUM_CHANNELS                    1

/* Macros */
#define ADV9880_LINES_720_LOWER       0x2Ea
#define ADV9880_LINES_720_UPPER       0x2EE

#define ADV9880_LINES_1080P_LOWER   0x462
#define ADV9880_LINES_1080P_UPPER   0x465

#define ADV9880_LINES_1080I_60_LOWER   0x232
#define ADV9880_LINES_1080I_60_UPPER   0x233

#define GENERATE_MASK(bits, pos) ((((0xFFFFFFFF) << (32-bits)) >> \
		(32-bits)) << pos)

/* Defines for input supported */
#define ADV9880_ANALOG_INPUT			ADV9880_INPUT_SELECT_ANALOG
#define ADV9880_DIGITAL_INPUT			ADV9880_INPUT_SELECT_DIGITAL

/* Macros for default register values */

#define ADV9880_HPLL_MSB_DEFAULT                0x89
#define ADV9880_HPLL_LSB_DEFAULT                0x80
#define ADV9880_HPLL_CONTROL_DEFAULT            0xA0
#define ADV9880_HPLL_PHASE_SEL_DEFAULT          0x80
#define ADV9880_CLAMP_START_DEFAULT             0x32
#define ADV9880_CLAMP_WIDTH_DEFAULT             0x20
#define ADV9880_SYNC_SEP_THLD_DEFAULT           0x20
#define ADV9880_HPLL_PRE_COAST_DEFAULT          0x01
#define ADV9880_HPLL_POST_COAST_DEFAULT         0x00


#define ADV9880_OUTPUT_MODE_CONTROL_DEFAULT     0x76 /* 4:2:2, primary output enabled, secondary output disabled, 
                                                        high drive strength, 1x CLK */

#define ADV9880_SYNC_FILTER_CONTROL_DEFAULT     0xec /* Enable HSYNC filter and VSYNC filter */
#define ADV9880_SYNC_FILTER_CONTROL_VSYNC_DURATION	0x10;


#define	ADV9880_VSYNC_DURATION_DEFAULT			0x4	/* Default VSYNC duration */

#define ADV9880_CLAMP_CONTROL_DEFAULT           0xa2 /* Pr and Pb mid-level clamp */

#define ADV9880_AUTO_OFFSET_CONTROL_DEFAULT     0xCE /* Enable auto-offset control */

#define ADV9880_GREEN_OFFSET_DEFAULT            0x04 /* Auto-offset target for luminance */

#define ADV9880_MISC_CONTROL_1_DEFAULT          0x88 /* BT.656 disabled */
//#define ADV9880_MISC_CONTROL_1_DEFAULT          0x98 /* BT.656 enabled */

//#define ADV9880_I2S_CONTROL_DEFAULT             0x10 /* I2S mode */
#define ADV9880_I2S_CONTROL_DEFAULT             0x30 /* right-justified mode */
//#define ADV9880_I2S_CONTROL_DEFAULT             0x50 /* left-justified mode */

//#define ADV9880_OUTPUT_SYNC_CONTROL_DEFAULT     0xee /* Hsync active high, Vsync active high, field ID active*/
//#define ADV9880_OUTPUT_SYNC_CONTROL_DEFAULT     0x3e /* Hsync active low, Vsync active low, field ID active high, DE active high, clock not inverted */
#define ADV9880_OUTPUT_SYNC_CONTROL_DEFAULT     0x3f /* Hsync active low, Vsync active low, field ID active high, DE active high, clock inverted */

#define ADV9880_INPUT_SELECT_DEFAULT            0x03 /* Manual select, HDMI */

#define ADV9880_HSYNC_DURATION_DEFAULT          0x30 /* Allow polarity switch to account for component input delay */

/* Macros for horizontal PLL */
#define FEEDBACK_DIVIDER_MSB_720p               0x67
#define FEEDBACK_DIVIDER_LSB_720p               0x02
#define VCO_CONTROL_720p                        0x02
#define CP_CURRENT_720p                         0x04
#define PHASE_SELECT_720p                       0x16
#define HPLL_CONTROL_720p			0xA0
#define AVID_START_PIXEL_LSB_720p		0x04
#define AVID_START_PIXEL_MSB_720p		0x01
#define AVID_STOP_PIXEL_LSB_720p		0x08
#define AVID_STOP_PIXEL_MSB_720p		0x06
#define VBLK_F0_START_LINE_OFFSET_720p		0x05
#define VBLK_F1_START_LINE_OFFSET_720p		0x00
#define VBLK_F0_DURATION_720p			0x2D
#define VBLK_F1_DURATION_720p			0x00
#define RESERVED_720p				0x03

#define FEEDBACK_DIVIDER_MSB_720p_50            0x7B
#define FEEDBACK_DIVIDER_LSB_720p_50            0x0C
#define VCO_CONTROL_720p_50                     0x02
#define CP_CURRENT_720p_50                      0x03
#define PHASE_SELECT_720p_50                    0x16
#define HPLL_CONTROL_720p_50			0xa0
#define AVID_START_PIXEL_LSB_720p_50		0x04
#define AVID_START_PIXEL_MSB_720p_50		0x01
#define AVID_STOP_PIXEL_LSB_720p_50		0x08
#define AVID_STOP_PIXEL_MSB_720p_50		0x06
#define VBLK_F0_START_LINE_OFFSET_720p_50	0x05
#define VBLK_F1_START_LINE_OFFSET_720p_50	0x00
#define VBLK_F0_DURATION_720p_50		0x2D
#define VBLK_F1_DURATION_720p_50		0x00
#define RESERVED_720p				0x03

#define FEEDBACK_DIVIDER_MSB_1080i              0x89
#define FEEDBACK_DIVIDER_LSB_1080i              0x08
#define VCO_CONTROL_1080i                       0x02
#define CP_CURRENT_1080i                        0x03
#define PHASE_SELECT_1080i                      0x14
#define HPLL_CONTROL_1080i			0xa0
//#define AVID_START_PIXEL_LSB_1080i		0x06
//#define AVID_START_PIXEL_MSB_1080i		0x01
#define AVID_START_PIXEL_LSB_1080i		0xc6
#define AVID_START_PIXEL_MSB_1080i		0x00
//#define AVID_STOP_PIXEL_LSB_1080i		0x8A
//#define AVID_STOP_PIXEL_MSB_1080i		0x08
#define AVID_STOP_PIXEL_LSB_1080i		0x4A
#define AVID_STOP_PIXEL_MSB_1080i		0x08
#define VBLK_F0_START_LINE_OFFSET_1080i		0x02
#define VBLK_F1_START_LINE_OFFSET_1080i		0x02
#define VBLK_F0_DURATION_1080i			0x16
#define VBLK_F1_DURATION_1080i			0x17
#define RESERVED_1080i				0x02

#define FEEDBACK_DIVIDER_MSB_1080i_50           0xA5
#define FEEDBACK_DIVIDER_LSB_1080i_50           0x00
#define VCO_CONTROL_1080i_50                    0x02
#define CP_CURRENT_1080i_50                     0x02
#define PHASE_SELECT_1080i_50                   0x14
#define HPLL_CONTROL_1080i_50			0xa0
#define AVID_START_PIXEL_LSB_1080i_50		0x06
#define AVID_START_PIXEL_MSB_1080i_50		0x01
#define AVID_STOP_PIXEL_LSB_1080i_50		0x8A
#define AVID_STOP_PIXEL_MSB_1080i_50		0x08
#define VBLK_F0_START_LINE_OFFSET_1080i_50	0x02
#define VBLK_F1_START_LINE_OFFSET_1080i_50	0x02
#define VBLK_F0_DURATION_1080i_50		0x16
#define VBLK_F1_DURATION_1080i_50		0x17
#define RESERVED_1080i_50			0x02


#define FEEDBACK_DIVIDER_MSB_1080P_30           0x89
#define FEEDBACK_DIVIDER_LSB_1080P_30           0x08
#define VCO_CONTROL_1080P_30                    0x02
#define CP_CURRENT_1080P_30                     0x03
#define PHASE_SELECT_1080P_30                   0x14
#define HPLL_CONTROL_1080P_30			0xa0
//#define AVID_START_PIXEL_LSB_1080P_30		0x06
//#define AVID_START_PIXEL_MSB_1080P_30		0x01
#define AVID_START_PIXEL_LSB_1080P_30		0xfe
#define AVID_START_PIXEL_MSB_1080P_30		0x00
//#define AVID_STOP_PIXEL_LSB_1080P_30		0x8A
//#define AVID_STOP_PIXEL_MSB_1080P_30		0x08
#define AVID_STOP_PIXEL_LSB_1080P_30		0x82
#define AVID_STOP_PIXEL_MSB_1080P_30		0x08
#define VBLK_F0_START_LINE_OFFSET_1080P_30	0x27
#define VBLK_F1_START_LINE_OFFSET_1080P_30	0x27
#define VBLK_F0_DURATION_1080P_30		0x16
#define VBLK_F1_DURATION_1080P_30		0x00
#define RESERVED_1080P_30			0x02

#define FEEDBACK_DIVIDER_MSB_1080P_25           0xA5
#define FEEDBACK_DIVIDER_LSB_1080P_25           0x00
#define VCO_CONTROL_1080P_25                    0x02
#define CP_CURRENT_1080P_25                     0x02
#define PHASE_SELECT_1080P_25                   0x14
#define HPLL_CONTROL_1080P_25			0xa0
#define AVID_START_PIXEL_LSB_1080P_25		0x06
#define AVID_START_PIXEL_MSB_1080P_25		0x01
#define AVID_STOP_PIXEL_LSB_1080P_25		0x8A
#define AVID_STOP_PIXEL_MSB_1080P_25		0x08
#define VBLK_F0_START_LINE_OFFSET_1080P_25	0x27
#define VBLK_F1_START_LINE_OFFSET_1080P_25	0x27
#define VBLK_F0_DURATION_1080P_25		0x16
#define VBLK_F1_DURATION_1080P_25		0x00
#define RESERVED_1080P   			0x02

#define FEEDBACK_DIVIDER_MSB_1080P_24           0xAB
#define FEEDBACK_DIVIDER_LSB_1080P_24           0x0E
#define VCO_CONTROL_1080P_24                    0x02
#define CP_CURRENT_1080P_24                     0x02
#define PHASE_SELECT_1080P_24                   0x14
#define HPLL_CONTROL_1080P_24			0xa0
#define AVID_START_PIXEL_LSB_1080P_24		0x06
#define AVID_START_PIXEL_MSB_1080P_24		0x01
#define AVID_STOP_PIXEL_LSB_1080P_24		0x8A
#define AVID_STOP_PIXEL_MSB_1080P_24		0x08
#define VBLK_F0_START_LINE_OFFSET_1080P_24	0x27
#define VBLK_F1_START_LINE_OFFSET_1080P_24	0x27
#define VBLK_F0_DURATION_1080P_24		0x16
#define VBLK_F1_DURATION_1080P_24		0x00
#define RESERVED_1080P_24 			0x02

#define FEEDBACK_DIVIDER_MSB_1080P_60           0x89
#define FEEDBACK_DIVIDER_LSB_1080P_60           0x08
#define VCO_CONTROL_1080P_60                    0x03
#define CP_CURRENT_1080P_60                     0x06
#define PHASE_SELECT_1080P_60                   0x80
#define HPLL_CONTROL_1080P_60			0xf0
//#define AVID_START_PIXEL_LSB_1080P_60		0x06
//#define AVID_START_PIXEL_MSB_1080P_60		0x01
#define AVID_START_PIXEL_LSB_1080P_60		0xfe
#define AVID_START_PIXEL_MSB_1080P_60		0x00
//#define AVID_STOP_PIXEL_LSB_1080P_60		0x8A
//#define AVID_STOP_PIXEL_MSB_1080P_60		0x08
#define AVID_STOP_PIXEL_LSB_1080P_60		0x82
#define AVID_STOP_PIXEL_MSB_1080P_60		0x08
#define VBLK_F0_START_LINE_OFFSET_1080P_60	0x27
#define VBLK_F1_START_LINE_OFFSET_1080P_60	0x27
#define VBLK_F0_DURATION_1080P_60		0x16
#define VBLK_F1_DURATION_1080P_60		0x00
#define RESERVED_1080P_60			0x02

#define FEEDBACK_DIVIDER_MSB_1080P_50           0xA5
#define FEEDBACK_DIVIDER_LSB_1080P_50           0x08
#define VCO_CONTROL_1080P_50                    0x03
#define CP_CURRENT_1080P_50                     0x06
#define PHASE_SELECT_1080P_50                   0x80
#define HPLL_CONTROL_1080P_50			0xf0
//#define AVID_START_PIXEL_LSB_1080P_50		0x06
//#define AVID_START_PIXEL_MSB_1080P_50		0x01
#define AVID_START_PIXEL_LSB_1080P_50		0xfe
#define AVID_START_PIXEL_MSB_1080P_50		0x00
//#define AVID_STOP_PIXEL_LSB_1080P_50		0x8A
//#define AVID_STOP_PIXEL_MSB_1080P_50		0x08
#define AVID_STOP_PIXEL_LSB_1080P_50		0x82
#define AVID_STOP_PIXEL_MSB_1080P_50		0x08
#define VBLK_F0_START_LINE_OFFSET_1080P_50	0x27
#define VBLK_F1_START_LINE_OFFSET_1080P_50	0x27
#define VBLK_F0_DURATION_1080P_50		0x16
#define VBLK_F1_DURATION_1080P_50		0x00
#define RESERVED_1080P_50			0x02

#define FEEDBACK_DIVIDER_MSB_480P		0x35
#define FEEDBACK_DIVIDER_LSB_480P		0x0A
#define VCO_CONTROL_480P			0x00
#define CP_CURRENT_480P				0x05
#define PHASE_SELECT_480P			0x14
#define HPLL_CONTROL_480P			0x28
#define AVID_START_PIXEL_LSB_480P		0x7a
#define AVID_START_PIXEL_MSB_480P		0x00
#define AVID_STOP_PIXEL_LSB_480P		0x4e
#define AVID_STOP_PIXEL_MSB_480P		0x03
#define VBLK_F0_START_LINE_OFFSET_480P		0x00
#define VBLK_F1_START_LINE_OFFSET_480P		0x00
#define VBLK_F0_DURATION_480P			0x2d
#define VBLK_F1_DURATION_480P			0x00
#define RESERVED_1080i_50                       0x02

#define FEEDBACK_DIVIDER_MSB_576P               0x36
#define FEEDBACK_DIVIDER_LSB_576P               0x00
#define VCO_CONTROL_576P                        0x00
#define CP_CURRENT_576P                         0x05
#define PHASE_SELECT_576P                       0x14
#define HPLL_CONTROL_576P                       0x28
#define AVID_START_PIXEL_LSB_576P               0x84
#define AVID_START_PIXEL_MSB_576P               0x00
#define AVID_STOP_PIXEL_LSB_576P                0x58
#define AVID_STOP_PIXEL_MSB_576P                0x03
#define VBLK_F0_START_LINE_OFFSET_576P          0x00
#define VBLK_F1_START_LINE_OFFSET_576P          0x00
#define VBLK_F0_DURATION_576P                   0x2C
#define VBLK_F1_DURATION_576P                   0x00
#define RESERVED_1080i_50                       0x02

#define FEEDBACK_DIVIDER_MSB_NTSC		0x35
#define FEEDBACK_DIVIDER_LSB_NTSC		0x0A
#define VCO_CONTROL_NTSC			0x00
#define CP_CURRENT_NTSC				0x02
#define PHASE_SELECT_NTSC			0x14
#define HPLL_CONTROL_NTSC			0x10
#define AVID_START_PIXEL_LSB_NTSC		0x7a
#define AVID_START_PIXEL_MSB_NTSC		0x00
#define AVID_STOP_PIXEL_LSB_NTSC		0x4E
#define AVID_STOP_PIXEL_MSB_NTSC		0x03
#define VBLK_F0_START_LINE_OFFSET_NTSC		0x03
#define VBLK_F1_START_LINE_OFFSET_NTSC		0x03
#define VBLK_F0_DURATION_NTSC			0x13
#define VBLK_F1_DURATION_NTSC			0x14

#define FEEDBACK_DIVIDER_MSB_PAL                0x36
#define FEEDBACK_DIVIDER_LSB_PAL                0x00
#define VCO_CONTROL_PAL                         0x00
#define CP_CURRENT_PAL                          0x02
#define PHASE_SELECT_PAL                        0x14
#define HPLL_CONTROL_PAL                        0x10
#define AVID_START_PIXEL_LSB_PAL                0x84
#define AVID_START_PIXEL_MSB_PAL                0x00
#define AVID_STOP_PIXEL_LSB_PAL                 0x58
#define AVID_STOP_PIXEL_MSB_PAL                 0x03
#define VBLK_F0_START_LINE_OFFSET_PAL           0x00
#define VBLK_F1_START_LINE_OFFSET_PAL           0x17
#define VBLK_F0_DURATION_PAL                    0x16
#define VBLK_F1_DURATION_PAL                    0x17



#define ADV9880_HD_ALC_PLACEMENT		0x5A
#define ADV9880_ED_ALC_PLACEMENT		0x18

#define ADV9880_HD_CLAMP_START			0x32
#define ADV9880_ED_CLAMP_START			0x06

#define ADV9880_HD_CLAMP_WIDTH			0x20
#define ADV9880_ED_CLAMP_WIDTH			0x10

#define ADV9880_HD_PRE_COAST			0x1
#define ADV9880_ED_PRE_COAST			0x03

#define ADV9880_HD_POST_COAST			0x0
#define ADV9880_ED_POST_COAST			0x0C

/* HPLL masks and shifts */
#define HPLL_DIVIDER_LSB_MASK                   GENERATE_MASK(4, 0)
#define HPLL_DIVIDER_LSB_SHIFT                  4
#define VCO_CONTROL_MASK                        GENERATE_MASK(2, 0)
#define CP_CURRENT_MASK                         GENERATE_MASK(3, 0)
#define VCO_CONTROL_SHIFT                       6
#define CP_CURRENT_SHIFT                        3
#define PHASE_SELECT_MASK                       GENERATE_MASK(5, 0)
#define PHASE_SELECT_SHIFT                      3


#define LINES_PER_VSYNC_MSB_MASK                GENERATE_MASK(4, 8)
#define LINES_PER_VSYNC_MSB_SHIFT               8


/* Gain and offset masks */

/* Defines for ADV9880 register address */
#define ADV9880_REVISION                        0x00
#define ADV9880_HPLL_DIVIDER_MSB                0x01
#define ADV9880_HPLL_DIVIDER_LSB                0x02
#define ADV9880_HPLL_CONTROL                    0x03
#define ADV9880_HPLL_PHASE_SELECT               0x04

#define ADV9880_RED_GAIN                        0x05
#define ADV9880_GREEN_GAIN                      0x06
#define ADV9880_BLUE_GAIN                       0x07

#define ADV9880_RED_AUTO_OFFSET_ADJUST          0x08
#define ADV9880_RED_OFFSET                      0x09
#define ADV9880_GREEN_AUTO_OFFSET_ADJUST        0x0a
#define ADV9880_GREEN_OFFSET                    0x0b
#define ADV9880_BLUE_AUTO_OFFSET_ADJUST         0x0c
#define ADV9880_BLUE_OFFSET                     0x0d

#define ADV9880_SYNC_SEPARATER_THLD             0x0e
#define ADV9880_SOG_THRESHOLD_ENTER             0x0f
#define ADV9880_SOG_THRESHOLD_EXIT              0x10

#define ADV9880_INPUT_SELECT                    0x11
#define ADV9880_INPUT_SELECT_INTERFACE_MASK     0x02
#define ADV9880_INPUT_SELECT_DIGITAL            0x02
#define ADV9880_INPUT_SELECT_ANALOG             0x00
#define ADV9880_INPUT_SELECT_MANUAL_MASK        0x01

#define ADV9880_COAST_CONTROL                   0x12
#define ADV9880_HPLL_PRE_COAST                  0x13
#define ADV9880_HPLL_POST_COAST                 0x14
#define ADV9880_SYNC_DETECT_1                   0x15
#define ADV9880_SYNC_DETECT_2                   0x16
#define ADV9880_LINES_PER_VSYNC_STATUS_HIGH     0x17
#define ADV9880_LINES_PER_VSYNC_STATUS_LOW      0x18
#define ADV9880_CLAMP_START                     0x19
#define ADV9880_CLAMP_WIDTH                     0x1a

#define ADV9880_CLAMP_CONTROL                   0x1b
#define ADV9880_AUTO_OFFSET_CONTROL             0x1c
#define ADV9880_AUTO_OFFSET_SLEW_LIMIT          0x1d
#define ADV9880_SYNC_FILTER_LOCK_THRESHOLD      0x1e
#define ADV9880_SYNC_FILTER_UNLOCK_THRESHOLD    0x1f
#define ADV9880_SYNC_FILTER_WINDOW_WIDTH        0x20
#define ADV9880_SYNC_FILTER_CONTROL             0x21
#define ADV9880_VSYNC_DURATION                  0x22
#define ADV9880_HSYNC_DURATION                  0x23
#define ADV9880_OUTPUT_SYNC_CONTROL             0x24
#define ADV9880_OUTPUT_MODE_CONTROL             0x25
#define ADV9880_POWER_CONTROL                   0x26
#define ADV9880_MISC_CONTROL_1                  0x27
#define ADV9880_VS_DELAY                        0x28
#define ADV9880_HS_DELAY                        0x29
#define ADV9880_LINE_WIDTH_MSB                  0x2A
#define ADV9880_LINE_WIDTH_LSB                  0x2B
#define ADV9880_SCREEN_HEIGHT_MSB               0x2C
#define ADV9880_SCREEN_HEIGHT_LSB               0x2D

/* HDMI registers */
#define ADV9880_I2S_CONTROL                     0x2E
#define ADV9880_HDMI_STATUS_1                   0x2F
#define ADV9880_HDMI_STATUS_2                   0x30
#define ADV9880_MACROVISION_CONTROL_1           0x31
#define ADV9880_MACROVISION_CONTROL_2           0x32
#define ADV9880_MACROVISION_CONTROL_3           0x33
#define ADV9880_COLOR_CONVERT_CONTROL           0x34

#define ADV9880_HDMI_STATUS_1_DE_MASK           0x40

/* Color conversion: 0x35 - 0x4C */
#define ADV9880_COLOR_CONVERT_COEF_BASE         0x35

#define ADV9880_AV_MUTE_CONTROL                 0x57
#define ADV9880_MCLK_CONTROL                    0x58
#define ADV9880_HDMI_CONTROL_1                  0x59
#define ADV9880_PACKET_DETECT_STATUS            0x5A
#define ADV9880_HDMI_MODE                       0x5B

#define ADV9880_HDMI_MODE_DVI_HDMI_MASK         0x08
#define ADV9880_HDMI_MODE_HDMI                  0x08
#define ADV9880_HDMI_MODE_DVI                   0x00


#define ADV9880_CHANNEL_STATUS                  0x5E

#define ADV9880_AUDIO_SAMPLE_RATE               0x61
#define ADV9880_AUDIO_SAMPLE_SIZE               0x62
#define ADV9880_AUDIO_CTS_BASE                  0x7B
#define ADV9880_AUDIO_N_BASE                    0x7D

#define ADV9880_AVI_INFOFRAME_1                 0x81

#define ADV9880_AVI_INFOFRAME_1_CSIN_MASK       0x60
#define ADV9880_AVI_INFOFRAME_1_CSIN_RGB        0x00
#define ADV9880_AVI_INFOFRAME_1_CSIN_YCBCR422   0x20
#define ADV9880_AVI_INFOFRAME_1_CSIN_YCBCR444   0x40

#define ADV9880_AVI_INFOFRAME_VIDEO_ID          0x84



#define ADV9880_MISC_CONTROL_1_BT656_EN_MASK    0x10
#define ADV9880_SYNC_DETECT_2_SYNC_LOCK_MASK    0x02


#define ADV9880_OUTPUT_SYNC_CONTROL_HSYNC_POL_MASK 0x80
#define ADV9880_OUTPUT_SYNC_CONTROL_VSYNC_POL_MASK 0x40
#define ADV9880_OUTPUT_SYNC_CONTROL_DE_POL_MASK    0x20
#define ADV9880_OUTPUT_SYNC_CONTROL_FIELD_POL_MASK 0x10



/* decoder standard related strctures */
#define ADV9880_MAX_NO_INPUTS			2
#define ADV9880_MAX_NO_STANDARDS		13
#define ADV9880_MAX_NO_CONTROLS			0

#define ADV9880_ALC_VCOEFF_SHIFT		4

#define ADV9880_STANDARD_INFO_SIZE      (ADV9880_MAX_NO_STANDARDS)

struct adv9880_control_info {
	int register_address;
	struct v4l2_queryctrl query_control;
};

struct adv9880_config {
	int no_of_inputs;
	struct {
		int input_type;
		struct v4l2_input input_info;
		int no_of_standard;
		struct v4l2_standard *standard;
		v4l2_std_id def_std;
		struct adv9880_format_params *format;
		int no_of_controls;
		struct adv9880_control_info *controls;
	} input[ADV9880_MAX_NO_INPUTS];
	struct adv9880_params def_params;
};

#endif				/* __KERNEL__ */
#endif
