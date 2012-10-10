#define DEBUG
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
#include <linux/version.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/ctype.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/i2c-id.h>
#include <linux/device.h>
#include <linux/workqueue.h>

#include <media/v4l2-ioctl.h>
#include <linux/videodev2.h>
#include <media/v4l2-device.h>
#include <media/v4l2-chip-ident.h>
#include <media/davinci/videohd.h> // For HD std (V4L2_STD_1080I, etc)

#include <asm/uaccess.h>
#if defined(CONFIG_DAVINCI_DM368_FPGA)
#include <asm/arch/dm368_fpga.h>
#endif

#include "adv9880.h"

/* Function Prototypes */
static int adv9880_set_hdmi_colorspace(struct i2c_client *ch_client);
static int adv9880_set_format_params(struct v4l2_subdev *sd,
                                     struct adv9880_format_params *tvpformats);
static int adv9880_i2c_read_reg(struct i2c_client *client, u8 reg, u8 * val);
static int adv9880_i2c_write_reg(struct i2c_client *client, u8 reg, u8 val);
static int adv9880_querystd(struct v4l2_subdev *sd, v4l2_std_id *id);
static int adv9880_s_std(struct v4l2_subdev *sd, v4l2_std_id std);
static int adv9880_s_routing(struct v4l2_subdev *sd,
                             u32 input, u32 output, u32 config);

static int adv9880_initialize(struct v4l2_subdev *sd);
static int adv9880_deinitialize(struct v4l2_subdev *sd);

static 	v4l2_std_id cea861_stds[256] = {
     [2]  = V4L2_STD_525P_60,
     [3]  = V4L2_STD_525P_60,
     
     [4]  = V4L2_STD_720P_60,
     [5]  = V4L2_STD_1080I_60,

     [16] = V4L2_STD_1080P_60,

     [19] = V4L2_STD_720P_50,
     [20] = V4L2_STD_1080I_50,

     [31] = V4L2_STD_1080P_50,
     [32] = V4L2_STD_1080P_24,
     [33] = V4L2_STD_1080P_25,
     [34] = V4L2_STD_1080P_30,

     [39] = V4L2_STD_1080I_50, // 1080I, 1250 total, 50fps

//     [60] = V4L2_STD_720P_24,
     [61] = V4L2_STD_720P_25,
     [62] = V4L2_STD_720P_30,
};

unsigned int cea861_fps_1000[256] = {
     [2]  = 60000,
     [3]  = 60000,
     
     [4]  = 60000,
     [5]  = 60000,

     [16] = 60000,

     [19] = 50000,
     [20] = 50000,

     [31] = 50000,
     [32] = 24000,
     [33] = 25000,
     [34] = 30000,

     [39] = 50000, // 1080I, 1250 total, 50fps

     [60] = 24000,
     [61] = 25000,
     [62] = 30000,
};

static struct v4l2_standard adv9880_standards[ADV9880_MAX_NO_STANDARDS] = {
	{
		.index = 0,
		.id = V4L2_STD_720P_60,
		.name = "720P-60",
		.frameperiod = {1, 60},
		.framelines = 750
	},
	{
		.index = 1,
		.id = V4L2_STD_1080I_60,
		.name = "1080I-30",
		.frameperiod = {1, 30},
		.framelines = 1125
	},
	{
		.index = 2,
		.id = V4L2_STD_1080I_50,
		.name = "1080I-25",
		.frameperiod = {1, 25},
		.framelines = 1125
	},
	{
		.index = 3,
		.id = V4L2_STD_720P_50,
		.name = "720P-50",
		.frameperiod = {1, 50},
		.framelines = 750
	},
	{
		.index = 4,
		.id = V4L2_STD_1080P_25,
		.name = "1080P-25",
		.frameperiod = {1, 25},
		.framelines = 1125
	},
	{
		.index = 5,
		.id = V4L2_STD_1080P_30,
		.name = "1080P-30",
		.frameperiod = {1, 30},
		.framelines = 1125
	},
	{
		.index = 6,
		.id = V4L2_STD_1080P_24,
		.name = "1080P-24",
		.frameperiod = {1, 24},
		.framelines = 1125
	},
	{
		.index = 7,
		.id = V4L2_STD_525P_60,
		.name = "480P-60",
		.frameperiod = {1, 60},
		.framelines = 525
	},
	{
		.index = 8,
		.id = V4L2_STD_625P_50,
		.name = "576P-50",
		.frameperiod = {1, 50},
		.framelines = 625
	},
	{
		.index = 9,
		.id = V4L2_STD_525_60,
		.name = "NTSC",
		.frameperiod = {1001, 30000},
		.framelines = 525
	},
	{
		.index = 10,
		.id = V4L2_STD_625_50,
		.name = "PAL",
		.frameperiod = {1, 25},
		.framelines = 625
	},
	{
		.index = 11,
		.id = V4L2_STD_1080P_50,
		.name = "1080P-50",
		.frameperiod = {1, 50},
		.framelines = 1125
	},
	{
		.index = 12,
		.id = V4L2_STD_1080P_60,
		.name = "1080P-60",
		.frameperiod = {1, 60},
		.framelines = 1125
	},

};

