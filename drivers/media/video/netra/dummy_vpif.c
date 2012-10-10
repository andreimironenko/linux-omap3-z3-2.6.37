/*
 * Dummy vpif - This code emulates a real video device with v4l2 api and real subdevs
 *
 * Copyright (c) 2011 Z3 Technology
 *      John Whittington <johnw@z3technology.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the BSD Licence, GNU General Public License
 * as published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version
 */
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/ioport.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/pci.h>
#include <linux/random.h>
#include <linux/version.h>
#include <linux/mutex.h>
#include <linux/videodev2.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/kthread.h>
#include <linux/highmem.h>
#include <linux/freezer.h>
#include <linux/i2c.h>

#include <media/videobuf-vmalloc.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-common.h>
#include <media/v4l2-chip-ident.h>
#include <media/tvp7002.h>

#include <mach/z3_fpga.h>
#include <mach/z3_app.h>

#include "../adv9880.h"

#define Z3NETRA_MODULE_NAME "z3netra"

#define NUM_INPUTS         8

#define MAX_SUBDEVS        8

#define VPIF_MAX_I2C_ADAPTERS 2

#define VIVI_MAJOR_VERSION 0
#define VIVI_MINOR_VERSION 6
#define VIVI_RELEASE 0
#define VIVI_VERSION \
	KERNEL_VERSION(VIVI_MAJOR_VERSION, VIVI_MINOR_VERSION, VIVI_RELEASE)

MODULE_DESCRIPTION("Video Technology Magazine Virtual Video Capture Board");
MODULE_AUTHOR("Mauro Carvalho Chehab, Ted Walther and John Sokol");
MODULE_LICENSE("Dual BSD/GPL");

static unsigned video_nr = 0;
module_param(video_nr, uint, 0644);
MODULE_PARM_DESC(video_nr, "videoX start number, -1 is autodetect");

static unsigned n_devs = 1;
module_param(n_devs, uint, 0644);
MODULE_PARM_DESC(n_devs, "number of video devices to create");

static unsigned debug = 1;
module_param(debug, uint, 0644);
MODULE_PARM_DESC(debug, "activates debug info");

static unsigned int vid_limit = 16;
module_param(vid_limit, uint, 0644);
MODULE_PARM_DESC(vid_limit, "capture memory limit in megabytes");


/* supported controls */
static struct v4l2_queryctrl vivi_qctrl[] = {
	{
		.id            = V4L2_CID_AUDIO_VOLUME,
		.name          = "Volume",
		.minimum       = 0,
		.maximum       = 65535,
		.step          = 65535/100,
		.default_value = 65535,
		.flags         = V4L2_CTRL_FLAG_SLIDER,
		.type          = V4L2_CTRL_TYPE_INTEGER,
	}, {
		.id            = V4L2_CID_BRIGHTNESS,
		.type          = V4L2_CTRL_TYPE_INTEGER,
		.name          = "Brightness",
		.minimum       = 0,
		.maximum       = 255,
		.step          = 1,
		.default_value = 127,
		.flags         = V4L2_CTRL_FLAG_SLIDER,
	}, {
		.id            = V4L2_CID_CONTRAST,
		.type          = V4L2_CTRL_TYPE_INTEGER,
		.name          = "Contrast",
		.minimum       = 0,
		.maximum       = 255,
		.step          = 0x1,
		.default_value = 0x10,
		.flags         = V4L2_CTRL_FLAG_SLIDER,
	}, {
		.id            = V4L2_CID_SATURATION,
		.type          = V4L2_CTRL_TYPE_INTEGER,
		.name          = "Saturation",
		.minimum       = 0,
		.maximum       = 255,
		.step          = 0x1,
		.default_value = 127,
		.flags         = V4L2_CTRL_FLAG_SLIDER,
	}, {
		.id            = V4L2_CID_HUE,
		.type          = V4L2_CTRL_TYPE_INTEGER,
		.name          = "Hue",
		.minimum       = -128,
		.maximum       = 127,
		.step          = 0x1,
		.default_value = 0,
		.flags         = V4L2_CTRL_FLAG_SLIDER,
	}
};