static struct adv9880_format_params
	adv9880_formats[ADV9880_MAX_NO_STANDARDS] = {
	{
		.hpll_divider_msb = FEEDBACK_DIVIDER_MSB_720p,
		.hpll_divider_lsb = FEEDBACK_DIVIDER_LSB_720p,
		.hpll_vco_control = VCO_CONTROL_720p,
		.hpll_cp_current = CP_CURRENT_720p,
		.hpll_phase_select = PHASE_SELECT_720p,
		.hpll_control = HPLL_CONTROL_720p,
		.avid_start_msb = AVID_START_PIXEL_MSB_720p,
		.avid_start_lsb = AVID_START_PIXEL_LSB_720p,
		.avid_stop_lsb = AVID_STOP_PIXEL_LSB_720p,
		.avid_stop_msb = AVID_STOP_PIXEL_MSB_720p,
		.vblk_start_f0_line_offset = 30, // VBLK_F0_START_LINE_OFFSET_720p,
		.vblk_start_f1_line_offset = VBLK_F1_START_LINE_OFFSET_720p,
		.vblk_f0_duration = 0, // VBLK_F0_DURATION_720p,
		.vblk_f1_duration = 0, // VBLK_F1_DURATION_720p,
		.clamp_start = ADV9880_HD_CLAMP_START,
		.clamp_width = ADV9880_HD_CLAMP_WIDTH,
		.hpll_pre_coast = ADV9880_HD_PRE_COAST,
		.hpll_post_coast = ADV9880_HD_POST_COAST,
		.reserved = RESERVED_720p,
                .screen_height_msb = 0x02,
                .screen_height_lsb = 0xed,
                .sync_filter_control = 0xdc,
				.vsync_duration = 26,

	},
	{
		.hpll_divider_msb = FEEDBACK_DIVIDER_MSB_1080i,
		.hpll_divider_lsb = FEEDBACK_DIVIDER_LSB_1080i,
		.hpll_vco_control = VCO_CONTROL_1080i,
		.hpll_cp_current = CP_CURRENT_1080i,
		.hpll_phase_select = PHASE_SELECT_1080i,
		.hpll_control = HPLL_CONTROL_1080i,
		.avid_start_msb = AVID_START_PIXEL_MSB_1080i,
		.avid_start_lsb = AVID_START_PIXEL_LSB_1080i,
		.avid_stop_lsb = AVID_STOP_PIXEL_LSB_1080i,
		.avid_stop_msb = AVID_STOP_PIXEL_MSB_1080i,
		.vblk_start_f0_line_offset = 1,//VBLK_F0_START_LINE_OFFSET_1080i,
		.vblk_start_f1_line_offset = 0,//VBLK_F1_START_LINE_OFFSET_1080i,
		.vblk_f0_duration = 0,//VBLK_F0_DURATION_1080i,
		.vblk_f1_duration = 1,//VBLK_F1_DURATION_1080i,
		.clamp_start = ADV9880_HD_CLAMP_START,
		.clamp_width = ADV9880_HD_CLAMP_WIDTH,
		.hpll_pre_coast = ADV9880_HD_PRE_COAST,
		.hpll_post_coast = ADV9880_HD_POST_COAST,
		.reserved = RESERVED_1080i,
                .screen_height_msb = 0x04,
                .screen_height_lsb = 0x64,
                .sync_filter_control = ADV9880_SYNC_FILTER_CONTROL_DEFAULT,
//		.vsync_duration = ADV9880_VSYNC_DURATION_DEFAULT,
		.vsync_duration = 2,
	},
	{
		.hpll_divider_msb = FEEDBACK_DIVIDER_MSB_1080i_50,
		.hpll_divider_lsb = FEEDBACK_DIVIDER_LSB_1080i_50,
		.hpll_vco_control = VCO_CONTROL_1080i_50,
		.hpll_cp_current = CP_CURRENT_1080i_50,
		.hpll_phase_select = PHASE_SELECT_1080i_50,
		.hpll_control = HPLL_CONTROL_1080i_50,
		.avid_start_msb = AVID_START_PIXEL_MSB_1080i_50,
		.avid_start_lsb = AVID_START_PIXEL_LSB_1080i_50,
		.avid_stop_lsb = AVID_STOP_PIXEL_LSB_1080i_50,
		.avid_stop_msb = AVID_STOP_PIXEL_MSB_1080i_50,
		.vblk_start_f0_line_offset = VBLK_F0_START_LINE_OFFSET_1080i_50,
		.vblk_start_f1_line_offset = VBLK_F1_START_LINE_OFFSET_1080i_50,
		.vblk_f0_duration = VBLK_F0_DURATION_1080i_50,
		.vblk_f1_duration = VBLK_F1_DURATION_1080i_50,
		.clamp_start = ADV9880_HD_CLAMP_START,
		.clamp_width = ADV9880_HD_CLAMP_WIDTH,
		.hpll_pre_coast = ADV9880_HD_PRE_COAST,
		.hpll_post_coast = ADV9880_HD_POST_COAST,
		.reserved = RESERVED_1080i_50,
                .screen_height_msb = 0x04,
                .screen_height_lsb = 0x38,
                .sync_filter_control = ADV9880_SYNC_FILTER_CONTROL_DEFAULT,
				.vsync_duration = ADV9880_VSYNC_DURATION_DEFAULT,
	},
	{
		.hpll_divider_msb = FEEDBACK_DIVIDER_MSB_720p_50,
		.hpll_divider_lsb = FEEDBACK_DIVIDER_LSB_720p_50,
		.hpll_vco_control = VCO_CONTROL_720p_50,
		.hpll_cp_current = CP_CURRENT_720p_50,
		.hpll_phase_select = PHASE_SELECT_720p_50,
		.hpll_control = HPLL_CONTROL_720p_50,
		.avid_start_msb = AVID_START_PIXEL_MSB_720p_50,
		.avid_start_lsb = AVID_START_PIXEL_LSB_720p_50,
		.avid_stop_lsb = AVID_STOP_PIXEL_LSB_720p_50,
		.avid_stop_msb = AVID_STOP_PIXEL_MSB_720p_50,
		.vblk_start_f0_line_offset = VBLK_F0_START_LINE_OFFSET_720p_50,
		.vblk_start_f1_line_offset = VBLK_F1_START_LINE_OFFSET_720p_50,
		.vblk_f0_duration = VBLK_F0_DURATION_720p_50,
		.vblk_f1_duration = 0,
		.clamp_start = ADV9880_HD_CLAMP_START,
		.clamp_width = ADV9880_HD_CLAMP_WIDTH,
		.hpll_pre_coast = ADV9880_HD_PRE_COAST,
		.hpll_post_coast = ADV9880_HD_POST_COAST,
		.reserved = RESERVED_720p,
                .screen_height_msb = 0x02,
                .screen_height_lsb = 0xd0,
                .sync_filter_control = ADV9880_SYNC_FILTER_CONTROL_DEFAULT,
				.vsync_duration = ADV9880_VSYNC_DURATION_DEFAULT,
	},
	{
		.hpll_divider_msb = FEEDBACK_DIVIDER_MSB_1080P_25,
		.hpll_divider_lsb = FEEDBACK_DIVIDER_LSB_1080P_25,
		.hpll_vco_control = VCO_CONTROL_1080P_25,
		.hpll_cp_current = CP_CURRENT_1080P_25,
		.hpll_phase_select = PHASE_SELECT_1080P_25,
		.hpll_control = HPLL_CONTROL_1080P_25,
		.avid_start_msb = AVID_START_PIXEL_MSB_1080P_25,
		.avid_start_lsb = AVID_START_PIXEL_LSB_1080P_25,
		.avid_stop_lsb = AVID_STOP_PIXEL_LSB_1080P_25,
		.avid_stop_msb = AVID_STOP_PIXEL_MSB_1080P_25,
		.vblk_start_f0_line_offset = VBLK_F0_START_LINE_OFFSET_1080P_25,
		.vblk_start_f1_line_offset = VBLK_F1_START_LINE_OFFSET_1080P_25,
		.vblk_f0_duration = VBLK_F0_DURATION_1080P_25,
		.vblk_f1_duration = 0,
		.clamp_start = ADV9880_HD_CLAMP_START,
		.clamp_width = ADV9880_HD_CLAMP_WIDTH,
		.hpll_pre_coast = ADV9880_HD_PRE_COAST,
		.hpll_post_coast = ADV9880_HD_POST_COAST,
		.reserved = RESERVED_1080P_30,
                .screen_height_msb = 0x04,
                .screen_height_lsb = 0x38,
                .sync_filter_control = ADV9880_SYNC_FILTER_CONTROL_DEFAULT,
				.vsync_duration = ADV9880_VSYNC_DURATION_DEFAULT,
	},
	{
		.hpll_divider_msb = FEEDBACK_DIVIDER_MSB_1080P_30,
		.hpll_divider_lsb = FEEDBACK_DIVIDER_LSB_1080P_30,
		.hpll_vco_control = VCO_CONTROL_1080P_30,
		.hpll_cp_current = CP_CURRENT_1080P_30,
		.hpll_phase_select = PHASE_SELECT_1080P_30,
		.hpll_control = HPLL_CONTROL_1080P_30,
		.avid_start_msb = AVID_START_PIXEL_MSB_1080P_30,
		.avid_start_lsb = AVID_START_PIXEL_LSB_1080P_30,
		.avid_stop_lsb = AVID_STOP_PIXEL_LSB_1080P_30,
		.avid_stop_msb = AVID_STOP_PIXEL_MSB_1080P_30,
		.vblk_start_f0_line_offset = VBLK_F0_START_LINE_OFFSET_1080P_30,
		.vblk_start_f1_line_offset = VBLK_F1_START_LINE_OFFSET_1080P_30,
		.vblk_f0_duration = VBLK_F0_DURATION_1080P_30,
		.vblk_f1_duration = 0,
		.clamp_start = ADV9880_HD_CLAMP_START,
		.clamp_width = ADV9880_HD_CLAMP_WIDTH,
		.hpll_pre_coast = ADV9880_HD_PRE_COAST,
		.hpll_post_coast = ADV9880_HD_POST_COAST,
		.reserved = RESERVED_1080P,
                .screen_height_msb = 0x04,
                .screen_height_lsb = 0x38,
                .sync_filter_control = ADV9880_SYNC_FILTER_CONTROL_DEFAULT,
				.vsync_duration = ADV9880_VSYNC_DURATION_DEFAULT,
	},
	{
		.hpll_divider_msb = FEEDBACK_DIVIDER_MSB_1080P_24,
		.hpll_divider_lsb = FEEDBACK_DIVIDER_LSB_1080P_24,
		.hpll_vco_control = VCO_CONTROL_1080P_24,
		.hpll_cp_current = CP_CURRENT_1080P_24,
		.hpll_phase_select = PHASE_SELECT_1080P_24,
		.hpll_control = HPLL_CONTROL_1080P_24,
		.avid_start_msb = AVID_START_PIXEL_MSB_1080P_24,
		.avid_start_lsb = AVID_START_PIXEL_LSB_1080P_24,
		.avid_stop_lsb = AVID_STOP_PIXEL_LSB_1080P_24,
		.avid_stop_msb = AVID_STOP_PIXEL_MSB_1080P_24,
		.vblk_start_f0_line_offset = VBLK_F0_START_LINE_OFFSET_1080P_24,
		.vblk_start_f1_line_offset = VBLK_F1_START_LINE_OFFSET_1080P_24,
		.vblk_f0_duration = VBLK_F0_DURATION_1080P_24,
		.vblk_f1_duration = 0,
		.clamp_start = ADV9880_HD_CLAMP_START,
		.clamp_width = ADV9880_HD_CLAMP_WIDTH,
		.hpll_pre_coast = ADV9880_HD_PRE_COAST,
		.hpll_post_coast = ADV9880_HD_POST_COAST,
		.reserved = RESERVED_1080P,
                .screen_height_msb = 0x04,
                .screen_height_lsb = 0x38,
                .sync_filter_control = ADV9880_SYNC_FILTER_CONTROL_DEFAULT,
				.vsync_duration = ADV9880_VSYNC_DURATION_DEFAULT,
	},
	{
		.hpll_divider_msb = FEEDBACK_DIVIDER_MSB_480P,
		.hpll_divider_lsb = FEEDBACK_DIVIDER_LSB_480P,
		.hpll_vco_control = VCO_CONTROL_480P,
		.hpll_cp_current = CP_CURRENT_480P,
		.hpll_phase_select = PHASE_SELECT_480P,
		.hpll_control = HPLL_CONTROL_480P,
		.avid_start_msb = AVID_START_PIXEL_MSB_480P,
		.avid_start_lsb = AVID_START_PIXEL_LSB_480P,
		.avid_stop_lsb = AVID_STOP_PIXEL_LSB_480P,
		.avid_stop_msb = AVID_STOP_PIXEL_MSB_480P,
		.vblk_start_f0_line_offset = VBLK_F0_START_LINE_OFFSET_480P,
		.vblk_start_f1_line_offset = 0,//VBLK_F1_START_LINE_OFFSET_480P,
		.vblk_f0_duration = VBLK_F0_DURATION_480P,
		.vblk_f1_duration = 0,//VBLK_F1_DURATION_480P,
		.clamp_start = ADV9880_ED_CLAMP_START,
		.clamp_width = ADV9880_ED_CLAMP_WIDTH,
		.hpll_pre_coast = ADV9880_ED_PRE_COAST,
		.hpll_post_coast = ADV9880_ED_POST_COAST,
		.reserved = RESERVED_720p,
                .screen_height_msb = 0x02,
                .screen_height_lsb = 0x0c,
                .sync_filter_control = ADV9880_SYNC_FILTER_CONTROL_DEFAULT,
				.vsync_duration = ADV9880_VSYNC_DURATION_DEFAULT,
	},
	{
		.hpll_divider_msb = FEEDBACK_DIVIDER_MSB_576P,
		.hpll_divider_lsb = FEEDBACK_DIVIDER_LSB_576P,
		.hpll_vco_control = VCO_CONTROL_576P,
		.hpll_cp_current = CP_CURRENT_576P,
		.hpll_phase_select = PHASE_SELECT_576P,
		.hpll_control = HPLL_CONTROL_576P,
		.avid_start_msb = AVID_START_PIXEL_MSB_576P,
		.avid_start_lsb = AVID_START_PIXEL_LSB_576P,
		.avid_stop_lsb = AVID_STOP_PIXEL_LSB_576P,
		.avid_stop_msb = AVID_STOP_PIXEL_MSB_576P,
		.vblk_start_f0_line_offset = VBLK_F0_START_LINE_OFFSET_576P,
		.vblk_start_f1_line_offset = VBLK_F1_START_LINE_OFFSET_576P,
		.vblk_f0_duration = VBLK_F0_DURATION_576P,
		.vblk_f1_duration = 0,//VBLK_F1_DURATION_576P,
		.clamp_start = ADV9880_ED_CLAMP_START,
		.clamp_width = ADV9880_ED_CLAMP_WIDTH,
		.hpll_pre_coast = ADV9880_ED_PRE_COAST,
		.hpll_post_coast = ADV9880_ED_POST_COAST,
		.reserved = RESERVED_720p,
                .screen_height_msb = 0x02,
                .screen_height_lsb = 0x70,
                .sync_filter_control = ADV9880_SYNC_FILTER_CONTROL_DEFAULT,
				.vsync_duration = ADV9880_VSYNC_DURATION_DEFAULT,
	},
	{
		.hpll_divider_msb = FEEDBACK_DIVIDER_MSB_NTSC,
		.hpll_divider_lsb = FEEDBACK_DIVIDER_LSB_NTSC,
		.hpll_vco_control = VCO_CONTROL_NTSC,
		.hpll_cp_current = CP_CURRENT_NTSC,
		.hpll_phase_select = PHASE_SELECT_NTSC,
		.hpll_control = HPLL_CONTROL_NTSC,
		.avid_start_msb = AVID_START_PIXEL_MSB_NTSC,
		.avid_start_lsb = AVID_START_PIXEL_LSB_NTSC,
		.avid_stop_lsb = AVID_STOP_PIXEL_LSB_NTSC,
		.avid_stop_msb = AVID_STOP_PIXEL_MSB_NTSC,
		.vblk_start_f0_line_offset = VBLK_F0_START_LINE_OFFSET_NTSC,
		.vblk_start_f1_line_offset = VBLK_F1_START_LINE_OFFSET_NTSC,
		.vblk_f0_duration = VBLK_F0_DURATION_NTSC,
		.vblk_f1_duration = VBLK_F1_DURATION_NTSC,
		.clamp_start = ADV9880_ED_CLAMP_START,
		.clamp_width = ADV9880_ED_CLAMP_WIDTH,
		.hpll_pre_coast = ADV9880_ED_PRE_COAST,
		.hpll_post_coast = ADV9880_ED_POST_COAST,
		.reserved = RESERVED_720p,
                .screen_height_msb = 0x02,
                .screen_height_lsb = 0x0c,
                .sync_filter_control = ADV9880_SYNC_FILTER_CONTROL_DEFAULT,
				.vsync_duration = ADV9880_VSYNC_DURATION_DEFAULT,
	},
	{
		.hpll_divider_msb = FEEDBACK_DIVIDER_MSB_PAL,
		.hpll_divider_lsb = FEEDBACK_DIVIDER_LSB_PAL,
		.hpll_vco_control = VCO_CONTROL_PAL,
		.hpll_cp_current = CP_CURRENT_PAL,
		.hpll_phase_select = PHASE_SELECT_PAL,
		.hpll_control = HPLL_CONTROL_PAL,
		.avid_start_msb = AVID_START_PIXEL_MSB_PAL,
		.avid_start_lsb = AVID_START_PIXEL_LSB_PAL,
		.avid_stop_lsb = AVID_STOP_PIXEL_LSB_PAL,
		.avid_stop_msb = AVID_STOP_PIXEL_MSB_PAL,
		.vblk_start_f0_line_offset = VBLK_F0_START_LINE_OFFSET_PAL,
		.vblk_start_f1_line_offset = VBLK_F1_START_LINE_OFFSET_PAL,
		.vblk_f0_duration = VBLK_F0_DURATION_PAL,
		.vblk_f1_duration = VBLK_F1_DURATION_PAL,
		.clamp_start = ADV9880_ED_CLAMP_START,
		.clamp_width = ADV9880_ED_CLAMP_WIDTH,
		.hpll_pre_coast = ADV9880_ED_PRE_COAST,
		.hpll_post_coast = ADV9880_ED_POST_COAST,
		.reserved = RESERVED_720p,
                .screen_height_msb = 0x02,
                .screen_height_lsb = 0x70,
                .sync_filter_control = ADV9880_SYNC_FILTER_CONTROL_DEFAULT,
				.vsync_duration = ADV9880_VSYNC_DURATION_DEFAULT,
	},
	{
		.hpll_divider_msb = FEEDBACK_DIVIDER_MSB_1080P_50,
		.hpll_divider_lsb = FEEDBACK_DIVIDER_LSB_1080P_50,
		.hpll_vco_control = VCO_CONTROL_1080P_50,
		.hpll_cp_current = CP_CURRENT_1080P_50,
		.hpll_phase_select = PHASE_SELECT_1080P_50,
		.hpll_control = HPLL_CONTROL_1080P_50,
		.avid_start_msb = AVID_START_PIXEL_MSB_1080P_50,
		.avid_start_lsb = AVID_START_PIXEL_LSB_1080P_50,
		.avid_stop_lsb = AVID_STOP_PIXEL_LSB_1080P_50,
		.avid_stop_msb = AVID_STOP_PIXEL_MSB_1080P_50,
		.vblk_start_f0_line_offset = VBLK_F0_START_LINE_OFFSET_1080P_50,
		.vblk_start_f1_line_offset = VBLK_F1_START_LINE_OFFSET_1080P_50,
		.vblk_f0_duration = VBLK_F0_DURATION_1080P_50,
		.vblk_f1_duration = 0,//VBLK_F1_DURATION_1080P_50,
		.clamp_start = ADV9880_HD_CLAMP_START,
		.clamp_width = ADV9880_HD_CLAMP_WIDTH,
		.hpll_pre_coast = ADV9880_HD_PRE_COAST,
		.hpll_post_coast = ADV9880_HD_POST_COAST,
		.reserved = RESERVED_1080P,
                .screen_height_msb = 0x04,
                .screen_height_lsb = 0x38,
                .sync_filter_control = ADV9880_SYNC_FILTER_CONTROL_DEFAULT,
				.vsync_duration = ADV9880_VSYNC_DURATION_DEFAULT,
	},
	{
		.hpll_divider_msb = FEEDBACK_DIVIDER_MSB_1080P_60,
		.hpll_divider_lsb = FEEDBACK_DIVIDER_LSB_1080P_60,
		.hpll_vco_control = VCO_CONTROL_1080P_60,
		.hpll_cp_current = CP_CURRENT_1080P_60,
		.hpll_phase_select = PHASE_SELECT_1080P_60,
		.hpll_control = HPLL_CONTROL_1080P_60,
		.avid_start_msb = AVID_START_PIXEL_MSB_1080P_60,
		.avid_start_lsb = AVID_START_PIXEL_LSB_1080P_60,
		.avid_stop_lsb = AVID_STOP_PIXEL_LSB_1080P_60,
		.avid_stop_msb = AVID_STOP_PIXEL_MSB_1080P_60,
		.vblk_start_f0_line_offset = 45,//VBLK_F0_START_LINE_OFFSET_1080P_60,
		.vblk_start_f1_line_offset = 0,//VBLK_F1_START_LINE_OFFSET_1080P_60,
		.vblk_f0_duration = VBLK_F0_DURATION_1080P_60,
		.vblk_f1_duration = 0,//VBLK_F1_DURATION_1080P_60,
		.clamp_start = ADV9880_HD_CLAMP_START,
		.clamp_width = ADV9880_HD_CLAMP_WIDTH,
		.hpll_pre_coast = ADV9880_HD_PRE_COAST,
		.hpll_post_coast = ADV9880_HD_POST_COAST,
		.reserved = RESERVED_1080P,
                .screen_height_msb = 0x04,
                .screen_height_lsb = 0x64,
                .sync_filter_control = 0xdc,	/* Enable VSYNC duration, disable filter */
				.vsync_duration = 41,
	},
};

static struct adv9880_config adv9880_configuration[ADV9880_NUM_CHANNELS] = {
	{
		.no_of_inputs = ADV9880_MAX_NO_INPUTS,
		.input[0] = {
			.input_type = ADV9880_ANALOG_INPUT,
			.input_info = {
				.index = 0,
				.name = "COMPONENT",
				.type = V4L2_INPUT_TYPE_CAMERA,
				.std = V4L2_STD_ADV9880_ALL
			},
			.no_of_standard = ADV9880_MAX_NO_STANDARDS,
			.standard = (struct v4l2_standard *)&adv9880_standards,
			.def_std = V4L2_STD_720P_60,
			.format =
			   (struct adv9880_format_params *)&adv9880_formats,
			.no_of_controls = ADV9880_MAX_NO_CONTROLS,
			.controls = NULL
		},
		.input[1] = {
			.input_type = ADV9880_DIGITAL_INPUT,
			.input_info = {
				.index = 1,
				.name = "HDMI",
				.type = V4L2_INPUT_TYPE_CAMERA,
				.std = V4L2_STD_ADV9880_ALL
			},
			.no_of_standard = ADV9880_MAX_NO_STANDARDS,
			.standard = (struct v4l2_standard *)&adv9880_standards,
			.def_std = V4L2_STD_720P_60,
			.format =
			   (struct adv9880_format_params *)&adv9880_formats,
			.no_of_controls = ADV9880_MAX_NO_CONTROLS,
			.controls = NULL
		},
		.def_params = {V4L2_STD_1080P_60, 1,
				{1, 0xa, 0x6}, {0, 0, 0, 7, 7, 7},
				{0x80, 0x80, 0x80, 0, 0, 0, 0x10, 0x10, 0x10} }
	 }
};