#define dprintk(dev, level, fmt, arg...) \
	v4l2_dbg(level, debug, &dev->v4l2_dev, fmt, ## arg)

/* ------------------------------------------------------------------
	Basic structures
   ------------------------------------------------------------------*/


#if defined( CONFIG_VIDEO_ADV9880 ) || defined( CONFIG_VIDEO_ADV9880_MODULE )

/* Inputs available at the ADV9880 */
static struct v4l2_input adv9880_1_inputs[] = {
     {
          .index = 0,
          .name = "COMPONENT",
          .type = V4L2_INPUT_TYPE_CAMERA,
          .std = V4L2_STD_ADV9880_ALL
     },
     {
          .index = 1,
          .name = "HDMI",
          .type = V4L2_INPUT_TYPE_CAMERA,
          .std = V4L2_STD_ADV9880_ALL
     },
};

/* Inputs available at the ADV9880 */
static struct v4l2_input adv9880_2_inputs[] = {
     {
          .index = 0,
          .name = "COMPONENT",
          .type = V4L2_INPUT_TYPE_CAMERA,
          .std = V4L2_STD_ADV9880_ALL
     },
     {
          .index = 1,
          .name = "HDMI",
          .type = V4L2_INPUT_TYPE_CAMERA,
          .std = V4L2_STD_ADV9880_ALL
     },
};
#endif

#if defined( CONFIG_VIDEO_ADV7611 ) || defined( CONFIG_VIDEO_ADV7611_MODULE )

/* Inputs available at the ADV7611 */
static struct v4l2_input adv7611_4c_inputs[] = {
     {
          .index = 0,
          .name = "HDMI1",
          .type = V4L2_INPUT_TYPE_CAMERA,
          .std = V4L2_STD_ADV9880_ALL | V4L2_STD_VESA_ALL
     },
};
static struct v4l2_input adv7611_4d_inputs[] = {
     {
          .index = 1,
          .name = "HDMI2",
          .type = V4L2_INPUT_TYPE_CAMERA,
          .std = V4L2_STD_ADV9880_ALL | V4L2_STD_VESA_ALL
     },
};

#endif

#if defined( CONFIG_VIDEO_TVP7002 ) || defined( CONFIG_VIDEO_TVP7002_MODULE )

/* Inputs available at the TVP7002 */
static struct v4l2_input tvp7002_5c_inputs[] = {
     {
          .index = 0,
          .name = "COMPONENT1",
          .type = V4L2_INPUT_TYPE_CAMERA,
          .std = V4L2_STD_ADV9880_ALL
     },
};
static struct v4l2_input tvp7002_5d_inputs[] = {
     {
          .index = 1,
          .name = "COMPONENT2",
          .type = V4L2_INPUT_TYPE_CAMERA,
          .std = V4L2_STD_ADV9880_ALL
     },
};

static struct tvp7002_config  tvp7002_config = {
	.clk_polarity  = 0,
        .hs_polarity   = 0,
        .vs_polarity   = 0,
        .fid_polarity  = 0,
        .sog_polarity  = 0,
};

#endif


 #if defined( CONFIG_VIDEO_GV7601 ) || defined( CONFIG_VIDEO_GV7601_MODULE )

static struct v4l2_input gv7601_inputs[] = {
     {
          .index = 0,
          .name = "HD-SDI",
          .type = V4L2_INPUT_TYPE_CAMERA,
          .std = V4L2_STD_ADV9880_ALL
     },
};

#endif

struct vivi_fmt {
	char  *name;
	u32   fourcc;          /* v4l2 format id */
	int   depth;
};

static struct vivi_fmt formats[] = {
	{
		.name     = "4:2:2, packed, YUYV",
		.fourcc   = V4L2_PIX_FMT_YUYV,
		.depth    = 16,
	},
	{
		.name     = "4:2:2, packed, UYVY",
		.fourcc   = V4L2_PIX_FMT_UYVY,
		.depth    = 16,
	},
	{
		.name     = "RGB565 (LE)",
		.fourcc   = V4L2_PIX_FMT_RGB565, /* gggbbbbb rrrrrggg */
		.depth    = 16,
	},
	{
		.name     = "RGB565 (BE)",
		.fourcc   = V4L2_PIX_FMT_RGB565X, /* rrrrrggg gggbbbbb */
		.depth    = 16,
	},
	{
		.name     = "RGB555 (LE)",
		.fourcc   = V4L2_PIX_FMT_RGB555, /* gggbbbbb arrrrrgg */
		.depth    = 16,
	},
	{
		.name     = "RGB555 (BE)",
		.fourcc   = V4L2_PIX_FMT_RGB555X, /* arrrrrgg gggbbbbb */
		.depth    = 16,
	},
};

struct sg_to_addr {
	int pos;
	struct scatterlist *sg;
};

/* buffer for one video frame */
struct vivi_buffer {
	/* common v4l buffer stuff -- must be first */
	struct videobuf_buffer vb;

	struct vivi_fmt        *fmt;
};

struct vivi_dmaqueue {
	struct list_head       active;

	/* thread for generating video stream*/
	struct task_struct         *kthread;
	wait_queue_head_t          wq;
	/* Counters to control fps rate */
	int                        frame;
	int                        ini_jiffies;
};

static LIST_HEAD(vpif_devlist);

struct vpif_dev {
	struct list_head           vpif_devlist;
	struct v4l2_device 	   v4l2_dev;

	spinlock_t                 slock;
	struct mutex		   mutex;

	int                        users;

	/* various device info */
	struct video_device        *vfd;

	struct vivi_dmaqueue       vidq;

	/* Several counters */
	int                        h, m, s, ms;
	unsigned long              jiffies;
	char                       timestr[13];

	int			   mv_count;	/* Controls bars movement */

	/* Input Number */
	int			   input;

	/* Number of sub devices connected to vpfe */
	int num_subdevs;
	/* i2c bus adapter no */
	struct i2c_adapter *i2c_adaps[VPIF_MAX_I2C_ADAPTERS];
	/* information about each subdev */
	struct vpfe_subdev_info *sub_devs;

	/* sub devices */
	struct v4l2_subdev **sd;

	struct vpfe_subdev_info *current_subdev;
	/* current input at the sub device */
	int current_input;

	/* Control 'registers' */
	int 			   qctl_regs[ARRAY_SIZE(vivi_qctrl)];
};

struct vivi_fh {
	struct vpif_dev            *dev;

	/* video capture */
	struct vivi_fmt            *fmt;
	unsigned int               width, height;
	struct videobuf_queue      vb_vidq;

	enum v4l2_buf_type         type;
	unsigned char              bars[8][3];
	int			   input; 	/* Input Number on bars */
};


struct vpfe_subdev_info {
	/* Sub device name */
	char name[32];
	/* Sub device group id (same if decoders share video port) */
	int grp_id;
	/* Number of inputs supported */
	int num_inputs;
	/* inputs available at the sub device */
	struct v4l2_input *inputs;
	/* i2c data */
	struct i2c_board_info i2c_board_info;
        struct i2c_adapter *i2c_adap;
        /* spi data */
        struct spi_board_info spi_board_info;
        struct spi_master *spi_master;

         /* Which V4L2 subdev */
        struct v4l2_subdev *v4l2_sd;
 };

 /*
  * vpfe_get_subdev_input_index - Get subdev index and subdev input index for a
  * given app input index
  */
 static int vpfe_get_subdev_input_index(struct vpif_dev *vpif_dev,
                                         int *subdev_index,
                                         int *subdev_input_index,
                                         int app_input_index)
 {
         struct vpfe_subdev_info *sdinfo;
         int i, j = 0;

         for (i = 0; i < vpif_dev->num_subdevs; i++) {
                 sdinfo = &vpif_dev->sub_devs[i];
                 if (app_input_index < (j + sdinfo->num_inputs)) {
                         *subdev_index = i;
                         *subdev_input_index = app_input_index - j;
                         return 0;
                 }
                 j += sdinfo->num_inputs;
         }
         return -EINVAL;
 }

 static struct dv_preset_entry {
         int          preset;
         v4l2_std_id  id;
 } dv_presets_table[] = {
         { V4L2_DV_480P59_94, V4L2_STD_525P_60 },
         { V4L2_DV_576P50,    V4L2_STD_625P_50 },

         { V4L2_DV_720P25,    V4L2_STD_720P_25 },
         { V4L2_DV_720P24,    V4L2_STD_720P_25 },
         { V4L2_DV_720P50,    V4L2_STD_720P_50 },
         { V4L2_DV_1080I29_97,V4L2_STD_1080I_60|V4L2_STD_HD_DIV_1001 },
         { V4L2_DV_720P59_94, V4L2_STD_720P_60|V4L2_STD_HD_DIV_1001 },
         { V4L2_DV_720P60,    V4L2_STD_720P_60 },

         { V4L2_DV_1080I60,   V4L2_STD_1080I_60 },
         { V4L2_DV_1080I30,   V4L2_STD_1080I_60 },
         { V4L2_DV_1080I29_97,   V4L2_STD_1080I_60|V4L2_STD_HD_DIV_1001 },
         { V4L2_DV_1080I50,   V4L2_STD_1080I_50 },
         { V4L2_DV_1080I25,   V4L2_STD_1080I_50 },

         { V4L2_DV_1080P24,   V4L2_STD_1080P_24 },
         { V4L2_DV_1080P25,   V4L2_STD_1080P_25 },
         { V4L2_DV_1080P30,   V4L2_STD_1080P_30 },
         { V4L2_DV_1080P50,   V4L2_STD_1080P_50 },
         { V4L2_DV_1080P60,   V4L2_STD_1080P_60 },

         { V4L2_DV_INVALID,    V4L2_STD_UNKNOWN },
 };

 static v4l2_std_id vpif_convert_dvpreset_to_std(int preset)
 {
         struct dv_preset_entry *entry;

         for ( entry = &dv_presets_table[0];
               entry->preset != V4L2_DV_INVALID;
               entry++ ) {
                 if ( entry->preset == preset )
                         break;
         }

         return entry->id;
 }

 static int vpif_convert_std_to_dvpreset(v4l2_std_id id)
 {
         struct dv_preset_entry *entry;

         for ( entry = &dv_presets_table[0];
               entry->preset != V4L2_DV_INVALID;
               entry++ ) {
                 if ( entry->id == id )
                         break;
         }

         return entry->preset;
 }


 static int vidioc_s_std(struct file *file, void *priv, v4l2_std_id *std_id)
 {
         struct vivi_fh *fh = priv;
         struct vpif_dev *dev = fh->dev;
         struct vpfe_subdev_info *sdinfo;
         struct v4l2_subdev *sd;					
         int subdev, index ;
         int ret = 0;


         v4l2_dbg(1, debug, &dev->v4l2_dev, "dummy_vpif_s_std\n");


         ret = mutex_lock_interruptible(&dev->mutex);
         if (ret)
                 return ret;

         if (vpfe_get_subdev_input_index(dev,
                                         &subdev,
                                         &index,
                                         dev->input) < 0) {
              v4l2_dbg(1, debug, &dev->v4l2_dev, "input information not found"
                          " for the subdev\n");
                 return -EINVAL;
         }
         sdinfo = &dev->sub_devs[subdev];
         sd = sdinfo->v4l2_sd;

         if ( sd->ops->core ) {
                 if ( sd->ops->core->s_std ) {
                         ret = sd->ops->core->s_std(sd, *std_id);
                 } 
                 else if ( sd->ops->video && sd->ops->video->s_dv_preset ) {
                         struct v4l2_dv_preset dv_preset;

                         memset( &dv_preset, '\0', sizeof(dv_preset) );
                         dv_preset.preset = vpif_convert_std_to_dvpreset( *std_id );
                         ret = sd->ops->video->s_dv_preset(sd, &dv_preset);
                 }
         }

         if (ret) {
                 v4l2_err(&dev->v4l2_dev,
                         "vpfe_doioctl:error in setting std in decoder\n");
                 ret = -EINVAL;
                 goto unlock_out;
         }


         /* Start streaming */
         ret = v4l2_device_call_until_err(&dev->v4l2_dev, sdinfo->grp_id,
                                          video, s_stream, __sd == sdinfo->v4l2_sd);

         if (ret) {
                 v4l2_err(&dev->v4l2_dev,
                         "vpfe_doioctl:error in setting streaming in decoder\n");
                 ret = -EINVAL;
                 goto unlock_out;
         } else {
                 switch (sdinfo->grp_id)
                 {
                 case 1:
                         z3_fpga_set_pll_vidin_reference(Z3_FPGA_PLL_VIDIN0_REFERENCE);
                         break;
                 case 2:
                         z3_fpga_set_pll_vidin_reference(Z3_FPGA_PLL_VIDIN1_REFERENCE);
                         break;
                 }
         }


 unlock_out:
         mutex_unlock(&dev->mutex);
         return ret;
 }



 /* only one input in this sample driver */
 static int vidioc_enum_input(struct file *file, void *priv,
                                 struct v4l2_input *inp)
 {
         struct vpif_dev *vpif_dev = video_drvdata(file);
         struct vpfe_subdev_info *sdinfo;
         int subdev, index ;

         v4l2_dbg(1, debug, &vpif_dev->v4l2_dev, "vpfe_enum_input\n");

         if (vpfe_get_subdev_input_index(vpif_dev,
                                         &subdev,
                                         &index,
                                         inp->index) < 0) {
              v4l2_dbg(1, debug, &vpif_dev->v4l2_dev, "input information not found"
                          " for the subdev\n");
                 return -EINVAL;
         }
         sdinfo = &vpif_dev->sub_devs[subdev];
         sdinfo->inputs[index].index = inp->index;
         memcpy(inp, &sdinfo->inputs[index], sizeof(struct v4l2_input));
         return 0;
 }

 static int vidioc_g_input(struct file *file, void *priv, unsigned int *i)
 {
         struct vivi_fh *fh = priv;
         struct vpif_dev *dev = fh->dev;

         *i = dev->input;

         return (0);
 }

 static int vidioc_s_input(struct file *file, void *priv, unsigned int input_index)
 {
         struct vivi_fh *fh = priv;
         struct vpif_dev *dev = fh->dev;
         struct vpfe_subdev_info *sdinfo;
         int subdev, index ;
         int ret = 0;

         v4l2_dbg(1, debug, &dev->v4l2_dev, "vpfe_s_input %d\n", input_index);

         if (input_index >= NUM_INPUTS)
                 return -EINVAL;


         ret = mutex_lock_interruptible(&dev->mutex);
         if (ret)
                 return ret;

         if (vpfe_get_subdev_input_index(dev,
                                         &subdev,
                                         &index,
                                         input_index) < 0) {
              v4l2_dbg(1, debug, &dev->v4l2_dev, "input information not found"
                          " for the subdev\n");
                 return -EINVAL;
         }
         sdinfo = &dev->sub_devs[subdev];

         dev->input = input_index;
         dev->current_subdev = sdinfo;

 unlock_out:
         mutex_unlock(&dev->mutex);
         return ret;
 }

 static int vidioc_g_ctrl(struct file *file, void *priv,
                          struct v4l2_control *ctrl)
 {
         struct vivi_fh *fh = priv;
         struct vpif_dev *dev = fh->dev;

         (void) ctrl;
         (void) dev;

         return -EINVAL;
 }
 static int vidioc_s_ctrl(struct file *file, void *priv,
                                 struct v4l2_control *ctrl)
 {
         struct vivi_fh *fh = priv;
         struct vpif_dev *dev = fh->dev;

         (void) ctrl;
         (void) dev;

         return -EINVAL;
 }

 static int vidioc_querystd(struct file *file, void *priv, v4l2_std_id *std_id)
 {
         struct vpif_dev *vpif_dev = video_drvdata(file);
         struct vpfe_subdev_info *sdinfo;
         int ret = 0;

         struct v4l2_subdev *sd;					

         v4l2_dbg(1, debug, &vpif_dev->v4l2_dev, "vpfe_querystd\n");

         ret = mutex_lock_interruptible(&vpif_dev->mutex);
         if (ret)
                 return ret;

         sdinfo = vpif_dev->current_subdev;
         sd = sdinfo->v4l2_sd;

         /* Call querystd function of decoder device */

         if ( sd->ops->video ) {
                 if ( sd->ops->video->querystd ) {
                         ret = sd->ops->video->querystd(sd, std_id);
                 }
                 else if ( sd->ops->video->query_dv_preset ) {
                         struct v4l2_dv_preset qdvpreset;

                         ret = sd->ops->video->query_dv_preset(sd, &qdvpreset);

                         if ( 0 == ret ) {
                                 *std_id = vpif_convert_dvpreset_to_std( qdvpreset.preset );
                                 if ( *std_id == V4L2_STD_UNKNOWN ) {
                                         ret = -EINVAL;
                                 }
                         }
                 }
         }

         ret = (ret == -ENOIOCTLCMD) ? 0 : ret;    

         mutex_unlock(&vpif_dev->mutex);
         return ret;
 }


 /* ------------------------------------------------------------------
         File operations for the device
    ------------------------------------------------------------------*/

 static int vivi_open(struct file *file)
 {
         struct vpif_dev *dev = video_drvdata(file);
         struct vivi_fh *fh = NULL;
         int retval = 0;

         mutex_lock(&dev->mutex);
         dev->users++;

         if (dev->users > 1) {
                 dev->users--;
                 mutex_unlock(&dev->mutex);
                 return -EBUSY;
         }

         dprintk(dev, 1, "open %s type=%s users=%d\n",
                 video_device_node_name(dev->vfd),
                 v4l2_type_names[V4L2_BUF_TYPE_VIDEO_CAPTURE], dev->users);

         /* allocate + initialize per filehandle data */
         fh = kzalloc(sizeof(*fh), GFP_KERNEL);
         if (NULL == fh) {
                 dev->users--;
                 retval = -ENOMEM;
         }
         mutex_unlock(&dev->mutex);

         if (retval)
                 return retval;

         file->private_data = fh;
         fh->dev      = dev;

         fh->type     = V4L2_BUF_TYPE_VIDEO_CAPTURE;
         fh->fmt      = &formats[0];
         fh->width    = 640;
         fh->height   = 480;

         /* Resets frame counters */
         dev->h = 0;
         dev->m = 0;
         dev->s = 0;
         dev->ms = 0;
         dev->mv_count = 0;
         dev->jiffies = jiffies;
         sprintf(dev->timestr, "%02d:%02d:%02d:%03d",
                         dev->h, dev->m, dev->s, dev->ms);

         return 0;
 }

 static int vivi_close(struct file *file)
 {
         struct vivi_fh         *fh = file->private_data;
         struct vpif_dev *dev       = fh->dev;
 //	struct vivi_dmaqueue *vidq = &dev->vidq;
         struct video_device  *vdev = video_devdata(file);

         kfree(fh);

         mutex_lock(&dev->mutex);
         dev->users--;
         mutex_unlock(&dev->mutex);

         dprintk(dev, 1, "close called (dev=%s, users=%d)\n",
                 video_device_node_name(vdev), dev->users);

         return 0;
 }

 static const struct v4l2_file_operations vivi_fops = {
         .owner		= THIS_MODULE,
         .open           = vivi_open,
         .release        = vivi_close,
         .read           = NULL,
         .poll		= NULL,
         .ioctl          = video_ioctl2, /* V4L2 ioctl handler */
         .mmap           = NULL,
 };

 static const struct v4l2_ioctl_ops vivi_ioctl_ops = {
 //	.vidioc_querycap      = vidioc_querycap,
 //	.vidioc_enum_fmt_vid_cap  = vidioc_enum_fmt_vid_cap,
 //	.vidioc_g_fmt_vid_cap     = vidioc_g_fmt_vid_cap,
 //	.vidioc_try_fmt_vid_cap   = vidioc_try_fmt_vid_cap,
 //	.vidioc_s_fmt_vid_cap     = vidioc_s_fmt_vid_cap,
 //	.vidioc_reqbufs       = vidioc_reqbufs,
 //	.vidioc_querybuf      = vidioc_querybuf,
 //	.vidioc_qbuf          = vidioc_qbuf,
 //	.vidioc_dqbuf         = vidioc_dqbuf,
         .vidioc_s_std         = vidioc_s_std,
         .vidioc_enum_input    = vidioc_enum_input,
         .vidioc_g_input       = vidioc_g_input,
         .vidioc_s_input       = vidioc_s_input,
 //	.vidioc_queryctrl     = vidioc_queryctrl,
         .vidioc_querystd      = vidioc_querystd,
         .vidioc_g_ctrl        = vidioc_g_ctrl,
         .vidioc_s_ctrl        = vidioc_s_ctrl,
 //	.vidioc_streamon      = vidioc_streamon,
 //	.vidioc_streamoff     = vidioc_streamoff,
 #ifdef CONFIG_VIDEO_V4L1_COMPAT
 //	.vidiocgmbuf          = vidiocgmbuf,
 #endif
 };

 static struct video_device vivi_template = {
         .name		= "dummy_vpif",
         .fops           = &vivi_fops,
         .ioctl_ops 	= &vivi_ioctl_ops,
         .release	= video_device_release,

         .tvnorms              = V4L2_STD_ADV9880_ALL | V4L2_STD_VESA_ALL,
         .current_norm         = V4L2_STD_NTSC_M,
 };

 /* -----------------------------------------------------------------
         Initialization and module stuff
    ------------------------------------------------------------------*/

 static int vivi_release(void)
 {
         struct vpif_dev *dev;
         struct list_head *list;
         int    i;

         while (!list_empty(&vpif_devlist)) {
                 list = vpif_devlist.next;
                 list_del(list);
                 dev = list_entry(list, struct vpif_dev, vpif_devlist);

                 v4l2_info(&dev->v4l2_dev, "unregistering %s\n",
                         video_device_node_name(dev->vfd));
                 video_unregister_device(dev->vfd);
                 v4l2_device_unregister(&dev->v4l2_dev);

                 for ( i=0; i<VPIF_MAX_I2C_ADAPTERS; i++ ) {
                         if ( dev->i2c_adaps[i] != NULL ) {
                                 i2c_put_adapter( dev->i2c_adaps[i] );
                         }
                 }
                 kfree(dev);
         }

         return 0;
 }



 static int __init vivi_create_instance(int inst)
 {
         struct vpif_dev *dev;
         struct video_device *vfd;
         struct vpfe_subdev_info *sdinfo;
         int ret, i;
         int num_subdevs = 0;
         int i2c_adapter_id = 0;

         u16 z3_board_id = z3_fpga_board_id();

         dev = kzalloc(sizeof(*dev), GFP_KERNEL);
         if (!dev)
                 return -ENOMEM;

         snprintf(dev->v4l2_dev.name, sizeof(dev->v4l2_dev.name),
                         "%s-%03d", Z3NETRA_MODULE_NAME, inst);
         ret = v4l2_device_register(NULL, &dev->v4l2_dev);
         if (ret)
                 goto free_dev;

         /* init video dma queues */
         INIT_LIST_HEAD(&dev->vidq.active);
         init_waitqueue_head(&dev->vidq.wq);

         /* initialize locks */
         spin_lock_init(&dev->slock);
         mutex_init(&dev->mutex);

         ret = -ENOMEM;
         vfd = video_device_alloc();
         if (!vfd)
                 goto unreg_dev;

         *vfd = vivi_template;
         vfd->debug = debug;

         ret = video_register_device(vfd, VFL_TYPE_GRABBER, video_nr);
         if (ret < 0)
                 goto rel_vdev;

         video_set_drvdata(vfd, dev);


         dev->sd = kzalloc(sizeof(struct v4l2_subdev *) * MAX_SUBDEVS,
                           GFP_KERNEL);
         if (NULL == dev->sd) {
                 v4l2_err(&dev->v4l2_dev,
                         "unable to allocate memory for subdevice pointers\n");
                 ret = -ENOMEM;
                 goto rel_vdev;
         }

         dev->sub_devs = kzalloc(sizeof(dev->sub_devs[0]) * MAX_SUBDEVS,
                           GFP_KERNEL);
         if (NULL == dev->sd) {
                 v4l2_err(&dev->v4l2_dev,
                         "unable to allocate memory for subdevice pointers\n");
                 ret = -ENOMEM;
                 goto probe_sd_out;
         }


         for ( i2c_adapter_id = 1; 
               i2c_adapter_id <= VPIF_MAX_I2C_ADAPTERS;
               i2c_adapter_id++ ) {
                 dev->i2c_adaps[i2c_adapter_id-1] = i2c_get_adapter(i2c_adapter_id);
                 if ( NULL == dev->i2c_adaps[i2c_adapter_id-1] ) {
                         break;
                 }
         }


 #if defined( CONFIG_VIDEO_ADV7611 ) || defined( CONFIG_VIDEO_ADV7611_MODULE )
         if ( z3_board_id == Z3_BOARD_ID_NONE 
              || z3_board_id == Z3_BOARD_ID_APP_02
              || z3_board_id == Z3_BOARD_ID_APP_21
              || z3_board_id == Z3_BOARD_ID_APP_31 ) {
                 strcpy( dev->sub_devs[num_subdevs].name, "adv7611" );
                 strcpy( dev->sub_devs[num_subdevs].i2c_board_info.type, "adv7611" );
                 dev->sub_devs[num_subdevs].i2c_board_info.addr = 0x4c;
                 dev->sub_devs[num_subdevs].grp_id = 1;
                 dev->sub_devs[num_subdevs].num_inputs = 1;
                 dev->sub_devs[num_subdevs].inputs = adv7611_4c_inputs;
                 dev->sub_devs[num_subdevs].i2c_adap = dev->i2c_adaps[0];
                 ++num_subdevs;
         }

         if ( z3_board_id == Z3_BOARD_ID_NONE 
              || z3_board_id == Z3_BOARD_ID_APP_02 ) {

                 strcpy( dev->sub_devs[num_subdevs].name, "adv7611" );
                 strcpy( dev->sub_devs[num_subdevs].i2c_board_info.type, "adv7611" );
                 dev->sub_devs[num_subdevs].i2c_board_info.addr = 0x4d;
                 dev->sub_devs[num_subdevs].grp_id = 2;
                 dev->sub_devs[num_subdevs].num_inputs = 1;
                 dev->sub_devs[num_subdevs].inputs = adv7611_4d_inputs;
                 dev->sub_devs[num_subdevs].i2c_adap = dev->i2c_adaps[0];
                 
                 ++num_subdevs;
         }
#endif

#if defined( CONFIG_VIDEO_ADV9880 ) || defined( CONFIG_VIDEO_ADV9880_MODULE )
                 if ( z3_board_id == Z3_BOARD_ID_NONE ) {
                         /* Populate sub_devs array */
                         strcpy( dev->sub_devs[num_subdevs].name, "adv9880" );
                         strcpy( dev->sub_devs[num_subdevs].i2c_board_info.type, "adv9880" );
                         dev->sub_devs[num_subdevs].i2c_board_info.addr = 0x4c;
                         dev->sub_devs[num_subdevs].grp_id = 1;
                         dev->sub_devs[num_subdevs].num_inputs = 2;
                         dev->sub_devs[num_subdevs].inputs = adv9880_1_inputs;
                         dev->sub_devs[num_subdevs].i2c_adap = dev->i2c_adaps[0];

                         ++num_subdevs;

                         strcpy( dev->sub_devs[num_subdevs].name, "adv9880" );
                         strcpy( dev->sub_devs[num_subdevs].i2c_board_info.type, "adv9880" );
                         dev->sub_devs[num_subdevs].i2c_board_info.addr = 0x4c;
                         dev->sub_devs[num_subdevs].grp_id = 2;
                         dev->sub_devs[num_subdevs].num_inputs = 2;
                         dev->sub_devs[num_subdevs].inputs = adv9880_2_inputs;
                         dev->sub_devs[num_subdevs].i2c_adap = dev->i2c_adaps[1];
                 }
                 ++num_subdevs;
#endif

#if defined( CONFIG_VIDEO_TVP7002 ) || defined( CONFIG_VIDEO_TVP7002_MODULE )
                 if ( z3_board_id == Z3_BOARD_ID_NONE 
                      || z3_board_id == Z3_BOARD_ID_APP_02 
                      || z3_board_id == Z3_BOARD_ID_APP_31 ) {
                         strcpy( dev->sub_devs[num_subdevs].name, "tvp7002" );
                         strcpy( dev->sub_devs[num_subdevs].i2c_board_info.type, "tvp7002" );
                         dev->sub_devs[num_subdevs].i2c_board_info.addr = 0x5c;
                         dev->sub_devs[num_subdevs].i2c_board_info.platform_data = &tvp7002_config;
                         dev->sub_devs[num_subdevs].grp_id = 1;
                         dev->sub_devs[num_subdevs].num_inputs = 1;
                         dev->sub_devs[num_subdevs].inputs = tvp7002_5c_inputs;
                         dev->sub_devs[num_subdevs].i2c_adap = dev->i2c_adaps[0];
                         ++num_subdevs;
                 }

                 if ( z3_board_id == Z3_BOARD_ID_NONE 
                      || z3_board_id == Z3_BOARD_ID_APP_21
                      || z3_board_id == Z3_BOARD_ID_APP_02 ) {

                         strcpy( dev->sub_devs[num_subdevs].name, "tvp7002" );
                         strcpy( dev->sub_devs[num_subdevs].i2c_board_info.type, "tvp7002" );
                         if (  z3_board_id == Z3_BOARD_ID_APP_21 ) { 
// I2C address got changed after protos
//                                 dev->sub_devs[num_subdevs].i2c_board_info.addr = 0x5c;
                                 dev->sub_devs[num_subdevs].i2c_board_info.addr = 0x5d;
                         } else {
                                 dev->sub_devs[num_subdevs].i2c_board_info.addr = 0x5d;
                         }
                         dev->sub_devs[num_subdevs].i2c_board_info.platform_data = &tvp7002_config;
                         dev->sub_devs[num_subdevs].grp_id = 2;
                         dev->sub_devs[num_subdevs].num_inputs = 1;
                         dev->sub_devs[num_subdevs].inputs = tvp7002_5d_inputs;
                         dev->sub_devs[num_subdevs].i2c_adap = dev->i2c_adaps[0];

                         ++num_subdevs;
                 }
#endif


#if defined( CONFIG_VIDEO_GV7601 ) || defined( CONFIG_VIDEO_GV7601_MODULE )
                 if ( z3_board_id == Z3_BOARD_ID_APP_31 ) {
                         strcpy( dev->sub_devs[num_subdevs].name, "gv7601" );

                         strcpy( dev->sub_devs[num_subdevs].spi_board_info.modalias, "gv7601" );
//                         dev->sub_devs[num_subdevs].spi_board_info.mode = SPI_MODE_1;
                         dev->sub_devs[num_subdevs].spi_board_info.mode = SPI_MODE_3;
                         dev->sub_devs[num_subdevs].spi_board_info.irq = 0;
                         dev->sub_devs[num_subdevs].spi_board_info.max_speed_hz =  100 * 1000;
                         dev->sub_devs[num_subdevs].spi_board_info.bus_num = 1;
                         dev->sub_devs[num_subdevs].spi_board_info.chip_select = 1;

                         dev->sub_devs[num_subdevs].grp_id = 2;
                         dev->sub_devs[num_subdevs].num_inputs = 1;
                         dev->sub_devs[num_subdevs].inputs = gv7601_inputs;
                         dev->sub_devs[num_subdevs].spi_master = 
                                 spi_busnum_to_master(dev->sub_devs[num_subdevs].spi_board_info.bus_num);

                         ++num_subdevs;


                         strcpy( dev->sub_devs[num_subdevs].name, "gv7600" );

                         strcpy( dev->sub_devs[num_subdevs].spi_board_info.modalias, "gv7600" );
//                         dev->sub_devs[num_subdevs].spi_board_info.mode = SPI_MODE_1;
                         dev->sub_devs[num_subdevs].spi_board_info.mode = SPI_MODE_3;
                         dev->sub_devs[num_subdevs].spi_board_info.irq = 0;
                         dev->sub_devs[num_subdevs].spi_board_info.max_speed_hz =  100 * 1000;
                         dev->sub_devs[num_subdevs].spi_board_info.bus_num = 1;
                         dev->sub_devs[num_subdevs].spi_board_info.chip_select = 0;

                         dev->sub_devs[num_subdevs].grp_id = 10;
                         dev->sub_devs[num_subdevs].num_inputs = 0;
                         dev->sub_devs[num_subdevs].inputs = NULL;
                         dev->sub_devs[num_subdevs].spi_master = 
                                 spi_busnum_to_master(dev->sub_devs[num_subdevs].spi_board_info.bus_num);

                         ++num_subdevs;

                 }

#endif

                 for (i = 0; num_subdevs > 0 && i < num_subdevs; i++) {
                         //		struct v4l2_input *inps;

                         sdinfo = &dev->sub_devs[i];

                         /* Load up the subdevice */
                         if ( sdinfo->i2c_adap ) {
                                 dev->sd[i] =
                                         v4l2_i2c_new_subdev_board(&dev->v4l2_dev, 
                                                                   sdinfo->i2c_adap,
                                                                   &sdinfo->i2c_board_info,
                                                                   NULL,
                                                                   0 );
                         } 
                         else if ( sdinfo->spi_master  ) {
                                 dev->sd[i] = 
                                         v4l2_spi_new_subdev(&dev->v4l2_dev,
                                                             sdinfo->spi_master,
                                                             &sdinfo->spi_board_info,
                                                             0 );
                         }

                         if (dev->sd[i]) {
                                 v4l2_info(&dev->v4l2_dev,
                                           "v4l2 sub device %s registered grp %d\n",
                                           sdinfo->name, sdinfo->grp_id);
                                 dev->sd[i]->grp_id = sdinfo->grp_id;

                                 sdinfo->v4l2_sd = dev->sd[i];
                        
                                 dev->num_subdevs++;
                         } else {
                                 v4l2_info(&dev->v4l2_dev,
                                           "v4l2 sub device %s register fails\n",
                                           sdinfo->name);

                                 memcpy( &dev->sub_devs[i],
                                         &dev->sub_devs[i+1],
                                         ((num_subdevs-(i+1)) * sizeof(dev->sub_devs[0])) );
                                 num_subdevs--;
                                 i--;
                         }
                 }

                 /* set first sub device as current one */
                 dev->current_subdev = &dev->sub_devs[0];
                 dev->num_subdevs = num_subdevs;

                 /* Set all controls to their default value. */
                 for (i = 0; i < ARRAY_SIZE(vivi_qctrl); i++)
                         dev->qctl_regs[i] = vivi_qctrl[i].default_value;

                 /* Now that everything is fine, let's add it to device list */
                 list_add_tail(&dev->vpif_devlist, &vpif_devlist);

                 if (video_nr != -1)
                         video_nr++;

                 dev->vfd = vfd;
                 v4l2_info(&dev->v4l2_dev, "V4L2 device registered as %s\n",
                           video_device_node_name(vfd));
                 return 0;

         probe_sd_out:
                 kfree(dev->sd);
         rel_vdev:

                 video_device_release(vfd);
         unreg_dev:
                 v4l2_device_unregister(&dev->v4l2_dev);
         free_dev:
                 kfree(dev);
                 return ret;
         }

/* This routine allocates from 1 to n_devs virtual drivers.

   The real maximum number of virtual drivers will depend on how many drivers
   will succeed. This is limited to the maximum number of devices that
   videodev supports, which is equal to VIDEO_NUM_DEVICES.
 */
static int __init vivi_init(void)
{
	int ret = 0, i;

	if (n_devs <= 0)
		n_devs = 1;

	for (i = 0; i < n_devs; i++) {
		ret = vivi_create_instance(i);
		if (ret) {
			/* If some instantiations succeeded, keep driver */
			if (i)
				ret = 0;
			break;
		}
	}

	if (ret < 0) {
		printk(KERN_INFO "Error %d while loading vivi driver\n", ret);
		return ret;
	}


	/* n_devs will reflect the actual number of allocated devices */
	n_devs = i;

	printk(KERN_INFO "Z3 Netra video decoders ver %u.%u.%u successfully loaded, %u devs.\n",
			(VIVI_VERSION >> 16) & 0xFF, (VIVI_VERSION >> 8) & 0xFF,
                        (VIVI_VERSION & 0xFF), n_devs
                );

	return ret;
}

static void __exit vivi_exit(void)
{
	vivi_release();
}

module_init(vivi_init);
module_exit(vivi_exit);