struct adv9880_channel {
	struct v4l2_subdev    sd;
        struct work_struct    work;
        int                   ch_id;
	struct adv9880_params params;
};


static const struct v4l2_subdev_video_ops adv9880_video_ops = {
	.querystd = adv9880_querystd,
	.s_routing = adv9880_s_routing,
//	.g_input_status = adv9880_g_input_status,
};

static const struct v4l2_subdev_core_ops adv9880_core_ops = {
        .g_chip_ident = NULL,
	.s_std = adv9880_s_std,
};

static const struct v4l2_subdev_ops adv9880_ops = {
	.core = &adv9880_core_ops,
	.video = &adv9880_video_ops,
};


static struct timer_list adv9880_timer;
static unsigned int adv9880_timeout_ms = 1000;


static inline struct adv9880_channel *to_adv9880(struct v4l2_subdev *sd)
{
	return container_of(sd, struct adv9880_channel, sd);
}


static void adv9880_timer_function(unsigned long data)
{
     struct adv9880_channel *channel = (struct adv9880_channel *) data;

     schedule_work(&channel->work);
     mod_timer( &adv9880_timer, jiffies + msecs_to_jiffies( adv9880_timeout_ms ) );
}


static void adv9880_work(struct work_struct *work)
{
     struct adv9880_channel *channel = container_of(work, struct adv9880_channel, work);
     struct i2c_client *ch_client = v4l2_get_subdevdata(&channel->sd);
     int ch_id = channel->ch_id;
     int err;
     int inputidx;
     int input_type;

     if (ch_client != NULL ) {

          inputidx = channel->params.inputidx;
          input_type = adv9880_configuration[ch_id].input[inputidx].input_type;
             
          if ( input_type == ADV9880_DIGITAL_INPUT ) {
#if 0
               unsigned char val;

               err = adv9880_i2c_read_reg(ch_client,
                                          0x87,
                                          &val);
                  
               if ( err == 0 && val != 0 ) {
                    err = adv9880_i2c_write_reg(ch_client,
                                                0x7f,
                                                0x00);
               }
#else
               err = adv9880_i2c_write_reg(ch_client,
                                           0x7f,
                                           0x00);
#endif
          }
     }
}

static int adv9880_set_input_mux(unsigned char channel)
{
     (void) channel;

     return 0;
}

/* adv9880_initialize :
 * This function will set the video format standard
 */
static int adv9880_initialize(struct v4l2_subdev *sd)
{
	int err = 0;
	int ch_id;
	v4l2_std_id std;
	struct i2c_client *ch_client = NULL;
	int index;
        struct adv9880_channel *channel = (to_adv9880(sd));

        ch_id = (to_adv9880(sd))->ch_id;
	ch_client = v4l2_get_subdevdata(sd);

	dev_dbg(&ch_client->dev, "Adv9880 driver registered\n");
	/*Configure the ADV9880 in default 720p 60 Hz standard for normal
	   power up mode */

#if defined(CONFIG_DAVINCI_DM368_FPGA)
        dm368_fpga_vinsel_hdmi();
#endif

	/* Reset the chip */
	err |= adv9880_i2c_write_reg(ch_client, ADV9880_POWER_CONTROL,
				     0xff);
        if ( 0 == err ) {
             msleep(10);

             err |= adv9880_i2c_write_reg(ch_client, ADV9880_POWER_CONTROL,
                                          0x0e);

             msleep(10);
        }


        if ( 0 == err ) {
             err |= adv9880_i2c_write_reg(ch_client, ADV9880_HPLL_DIVIDER_MSB,
                                          ADV9880_HPLL_MSB_DEFAULT);
        }
        if ( 0 == err ) {
             err |= adv9880_i2c_write_reg(ch_client, ADV9880_HPLL_DIVIDER_LSB,
                                          ADV9880_HPLL_LSB_DEFAULT);
        }
        if ( 0 == err ) {
             err |= adv9880_i2c_write_reg(ch_client, ADV9880_HPLL_CONTROL,
                                          ADV9880_HPLL_CONTROL_DEFAULT);
        }
        if ( 0 == err ) {
             err |= adv9880_i2c_write_reg(ch_client, ADV9880_HPLL_PHASE_SELECT,
				     ADV9880_HPLL_PHASE_SEL_DEFAULT);
        }
        if ( 0 == err ) {
             err |= adv9880_i2c_write_reg(ch_client, ADV9880_CLAMP_START,
                                          ADV9880_CLAMP_START_DEFAULT);
        }
        if ( 0 == err ) {
             err |= adv9880_i2c_write_reg(ch_client, ADV9880_CLAMP_WIDTH,
                                          ADV9880_CLAMP_WIDTH_DEFAULT);
        }
        if ( 0 == err ) {
             err |=
                  adv9880_i2c_write_reg(ch_client, ADV9880_SYNC_SEPARATER_THLD,
                                        ADV9880_SYNC_SEP_THLD_DEFAULT);
        }
        if ( 0 == err ) {
             err |=
                  adv9880_i2c_write_reg(ch_client, ADV9880_HPLL_PRE_COAST,
                                        ADV9880_HPLL_PRE_COAST_DEFAULT);
        }
        if ( 0 == err ) {
             err |=
                  adv9880_i2c_write_reg(ch_client, ADV9880_HPLL_POST_COAST,
                                        ADV9880_HPLL_POST_COAST_DEFAULT);
        }
        if ( 0 == err ) {
             err |=
                  adv9880_i2c_write_reg(ch_client, ADV9880_OUTPUT_MODE_CONTROL,
                                        ADV9880_OUTPUT_MODE_CONTROL_DEFAULT);
        }
        if ( 0 == err ) {
             err |=
                  adv9880_i2c_write_reg(ch_client, ADV9880_SYNC_FILTER_CONTROL,
                                        ADV9880_SYNC_FILTER_CONTROL_DEFAULT);
        }
        if ( 0 == err ) {
             err |=
                  adv9880_i2c_write_reg(ch_client, ADV9880_CLAMP_CONTROL,
                                        ADV9880_CLAMP_CONTROL_DEFAULT);
        }
        if ( 0 == err ) {
             err |=
                  adv9880_i2c_write_reg(ch_client, ADV9880_AUTO_OFFSET_CONTROL,
                                        ADV9880_AUTO_OFFSET_CONTROL_DEFAULT);
        }
        if ( 0 == err ) {
             err |=
                  adv9880_i2c_write_reg(ch_client, ADV9880_GREEN_OFFSET,
                                        ADV9880_GREEN_OFFSET_DEFAULT);
        }
        if ( 0 == err ) {
             err |= 
                  adv9880_i2c_write_reg(ch_client, ADV9880_MISC_CONTROL_1,
                                        ADV9880_MISC_CONTROL_1_DEFAULT);
        }
        if ( 0 == err ) {
             err |= 
                  adv9880_i2c_write_reg(ch_client, ADV9880_OUTPUT_SYNC_CONTROL,
                                        ADV9880_OUTPUT_SYNC_CONTROL_DEFAULT);
        }

        if ( 0 == err ) {
             err |= 
                  adv9880_i2c_write_reg(ch_client, ADV9880_I2S_CONTROL,
                                        ADV9880_I2S_CONTROL_DEFAULT);
        }
        if ( 0 == err ) {
             err |= 
                  adv9880_i2c_write_reg(ch_client, ADV9880_HSYNC_DURATION,
                                        ADV9880_HSYNC_DURATION_DEFAULT);
        }

        if ( 0 == err ) {
             err |=
                  adv9880_i2c_write_reg(ch_client, ADV9880_INPUT_SELECT,
                                        ADV9880_INPUT_SELECT_DEFAULT );
        }

	if (err < 0) {
		err = -EINVAL;
		adv9880_deinitialize(sd);
		return err;
	} else {

		memcpy(&channel->params,
		       &adv9880_configuration[ch_id].def_params,
		       sizeof(struct adv9880_params));
		/* Configure for default video standard */
		/* call set standard */
		index = channel->params.inputidx;
		std = adv9880_configuration[ch_id].input[index].def_std;
		err |= adv9880_s_std(sd, std);

		if (err < 0) {
			err = -EINVAL;
			adv9880_deinitialize(sd);
			return err;
		}
	}

	dev_dbg(&ch_client->dev, "End of adv9880_init.\n");
	return err;
}

static int adv9880_deinitialize(struct v4l2_subdev *sd)
{
     
     (void) sd;

     return 0;
}



/* adv9880_setstd :
 * Function to set the video standard
 */
static int adv9880_s_std(struct v4l2_subdev *sd, v4l2_std_id std)
{
	int err = 0;
	struct adv9880_format_params *adv9880formats;
	int ch_id;
	int i = 0;
	struct v4l2_standard *standard;
	int input_idx;
        int input_type;
        struct adv9880_channel *channel = to_adv9880(sd);
        struct i2c_client *ch_client = v4l2_get_subdevdata(sd);
 
        ch_id = (channel)->ch_id;

	dev_dbg(&ch_client->dev, "Start of adv9880_setstd..\n");
	input_idx = channel->params.inputidx;
        input_type = adv9880_configuration[ch_id].input[input_idx].input_type;
             
	for (i = 0; i < adv9880_configuration[ch_id].input[input_idx].
	     no_of_standard; i++) {
		standard = &adv9880_configuration[ch_id].input[input_idx].
		    standard[i];
		if (standard->id & std) {
			break;
		}
	}
	if (i == adv9880_configuration[ch_id].input[input_idx].no_of_standard) {
		dev_err(&ch_client->dev, "Invalid id...\n");
		return -EINVAL;
	}

       
        /* Horizontal alignment of video */
        switch ( input_type )  {
        case ADV9880_ANALOG_INPUT:
#if defined(CONFIG_DAVINCI_DM368_FPGA)
             /* Set audio path */
             dm368_fpga_audio_select( DM368_FPGA_AUDIO_TRISTATE );
#endif
             /* Disable CSC (assume YPbPr) */
             dev_dbg(&ch_client->dev, "Disable CSC analog input\n" );
             err |= adv9880_i2c_write_reg(ch_client,  ADV9880_COLOR_CONVERT_CONTROL, 0x80 );

#if 0
             /* Flip HSYNC polarity to account for internal video data delay vs. HSYNC */

             switch ( std ) {
#if 1
             case V4L2_STD_720P_60:
             case V4L2_STD_720P_50:
                  dev_dbg( &ch_client->dev,
                       "720p adjust analog\n");
                  adv9880_i2c_write_reg(ch_client,
                                        ADV9880_OUTPUT_SYNC_CONTROL,
                                        ADV9880_OUTPUT_SYNC_CONTROL_DEFAULT^ADV9880_OUTPUT_SYNC_CONTROL_HSYNC_POL_MASK);

                  adv9880_i2c_write_reg(ch_client,
                                        ADV9880_HSYNC_DURATION,
                                        0x26);
                  break;

             case V4L2_STD_1080I_60:
             case V4L2_STD_1080I_50:
                  dev_dbg( &ch_client->dev,
                       "1080i adjust analog\n");
                  adv9880_i2c_write_reg(ch_client,
                                        ADV9880_OUTPUT_SYNC_CONTROL,
                                        ADV9880_OUTPUT_SYNC_CONTROL_DEFAULT^ADV9880_OUTPUT_SYNC_CONTROL_HSYNC_POL_MASK);

                  adv9880_i2c_write_reg(ch_client,
                                        ADV9880_HSYNC_DURATION,
                                        0x2c);
                  break;
#endif
             default:
                  adv9880_i2c_write_reg(ch_client,
                                        ADV9880_OUTPUT_SYNC_CONTROL,
                                        ADV9880_OUTPUT_SYNC_CONTROL_DEFAULT);

                  adv9880_i2c_write_reg(ch_client,
                                        ADV9880_HSYNC_DURATION,
                                        ADV9880_HSYNC_DURATION_DEFAULT);
                  break;
             }
#else
                  adv9880_i2c_write_reg(ch_client,
                                        ADV9880_OUTPUT_SYNC_CONTROL,
                                        ADV9880_OUTPUT_SYNC_CONTROL_DEFAULT);

                  adv9880_i2c_write_reg(ch_client,
                                        ADV9880_HSYNC_DURATION,
                                        ADV9880_HSYNC_DURATION_DEFAULT);
#endif
             break;

        case ADV9880_DIGITAL_INPUT:

#if defined(CONFIG_DAVINCI_DM368_FPGA)
             dm368_fpga_audio_select( DM368_FPGA_AUDIO_HDMI );
#endif
             err = adv9880_set_hdmi_colorspace(ch_client);

             adv9880_i2c_write_reg(ch_client,
                                   ADV9880_OUTPUT_SYNC_CONTROL,
                                   ADV9880_OUTPUT_SYNC_CONTROL_DEFAULT);
             adv9880_i2c_write_reg(ch_client,
                                   ADV9880_HSYNC_DURATION,
                                   ADV9880_HSYNC_DURATION_DEFAULT);
             break;

        }

	adv9880formats =
	    &adv9880_configuration[ch_id].input[input_idx].format[i];

	err = adv9880_set_format_params(sd, adv9880formats);
	if (err < 0) {
		dev_err(&ch_client->dev, "Set standard failed\n");
		return err;
	}

	/* Lock the structure variable and assign std to the member
	   variable */
	channel->params.std = std;

	dev_dbg(&ch_client->dev, "End of adv9880 set standard...\n");
	return err;
}

#ifdef DEBUG
static void adv9880_dump_register(struct v4l2_subdev *sd)
{
	int i = 0, err;
	u8 val;
        struct i2c_client *ch_client = v4l2_get_subdevdata(sd);

	for (i = 0; i <= ADV9880_SCREEN_HEIGHT_LSB; i++) {
		err = adv9880_i2c_read_reg(ch_client,
				   i, &val);
		printk(KERN_NOTICE "reg %x, val = %x, err = %x\n", i, val, err);
	}
}
#endif

int
adv9880_set_hdmi_colorspace(struct i2c_client *ch_client)
{
     int err = 0;
     unsigned char avinfo1_val=ADV9880_AVI_INFOFRAME_1_CSIN_YCBCR422;
     unsigned char dvi_hdmi_val=0;

     err = adv9880_i2c_read_reg(ch_client, ADV9880_AVI_INFOFRAME_1, &avinfo1_val );
     
     if ( err != 0 ) {
          dev_dbg(&ch_client->dev, "set_hdmi_colorspace: Error reading AVI_INFOFRAME_1\n" );

          return err;
     }

     err = adv9880_i2c_read_reg(ch_client, ADV9880_HDMI_MODE, &dvi_hdmi_val);
     if ( err != 0 ) {
          dev_dbg(&ch_client->dev, "set_hdmi_colorspace: Error reading HDMI_MODE\n" );

          return err;
     }

     if ( ((avinfo1_val&ADV9880_AVI_INFOFRAME_1_CSIN_MASK) == ADV9880_AVI_INFOFRAME_1_CSIN_RGB)
          || ((dvi_hdmi_val&ADV9880_HDMI_MODE_DVI_HDMI_MASK) == ADV9880_HDMI_MODE_DVI) ) {

          dev_dbg(&ch_client->dev, "set_hdmi_colorspace: RGB\n" );

          err = adv9880_i2c_write_reg(ch_client,  0x35, 0x07 );
          err |= adv9880_i2c_write_reg(ch_client,  0x36, 0x06 );
          err |= adv9880_i2c_write_reg(ch_client,  0x37, 0x19 );
          err |= adv9880_i2c_write_reg(ch_client,  0x38, 0xa0 );
          err |= adv9880_i2c_write_reg(ch_client,  0x39, 0x1f );
          err |= adv9880_i2c_write_reg(ch_client,  0x3a, 0x5b );
          
          err |= adv9880_i2c_write_reg(ch_client,  0x3b, 0x08 );
          err |= adv9880_i2c_write_reg(ch_client,  0x3c, 0x00 );
          
          err |= adv9880_i2c_write_reg(ch_client,  0x3d, 0x02 );
          err |= adv9880_i2c_write_reg(ch_client,  0x3e, 0xed );
          err |= adv9880_i2c_write_reg(ch_client,  0x3f, 0x09 );
          err |= adv9880_i2c_write_reg(ch_client,  0x40, 0xd3 );
          err |= adv9880_i2c_write_reg(ch_client,  0x41, 0x00 );
          err |= adv9880_i2c_write_reg(ch_client,  0x42, 0xfd );

          err |= adv9880_i2c_write_reg(ch_client,  0x43, 0x01 );
          err |= adv9880_i2c_write_reg(ch_client,  0x44, 0x00 );
          
          err |= adv9880_i2c_write_reg(ch_client,  0x45, 0x1e );
          err |= adv9880_i2c_write_reg(ch_client,  0x46, 0x64 );
          err |= adv9880_i2c_write_reg(ch_client,  0x47, 0x1a );
          err |= adv9880_i2c_write_reg(ch_client,  0x48, 0x96 );
          err |= adv9880_i2c_write_reg(ch_client,  0x49, 0x07 );
          err |= adv9880_i2c_write_reg(ch_client,  0x4a, 0x06 );
          
          err |= adv9880_i2c_write_reg(ch_client,  0x4b, 0x08 );
          err |= adv9880_i2c_write_reg(ch_client,  0x4c, 0x00 );

          err |= adv9880_i2c_write_reg(ch_client,  ADV9880_COLOR_CONVERT_CONTROL, 0x82 );
     } else {
          dev_dbg(&ch_client->dev, 
                  "set_hdmi_colorspace: YPbPr: avinfo_val 0x%x\n", 
                  (unsigned int)avinfo1_val );

          err |= adv9880_i2c_write_reg(ch_client,  ADV9880_COLOR_CONVERT_CONTROL, 0x80 );
     }

     return err;
}

/* adv9880_querystd :
 * Function to return standard detected by decoder
 */
static int adv9880_querystd(struct v4l2_subdev *sd, v4l2_std_id *id)
{
	int err = 0;
	unsigned char val;
	unsigned short val1;
        unsigned short val2;
        unsigned char  cea861VidId = 0;
	int ch_id;
        int detected = 0;
        int gotformat = 0;
        unsigned char sync_status=0;
        unsigned char hdmi_status=0;
        int input_type;
        int inputidx;
        int formatretries = 4;
        struct i2c_client *ch_client = v4l2_get_subdevdata(sd);
        struct adv9880_channel *channel = to_adv9880(sd);
        unsigned int fps_1000 = 0;

        v4l2_std_id cea861Std = 0;

	dev_dbg(&ch_client->dev, "Starting querystd function...\n");
	if (id == NULL) {
		dev_err(&ch_client->dev, "NULL Pointer.\n");
		return -EINVAL;
	}

	msleep(100);

#ifdef DEBUG
//	adv9880_dump_register(sd);
#endif

        ch_id = channel->ch_id;

        inputidx = channel->params.inputidx;
        input_type = adv9880_configuration[ch_id].input[inputidx].input_type;
        switch (input_type) {
        case ADV9880_ANALOG_INPUT:
             /* Query the sync status */
             err = adv9880_i2c_read_reg(ch_client,
                                        ADV9880_SYNC_DETECT_2, &sync_status);
             if (err < 0) {
                  dev_err(&ch_client->dev,
                          "I2C read fails...sync detect\n");
                  return err;
             }
             
             
             if ( (sync_status & ADV9880_SYNC_DETECT_2_SYNC_LOCK_MASK) == ADV9880_SYNC_DETECT_2_SYNC_LOCK_MASK ) {
                  detected = 1;
             }
             break;

        case ADV9880_DIGITAL_INPUT:
        default:
             /* Query the sync status */
             err = adv9880_i2c_read_reg(ch_client,
                                        ADV9880_HDMI_STATUS_1, &hdmi_status);
             if (err < 0) {
                  dev_err(&ch_client->dev,
                          "I2C read fails...hdmi status\n");
                  return err;
             }
             
             if ( (hdmi_status & ADV9880_HDMI_STATUS_1_DE_MASK ) == ADV9880_HDMI_STATUS_1_DE_MASK ) {
                  detected = 1;                 
             }
             break;
        }
        

        if ( !detected ) {
             dev_dbg( &ch_client->dev, "No sync detected\n");
             return -EIO;
        }


        do { 
             val1 = 0;
             /* Query the standards */
             err = adv9880_i2c_read_reg(ch_client,
                                        ADV9880_LINES_PER_VSYNC_STATUS_HIGH, &val);
             if (err < 0) {
                  dev_err(&ch_client->dev,
                          "I2C read fails...Lines per frame high\n");
                  return err;
             }

             val1 |= (val << LINES_PER_VSYNC_MSB_SHIFT) & LINES_PER_VSYNC_MSB_MASK;

             err = adv9880_i2c_read_reg(ch_client,
                                        ADV9880_LINES_PER_VSYNC_STATUS_LOW, &val);
             if (err < 0) {
                  dev_err(&ch_client->dev,
                          "I2C read fails...Lines per frame low\n");
                  return err;
             }
             val1 |= (val&0xff);


             fps_1000 = 0;
             val2     = 0;
#if defined(CONFIG_DAVINCI_DM368_FPGA)
             val2 = dm368_fpga_hdmi_hsync_counter();

             if ( val1 != 0 && val2 != 0 ) {
                  fps_1000 = 27000000 / val1;
                  fps_1000 *= 1000;
                  fps_1000 /= val2;
             }
#else
             if ( input_type == ADV9880_DIGITAL_INPUT ) {
                  err = adv9880_i2c_read_reg(ch_client,
                                             ADV9880_AVI_INFOFRAME_VIDEO_ID, &cea861VidId);
                  if ( err < 0) {
                       dev_err( &ch_client->dev,
                                "I2C read fails...AVI Infoframe video ID\n" );
                       return err;
                  }

                  cea861VidId &= 0x7F; // only bits 6:0 are valid

                  cea861Std = cea861_stds[cea861VidId];
                  fps_1000  = cea861_fps_1000[cea861VidId];

                  if ( fps_1000 == 0 ) {
                       fps_1000 = 60000;
                  }

                  if (fps_1000 != 0 && val1 != 0 )
                       val2 = ((27000000/val1)*1000)/fps_1000;
             }


#endif

             if ( ADV9880_LINES_1080P_LOWER <= val1
                  && val1 <= ADV9880_LINES_1080P_UPPER  ) {

                  if ( fps_1000 >= 24800 && fps_1000 <= 25200 ) {
                       *id = V4L2_STD_1080P_25;
                       gotformat = 1;
                  } 
                  else if ( (fps_1000 >= 29700 && fps_1000 <= 30300) ) {
                       *id = V4L2_STD_1080P_30;
                       gotformat = 1;
                  }
                  else if ( fps_1000 >= 49500 && fps_1000 <= 50500 ) {
                       *id = V4L2_STD_1080P_50;
                       gotformat = 1;
                  }
                  else if ( fps_1000 >= 59400 && fps_1000 <= 60600 ) {
                       *id = V4L2_STD_1080P_60;
                       gotformat = 1;
                  }
                  else if ( fps_1000 >= 23700 && fps_1000 <= 24300 ) {
                       *id = V4L2_STD_1080P_24;
                       gotformat = 1;
                  }
             } 
             if ( ADV9880_LINES_1080I_60_LOWER <= val1
                  && val1 <= ADV9880_LINES_1080I_60_UPPER  
                  ) {
                  
                  if ( 0 == val2 || (0x310 <= val2 && val2 <= 0x330) ) {
                       *id = V4L2_STD_1080I_60;
                       gotformat = 1;
                  } else if ( 0x3b0 <= val2 && val2 <= 0x3d0 )  {
                       *id = V4L2_STD_1080I_50;
                       gotformat = 1;
                  }
             } 
             else if (ADV9880_LINES_720_LOWER <= val1 &&
                      val1 <= ADV9880_LINES_720_UPPER) {
                  
                  if ( 0 == val2 || (0x248 <= val2 && val2 <= 0x268) ) {
                       *id = V4L2_STD_720P_60;
                       gotformat = 1;
                  } 
                  else if ( (0x2c0 <= val2) && (val2 <= 0x2e0) ) {
                       *id = V4L2_STD_720P_50;
                       gotformat = 1;
                  }
             }
             else if ((524 == val1) || (525 == val1)) { // VESA mode has one less line
                  *id = V4L2_STD_525P_60;
                  gotformat = 1;
             }
             else if ((624==val1)||(625 == val1)) {
                  *id = V4L2_STD_625P_50;
                  gotformat = 1;
             }
             else if ((262==val1) || (263==val1)) {
                  *id = V4L2_STD_525_60;
                  gotformat = 1;
             }
             else if ((312==val1) || (313==val1)) {
                  *id = V4L2_STD_625_50;
                  gotformat = 1;
             }


             if ( !gotformat ) {
                  /* VSYNC ctr may take some time to converge */
                  msleep(50);
             }

        } while ( gotformat == 0 && --formatretries > 0 ) ;

	dev_notice(&ch_client->dev,
		   "ADV9880 - lines per frame detected = %d 27mhz clocks per line = %d fps=%u.%03u\n", (int)val1, (int)val2, fps_1000/1000, fps_1000%1000);

        if ( !gotformat ) {
		dev_notice(&ch_client->dev,
			"querystd, error, val = %x, lpf = %x, clkperline = %x\n", val, (int)val1, val2);
                return -EINVAL;
	}

	if ( V4L2_STD_720P_60 == *id )
	  {
	    if ( val1  == ADV9880_LINES_720_UPPER) 
	      {
		//printk ( KERN_ERR "STANDARD 720P\n" );
		adv9880_formats[0].avid_start_lsb = AVID_START_PIXEL_LSB_720p;
		adv9880_formats[0].avid_stop_lsb = AVID_STOP_PIXEL_LSB_720p;
		
		adv9880_formats[0].clamp_start = ADV9880_HD_CLAMP_START;
		adv9880_formats[0].clamp_width = ADV9880_HD_CLAMP_WIDTH;
		adv9880_formats[0].hpll_pre_coast = ADV9880_HD_PRE_COAST;
		adv9880_formats[0].hpll_post_coast = ADV9880_HD_POST_COAST;
	      }
	    else
	      {
		//printk ( KERN_ERR "VESA 720P\n" );

		/* VESA uses bi-level sync, so low pulse is twice as long */
		adv9880_formats[0].avid_start_lsb = AVID_START_PIXEL_LSB_720p - 44;
		adv9880_formats[0].avid_stop_lsb = AVID_STOP_PIXEL_LSB_720p - 44;
		
		/* VESA uses bi-level sync, so set up as for ED (bi-level) syncs */
		adv9880_formats[0].clamp_start = ADV9880_ED_CLAMP_START;
		adv9880_formats[0].clamp_width = ADV9880_ED_CLAMP_WIDTH;
		adv9880_formats[0].hpll_pre_coast = ADV9880_ED_PRE_COAST;
		adv9880_formats[0].hpll_post_coast = ADV9880_ED_POST_COAST;
	      }
	  }

	channel->params.std = *id;
//	err = adv9880_setstd(id, dec);
	dev_dbg(&ch_client->dev, "End of querystd function.\n");
	return err;
}


/**
 * adv9880_s_routing() - V4L2 decoder interface handler for s_routing
 * @sd: pointer to standard V4L2 sub-device structure
 * @input: input selector for routing the signal
 * @output: output selector for routing the signal
 * @config: config value. Not used
 *
 * If index is valid, selects the requested input. Otherwise, returns -EINVAL if
 * the input is not supported
 */
static int adv9880_s_routing(struct v4l2_subdev *sd,
                             u32 input, u32 output, u32 config)
{
     struct i2c_client *ch_client = v4l2_get_subdevdata(sd);
     struct adv9880_channel *channel = to_adv9880(sd);
     int err = 0;
     int ch_id = channel->ch_id;
     u8 input_sel;
     u8 input_sel_current;
     u8 input_sel_new;

     dev_dbg(&ch_client->dev, "Start of s_routing function.\n");

     input_sel = adv9880_configuration[ch_id].input[input].input_type;
       

     err = adv9880_i2c_read_reg(ch_client,
                                ADV9880_INPUT_SELECT,
                                &input_sel_current);
     if ( 0 != ((input_sel ^ input_sel_current) & ADV9880_INPUT_SELECT_INTERFACE_MASK) ) {

          input_sel_new = input_sel_current & ~ADV9880_INPUT_SELECT_INTERFACE_MASK;
          input_sel_new |= (input_sel & ADV9880_INPUT_SELECT_INTERFACE_MASK);
          input_sel_new |= ADV9880_INPUT_SELECT_MANUAL_MASK;
          adv9880_i2c_write_reg(ch_client,
                                ADV9880_INPUT_SELECT,
                                input_sel_new );
         
          /* Wait for sync */
          msleep( 50 );
     }
       
     channel->params.inputidx = input;

out:

     dev_dbg(&ch_client->dev, "End of set input function.\n");
     return err;

}



/* adv9880_enumstd : Function to enumerate standards supported
 */
/* static int adv9880_enumstd(struct v4l2_standard *std, void *dec) */
/* { */
/* 	int index, index1; */
/* 	int err = 0; */
/* 	int ch_id; */
/* 	int input_idx, sumstd = 0; */
/* 	if (NULL == dec) { */
/* 		printk(KERN_ERR "NULL Pointer\n"); */
/* 		return -EINVAL; */
/* 	} */
/* 	ch_id = ((struct decoder_device *)dec)->channel_id; */
/* 	if (std == NULL) { */
/* 		dev_err(&ch_client->dev, "NULL Pointer.\n"); */
/* 		return -EINVAL; */
/* 	} */
/* 	index = std->index; */
/* 	index1 = index; */
/* 	/\* Check for valid value of index *\/ */
/* 	for (input_idx = 0; */
/* 	     input_idx < adv9880_configuration[ch_id].no_of_inputs; */
/* 	     input_idx++) { */
/* 		sumstd += adv9880_configuration[ch_id].input[input_idx] */
/* 		    .no_of_standard; */
/* 		if (index < sumstd) { */
/* 			sumstd -= adv9880_configuration[ch_id] */
/* 			    .input[input_idx].no_of_standard; */
/* 			break; */
/* 		} */
/* 	} */
/* 	if (input_idx == adv9880_configuration[ch_id].no_of_inputs) */
/* 		return -EINVAL; */
/* 	index -= sumstd; */

/* 	memset(std, 0, sizeof(*std)); */

/* 	memcpy(std, &adv9880_configuration[ch_id].input[input_idx]. */
/* 	       standard[index], sizeof(struct v4l2_standard)); */
/* 	std->index = index1; */
/* 	return err; */
/* } */

/* adv9880_setinput :
 * Function to set the input
 */
/* static int adv9880_setinput(int *index, void *dec) */
/* { */
/* 	int err = 0; */
/* 	int ch_id; */
/*         u8 input_sel; */
/*         u8 input_sel_current; */
/*         u8 input_sel_new; */

/* 	if (NULL == dec) { */
/* 		printk(KERN_ERR "NULL Pointer\n"); */
/* 		return -EINVAL; */
/* 	} */
/* 	ch_id = ((struct decoder_device *)dec)->channel_id; */
/* 	dev_dbg(&ch_client->dev, "Start of set input function.\n"); */

/* 	/\* check for null pointer *\/ */
/* 	if (index == NULL) { */
/* 		dev_err(&ch_client->dev, "NULL Pointer.\n"); */
/* 		return -EINVAL; */
/* 	} */
/* 	if ((*index >= adv9880_configuration[ch_id].no_of_inputs) */
/* 	    || (*index < 0)) { */
/* 		return -EINVAL; */
/* 	} */


/*         input_sel = adv9880_configuration[ch_id].input[*index].input_type; */

       
/*         err = adv9880_i2c_read_reg(ch_client, */
/*                                    ADV9880_INPUT_SELECT, */
/*                                    &input_sel_current); */
/*         if ( 0 != ((input_sel ^ input_sel_current) & ADV9880_INPUT_SELECT_INTERFACE_MASK) ) { */

/*              input_sel_new = input_sel_current & ~ADV9880_INPUT_SELECT_INTERFACE_MASK; */
/*              input_sel_new |= (input_sel & ADV9880_INPUT_SELECT_INTERFACE_MASK); */
/*              input_sel_new |= ADV9880_INPUT_SELECT_MANUAL_MASK; */
/*              adv9880_i2c_write_reg(ch_client, */
/*                                    ADV9880_INPUT_SELECT, */
/*                                    input_sel_new ); */
         
/*              /\* Wait for sync *\/ */
/*              msleep( 50 ); */
/*         } */
       
/*         channel->params.inputidx = *index; */

/* 	dev_dbg(&ch_client->dev, "End of set input function.\n"); */
/* 	return err; */
/* } */

/* adv9880_getinput : Function to get the input
 */
/* static int adv9880_getinput(int *index, void *dec) */
/* { */
/* 	int err = 0; */
/* 	int ch_id; */
/* 	v4l2_std_id id; */
/* 	if (NULL == dec) { */
/* 		printk(KERN_ERR "NULL Pointer\n"); */
/* 		return -EINVAL; */
/* 	} */
/* 	ch_id = ((struct decoder_device *)dec)->channel_id; */
/* 	dev_dbg(&ch_client->dev, "Start of get input function.\n"); */

/* 	/\* check for null pointer *\/ */
/* 	if (index == NULL) { */
/* 		dev_err(&ch_client->dev, "NULL Pointer.\n"); */
/* 		return -EINVAL; */
/* 	} */
/* 	err |= adv9880_querystd(&id, dec); */
/* 	if (err < 0) { */
/* 		return err; */
/* 	} */
/* 	*index = 0; */
/* 	*index = channel->params.inputidx; */
/* 	dev_dbg(&ch_client->dev, "End of get input function.\n"); */
/* 	return err; */
/* } */

/* adv9880_enuminput :
 * Function to enumerate the input
 */
/* static int adv9880_enuminput(struct v4l2_input *input, void *dec) */
/* { */
/* 	int err = 0; */
/* 	int index = 0; */
/* 	int ch_id; */
/* 	if (NULL == dec) { */
/* 		printk(KERN_ERR "NULL Pointer.\n"); */
/* 		return -EINVAL; */
/* 	} */
/* 	ch_id = ((struct decoder_device *)dec)->channel_id; */

/* 	/\* check for null pointer *\/ */
/* 	if (input == NULL) { */
/* 		dev_err(&ch_client->dev, "NULL Pointer.\n"); */
/* 		return -EINVAL; */
/* 	} */

/* 	/\* Only one input is available *\/ */
/* 	if (input->index >= adv9880_configuration[ch_id].no_of_inputs) { */
/* 		return -EINVAL; */
/* 	} */
/* 	index = input->index; */
/* 	memset(input, 0, sizeof(*input)); */
/* 	input->index = index; */
/* 	memcpy(input, */
/* 	       &adv9880_configuration[ch_id].input[index].input_info, */
/* 	       sizeof(struct v4l2_input)); */
/* 	return err; */
/* } */

/* adv9880_set_format_params :
 * Function to set the format parameters
 */
static int adv9880_set_format_params(struct v4l2_subdev *sd, struct adv9880_format_params *tvpformats)
{
	int err = 0;
	unsigned char val;
	int ch_id;
        u8 vs_delay_reg;
        u16 line_start, line_end, line_width;
	struct i2c_client *ch_client = NULL;
        struct adv9880_channel *channel = (to_adv9880(sd));
        unsigned char vs_start;

	ch_client = v4l2_get_subdevdata(sd);

	ch_id = channel->ch_id;

	dev_dbg(&ch_client->dev,
		"Adv9880 set format params started...\n");
	if (tvpformats == NULL) {
		dev_err(&ch_client->dev, "NULL Pointer.\n");
		return -EINVAL;
	}

	/* Write the HPLL related registers */
	err = adv9880_i2c_write_reg(ch_client, ADV9880_HPLL_DIVIDER_MSB,
				    tvpformats->hpll_divider_msb);
	if (err < 0) {
		dev_err(&ch_client->dev,
			"I2C write fails...Divider MSB\n");
		return err;
	}

	val = ((tvpformats->
		hpll_divider_lsb & HPLL_DIVIDER_LSB_MASK) <<
	       HPLL_DIVIDER_LSB_SHIFT);
	err =
	    adv9880_i2c_write_reg(ch_client, ADV9880_HPLL_DIVIDER_LSB, val);
	if (err < 0) {
		dev_err(&ch_client->dev,
			"I2C write fails...Divider LSB.\n");
		return err;
	}

	err = adv9880_i2c_write_reg(ch_client, ADV9880_HPLL_CONTROL,
				    tvpformats->hpll_control);
	err = adv9880_i2c_write_reg(ch_client, ADV9880_CLAMP_START,
				    tvpformats->clamp_start);
	err = adv9880_i2c_write_reg(ch_client, ADV9880_CLAMP_WIDTH,
				    tvpformats->clamp_width);
	err = adv9880_i2c_write_reg(ch_client, ADV9880_HPLL_PRE_COAST,
				    tvpformats->hpll_pre_coast);
	err = adv9880_i2c_write_reg(ch_client, ADV9880_HPLL_POST_COAST,
				    tvpformats->hpll_post_coast);

        /* Set BT.656 size parameters */
        vs_start = tvpformats->vblk_start_f0_line_offset + tvpformats->vblk_f0_duration; 

        vs_delay_reg = ((vs_start)&0x3f)<<2;
        vs_delay_reg |= (tvpformats->avid_start_msb & 0x3);

	err = adv9880_i2c_write_reg(ch_client, ADV9880_VS_DELAY,
				    vs_delay_reg);
        
        err = adv9880_i2c_write_reg(ch_client, ADV9880_HS_DELAY,
				    (tvpformats->avid_start_lsb & 0xfe));

        line_start = tvpformats->avid_start_msb;
        line_start <<= 8;
        line_start |= tvpformats->avid_start_lsb;

        line_end = tvpformats->avid_stop_msb;
        line_end <<= 8;
        line_end |= tvpformats->avid_stop_lsb;

        line_width = (line_end-line_start)-4;

        if ( tvpformats->vblk_f1_duration != 0 ) {
             err = adv9880_i2c_write_reg(ch_client, ADV9880_MISC_CONTROL_1,
                                         ADV9880_MISC_CONTROL_1_DEFAULT | 0x00 /* Netra ACTVID does not like =1 ??*/ );
        } else {
             err = adv9880_i2c_write_reg(ch_client, ADV9880_MISC_CONTROL_1,
                                         ADV9880_MISC_CONTROL_1_DEFAULT | 0x00  );
        }

        err = adv9880_i2c_write_reg(ch_client, ADV9880_LINE_WIDTH_MSB,
				    (line_width>>8)&0xf );

        err = adv9880_i2c_write_reg(ch_client, ADV9880_LINE_WIDTH_LSB,
				    (line_width)&0xff );

        err = adv9880_i2c_write_reg(ch_client, ADV9880_SCREEN_HEIGHT_MSB,
				    tvpformats->screen_height_msb );

        err = adv9880_i2c_write_reg(ch_client, ADV9880_SCREEN_HEIGHT_LSB,
				    tvpformats->screen_height_lsb );

        err = adv9880_i2c_write_reg(ch_client, ADV9880_SYNC_FILTER_CONTROL,
				    tvpformats->sync_filter_control );

        err = adv9880_i2c_write_reg(ch_client, ADV9880_VSYNC_DURATION,
				    tvpformats->vsync_duration );


	channel->params.format = *tvpformats;

	dev_dbg(&ch_client->dev,
		"End of adv9880 set format params...\n");
	return err;
}


/* adv9880_i2c_read_reg :This function is used to read value from register
 * for i2c client.
 */
static int adv9880_i2c_read_reg(struct i2c_client *client, u8 reg, u8 * val)
{
	int err = 0;
        int retries = 5;

	struct i2c_msg msg[2];
	unsigned char writedata[1];
	unsigned char readdata[1];

	if (!client->adapter) {
		err = -ENODEV;
	} else {

             do { 
		msg[0].addr = client->addr;
		msg[0].flags = 0;
		msg[0].len = 1;
		msg[0].buf = writedata;
		writedata[0] = reg;

		msg[1].addr = client->addr;
		msg[1].flags = I2C_M_RD;
		msg[1].len = 1;
		msg[1].buf = readdata;

		err = i2c_transfer(client->adapter, msg, 2);
                if (err >= 2) {
                     *val = readdata[0];
		}
             } while ( err < 2 && --retries > 0);
	}
        if ( err < 0 ) {
             dev_err( &client->adapter->dev, "ADV9880: read x%02x failed\n", reg );
        } else {
             dev_dbg( &client->adapter->dev, "ADV9880: read x%02x val %02x\n", (int)reg, (int)(*val) );
        }
        
	return ((err < 0) ? err : 0);
}

/* adv9880_i2c_write_reg :This function is used to write value into register
 * for i2c client.
 */
static int adv9880_i2c_write_reg(struct i2c_client *client, u8 reg, u8 val)
{
	int err = 0;

        int retries = 3;

	struct i2c_msg msg[1];
	unsigned char data[2];
	if (!client->adapter) {
		err = -ENODEV;
	} else {

             
             do { 
		msg->addr = client->addr;
		msg->flags = 0;
		msg->len = 2;
		msg->buf = data;
		data[0] = reg;
		data[1] = val;
		err = i2c_transfer(client->adapter, msg, 1);
                if ( err < 0 ) {
                     msleep(10);
                }               
             } while ( err < 0 && --retries > 0);
	}

        if ( err < 0 
             && client->adapter != NULL ) {
             dev_err( &client->adapter->dev, 
                      "adv9880 i2c write failed: reg x%02x value x%02x\n", 
                      (unsigned int)reg,
                      (unsigned int)val
                  );
        }
        dev_dbg( &client->adapter->dev,"ADV9880: write x%02x val %02x\n", (int)reg, (int)val );

	return ((err < 0) ? err : 0);
}


/****************************************************************************
			I2C Client & Driver
 ****************************************************************************/

static int adv9880_probe(struct i2c_client *c,
			 const struct i2c_device_id *id)
{
	struct adv9880_channel *core;
	struct v4l2_subdev *sd;
        int ret;

	/* Check if the adapter supports the needed features */
	if (!i2c_check_functionality(c->adapter,
	     I2C_FUNC_SMBUS_READ_BYTE | I2C_FUNC_SMBUS_WRITE_BYTE_DATA))
		return -EIO;

	core = kzalloc(sizeof(struct adv9880_channel), GFP_KERNEL);
	if (!core) {
		return -ENOMEM;
	}

        core->params.inputidx = 1; //digital
        core->params.std = adv9880_configuration[0].input[core->params.inputidx].def_std;

	sd = &core->sd;
	v4l2_i2c_subdev_init(sd, c, &adv9880_ops);
	v4l_info(c, "chip found @ 0x%02x (%s)\n",
		 c->addr << 1, c->adapter->name);

	INIT_WORK(&core->work, adv9880_work);

        ret = adv9880_initialize(sd);
        if ( ret != 0 ) {
             v4l_err(c, "adv9880 init failed, code %d\n", ret );
             return ret;
        }

#if 0
        if ( adv9880_timer.function == NULL ) {
             init_timer(&adv9880_timer);
             adv9880_timer.function = adv9880_timer_function;
             adv9880_timer.data     = core;
        }
        mod_timer( &adv9880_timer, jiffies + msecs_to_jiffies( adv9880_timeout_ms ) );
#endif

	return ret;
}

static int adv9880_remove(struct i2c_client *c)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(c);

#ifdef DEBUG
 	v4l_info(c,
		"adv9880.c: removing adv9880 adapter on address 0x%x\n",
		c->addr << 1);
#endif
        del_timer_sync( &adv9880_timer );

	v4l2_device_unregister_subdev(sd);
//	kfree(to_adv9880(sd));
	return 0;
}

/* ----------------------------------------------------------------------- */

static const struct i2c_device_id adv9880_id[] = {
	{ "adv9880", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, adv9880_id);

static struct i2c_driver adv9880_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "adv9880",
	},
	.probe		= adv9880_probe,
	.remove		= adv9880_remove,
	.id_table	= adv9880_id,
};

static __init int init_adv9880(void)
{
	return i2c_add_driver(&adv9880_driver);
}

static __exit void exit_adv9880(void)
{
	i2c_del_driver(&adv9880_driver);
}

module_init(init_adv9880);
module_exit(exit_adv9880);

MODULE_DESCRIPTION("Analog Devices AD9880 video decoder driver");
MODULE_AUTHOR("John Whittington");
MODULE_LICENSE("GPL");
