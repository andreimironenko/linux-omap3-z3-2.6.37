//#define DEBUG
#include <linux/ctype.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/ioctl.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/list.h>
#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/slab.h>

#include <linux/spi/spi.h>
#include <linux/device.h>
#include <linux/cdev.h>

//#include <asm/arch/video_evm.h>
#include <asm/uaccess.h>
#include <mach/z3_fpga.h>

#include <media/v4l2-ioctl.h>
#include <linux/videodev2.h>
#include <media/v4l2-device.h>
#include <media/v4l2-chip-ident.h>
#include <asm/uaccess.h>

#include "gv7601.h"

#define GV7601_SPI_TRANSFER_MAX 1024

//#define GV7601_ENABLE_ANCILLARY_DATA

#define DRIVER_NAME     "anc_data"

/* Debug functions */
static int debug = 0;
module_param(debug, int, 0644);
MODULE_PARM_DESC(debug, "Debug level (0-2)");

/* Function Prototypes */
static int gv7601_initialize(struct v4l2_subdev *sd);
static int gv7601_deinitialize(struct v4l2_subdev *sd);
static int gv7601_s_std(struct v4l2_subdev *sd, v4l2_std_id std);
static int gv7601_querystd(struct v4l2_subdev *sd, v4l2_std_id *id);
static int gv7601_s_stream(struct v4l2_subdev *sd, int enable);


struct gv7601_spidata {
	struct spi_device	*spi;

	struct mutex		buf_lock;
	unsigned		users;
	u8			*buffer;
};

struct gv7601_params {
	v4l2_std_id std;
	int         inputidx;
	struct v4l2_format fmt;
};

struct gv7601_channel {
     struct v4l2_subdev    sd;
     struct gv7601_spidata *spidata;
     struct gv7601_params   params;
     u16    anc_buf[GV7601_ANC_BANK_SIZE];
};


static struct gv7601_channel gv7601_channel_info[GV7601_NUM_CHANNELS] = {
     [0] = { 
          .spidata = NULL,
     },
};


static atomic_t reference_count = ATOMIC_INIT(0);

/* For registeration of	charatcer device*/
static struct cdev c_dev;
/* device structure	to make	entry in device*/
static dev_t dev_entry;

static struct class *gv7601_class;

static const struct v4l2_subdev_video_ops gv7601_video_ops = {
	.querystd = gv7601_querystd,
	.s_stream = gv7601_s_stream,
//	.g_input_status = gv7601_g_input_status,
};

static const struct v4l2_subdev_core_ops gv7601_core_ops = {
        .g_chip_ident = NULL,
	.s_std = gv7601_s_std,
};

static const struct v4l2_subdev_ops gv7601_ops = {
	.core = &gv7601_core_ops,
	.video = &gv7601_video_ops,
};



#define V4L2_STD_GV7601_ALL        (V4L2_STD_720P_60 | \
					V4L2_STD_720P_50 | \
					V4L2_STD_1080I_60 | \
					V4L2_STD_1080I_50 | \
					V4L2_STD_1080P_24 | \
					V4L2_STD_1080P_25 | \
					V4L2_STD_1080P_30 | \
					V4L2_STD_525P_60 | \
					V4L2_STD_625P_50 | \
                                        V4L2_STD_525_60 | \
                                        V4L2_STD_625_50 \
                                   )
#if 0
static struct v4l2_standard gv7601_standards[GV7601_MAX_NO_STANDARDS] = {
	{
		.index = 0,
		.id = V4L2_STD_720P_60,
		.name = "720P-60",
		.frameperiod = {1, 60},
		.framelines = 720
	},
	{
		.index = 1,
		.id = V4L2_STD_1080I_60,
		.name = "1080I-30",
		.frameperiod = {1, 30},
		.framelines = 1080
	},
	{
		.index = 2,
		.id = V4L2_STD_1080I_50,
		.name = "1080I-25",
		.frameperiod = {1, 25},
		.framelines = 1080
	},
	{
		.index = 3,
		.id = V4L2_STD_720P_50,
		.name = "720P-50",
		.frameperiod = {1, 50},
		.framelines = 720
	},
	{
		.index = 4,
		.id = V4L2_STD_1080P_25,
		.name = "1080P-25",
		.frameperiod = {1, 25},
		.framelines = 1080
	},
	{
		.index = 5,
		.id = V4L2_STD_1080P_30,
		.name = "1080P-30",
		.frameperiod = {1, 30},
		.framelines = 1080
	},
	{
		.index = 6,
		.id = V4L2_STD_1080P_24,
		.name = "1080P-24",
		.frameperiod = {1, 24},
		.framelines = 1080
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
};
#endif

struct v4l2_input gv7601_inputs[GV7601_MAX_NO_INPUTS] = {
     [0] = {
          .index = 0,
          .name  = "HD-SDI",
          .type = V4L2_INPUT_TYPE_CAMERA,
          .std  = V4L2_STD_GV7601_ALL,
     },
};


static inline struct gv7601_channel *to_gv7601(struct v4l2_subdev *sd)
{
	return container_of(sd, struct gv7601_channel, sd);
}


static struct spi_device *gv7601_get_spi( struct gv7601_channel *ch )
{
     if ( ch == NULL 
          || ch->spidata == NULL ) 
          return NULL;

     return ch->spidata->spi;
         
}

static struct device *gv7601_get_dev( struct gv7601_channel *ch )
{
     if ( ch == NULL 
          || ch->spidata == NULL  
          || ch->spidata->spi == NULL ) 
          return NULL;

     return &ch->spidata->spi->dev;
}

int gv7601_read_buffer(struct spi_device *spi, u16 offset, u16 *values, int length )
{
     struct spi_message msg;
     struct spi_transfer spi_xfer;
     int status;
     u16 txbuf[GV7601_SPI_TRANSFER_MAX+1] = {
          0x9000,  // read, auto-increment
     };
     u16 rxbuf[GV7601_SPI_TRANSFER_MAX+1];

     if ( NULL == spi ) {
          return -ENODEV;
     }

     if ( length > GV7601_SPI_TRANSFER_MAX ) {
          return -EINVAL;
     }

     txbuf[0] = txbuf[0] | (offset & 0xfff);

     memset( &spi_xfer, '\0', sizeof(spi_xfer) );
     spi_xfer.tx_buf = txbuf;
     spi_xfer.rx_buf = rxbuf;
     spi_xfer.cs_change = 1;
     spi_xfer.bits_per_word = 16;
     spi_xfer.delay_usecs = 0;
     spi_xfer.speed_hz = 1000000;
     spi_xfer.len = sizeof(*values)*(length+1); // length in bytes

     spi_message_init( &msg );
     spi_message_add_tail( &spi_xfer, &msg );

     status = spi_sync(spi, &msg);

     memcpy( values, &rxbuf[1], sizeof(*values)*length );

     return status;
}

int gv7601_read_register(struct spi_device *spi, u16 offset, u16 *value)
{
     struct spi_message msg;
     struct spi_transfer spi_xfer;
     int status;
     u32 txbuf[1] = {
          0x90000000,  // read, auto-increment
     };
     u32 rxbuf[1];

     if ( NULL == spi ) {
          return -ENODEV;
     }


     txbuf[0] = txbuf[0] | ((offset & 0xfff)<<16);

     memset( &spi_xfer, '\0', sizeof(spi_xfer) );
     spi_xfer.tx_buf = txbuf;
     spi_xfer.rx_buf = rxbuf;
     spi_xfer.cs_change = 0; // ??
     spi_xfer.bits_per_word = 32;
     spi_xfer.delay_usecs = 0;
     spi_xfer.speed_hz = 1000000;
     spi_xfer.len = 4; // length in bytes

     spi_message_init( &msg );
     spi_message_add_tail( &spi_xfer, &msg );

     status = spi_sync(spi, &msg);

     if ( 0 != status ) {
             dev_dbg( &spi->dev, "read_reg failed\n" );
             *value = 0xffff;
     } else {
             *value = (u16)(rxbuf[0] & 0xffff);
     }

     return status;
}

int gv7601_write_buffer(struct spi_device *spi, u16 offset, u16 *values, int length)
{
     struct spi_message msg;
     struct spi_transfer spi_xfer;
     int status;
     u16 txbuf[GV7601_SPI_TRANSFER_MAX] = {
          0x1000,  // write, auto-increment
     };
     u16 rxbuf[GV7601_SPI_TRANSFER_MAX] = {
     };

     if ( NULL == spi ) {
          return -ENODEV;
     }

     if ( length > GV7601_SPI_TRANSFER_MAX-1 ) {
          return -EINVAL;
     }

     txbuf[0] = txbuf[0] | (offset & 0xfff);
     memcpy( &txbuf[1], values, sizeof(*values)*length );
     
     memset( &spi_xfer, '\0', sizeof(spi_xfer) );
     spi_xfer.tx_buf = txbuf;
     spi_xfer.rx_buf = rxbuf;
     spi_xfer.cs_change = 0; // ??
     spi_xfer.bits_per_word = 16;
     spi_xfer.delay_usecs = 0;
     spi_xfer.speed_hz = 1000000;
     spi_xfer.len = sizeof(*values)*(length+1); // length in bytes

     spi_message_init( &msg );
     spi_message_add_tail( &spi_xfer, &msg );

     status = spi_sync(spi, &msg);
     return status;
}


int gv7601_write_register(struct spi_device *spi, u16 offset, u16 value)
{
     return gv7601_write_buffer(spi, offset, &value, 1);
}


static struct timer_list gv7601_timer;
static unsigned int gv7601_timeout_ms = 50;

static void gv7601_workqueue_handler(struct work_struct * data);

DECLARE_WORK(gv7601_worker, gv7601_workqueue_handler);

static void gv7601_timer_function(unsigned long data)
{
     (void) data;
     schedule_work(&gv7601_worker);
     mod_timer( &gv7601_timer, jiffies + msecs_to_jiffies( gv7601_timeout_ms ) );
}


static void gv7601_workqueue_handler(struct work_struct *ignored)
{
     struct gv7601_channel *ch;
     struct spi_device *spi;
     u16    bank_reg_base, anc_reg;
     static u16    anc_buf[257];
     int    anc_length;
     int    adf_found = 1;
     int    ch_id;
     int    status;
     u16    did,sdid;
     
     for ( ch_id = 0; ch_id < GV7601_NUM_CHANNELS; ch_id++ ) {
          ch = &gv7601_channel_info[ch_id];

          spi = gv7601_get_spi( ch );
          if ( NULL == spi ) {
               continue;
          }

          /* Step 2: start writing to other bank */
          gv7601_write_register( spi,
                                 GV7601_REG_ANC_CONFIG,
                                 GV7601_ANC_CONFIG_ANC_DATA_SWITCH
          );


#if 1
          /* Step 1: read ancillary data */
          bank_reg_base = GV7601_ANC_BANK_REG;
          status = 0;
          for ( anc_reg = bank_reg_base, adf_found = 1;
                (0 == status) && ((anc_reg+6) < bank_reg_base + GV7601_ANC_BANK_SIZE);
                ) {

               status = gv7601_read_buffer(spi, anc_reg, anc_buf, 6);
               anc_reg += 6;

               if (anc_reg >= bank_reg_base + GV7601_ANC_BANK_SIZE)
                    break;

               if ( 0 == status ) {
                    if ( anc_buf[0] == 0
                         || anc_buf[1] == 0xff
                         || anc_buf[2] == 0xff ) {

                         did = anc_buf[3];
                         sdid = anc_buf[4];
                         anc_length = (anc_buf[5] & 0xff)+1;
                         anc_reg += anc_length;
                         
                         if ( !( did ==0 && sdid == 0) 
                              && ( did < 0xf8 ) ) {
                              dev_dbg( &spi->dev, "anc[%x] %02x %02x %02x\n",
                                       anc_reg,
                                       did,
                                       sdid,
                                       anc_length );
                         }
                    } else {
                         break;
                    }
               }
          }
          dev_dbg( &spi->dev, "anc_end[%x] %02x %02x %02x\n",
                   anc_reg,
                   anc_buf[0],
                   anc_buf[1],
                   anc_buf[2] );
#endif          

          /* Step 3: switch reads to other bank */
          gv7601_write_register( spi,
                                 GV7601_REG_ANC_CONFIG,
                                 0 );
          
     }
}


static int gv7601_dump_regs(struct spi_device *spi)
{
        u16 value;
        int offset; 

        u16 stdlock_value = 0;

        for ( offset=0; offset < 0x80; offset++ ) {

                /* Skip reserved registers */
                if ( offset == 1
                     || offset == 3 
                     || offset == 7
                     || offset == 0x0d 
                     || offset == 0x0e
                     || ( 0x14 <= offset  && offset <= 0x1e )
                     || ( 0x27 <= offset  && offset <= 0x36 )
                     || offset == 0x38
                     || ( 0x3a <= offset  && offset <= 0x6b )
                     || ( 0x6e <= offset  && offset <= 0x72 )
                     || ( 0x74 <= offset  && offset <= 0x85 ) )
                        continue;

                if ( 0 == gv7601_read_register( spi, offset, &value ) ) {
                        if ( value != 0 ) {
                                dev_dbg( &spi->dev, "vid_core offset %u value x%04x\n",
                                         (unsigned int)offset, (unsigned int)value );
                        }
                        if ( offset == GV7601_REG_STD_LOCK ) {
                                stdlock_value = value;
                        }
                }
                
        }

        /* Audio registers only valid when standard is locked */
        switch (stdlock_value & 0xd000) {
        case 0x1000:
        case 0x9000:

                for ( offset=0x200; offset < 0x210; offset++ ) {
                        if ( 0 == gv7601_read_register( spi, offset, &value ) ) {
                                if ( value != 0 ) {
                                        dev_dbg( &spi->dev, "hd_audio offset %u value x%04x\n",
                                                 (unsigned int)offset, (unsigned int)value );
                                }
                        }
                }
                break;

        case 0x5000:
        case 0xd000:
                
                for ( offset=0x400; offset < 0x410; offset++ ) {
                        if ( 0 == gv7601_read_register( spi, offset, &value ) ) {
                                if ( value != 0 ) {
                                        dev_dbg( &spi->dev, "sd_audio offset %u value x%04x\n",
                                                 (unsigned int)offset, (unsigned int)value );
                                }
                        }
                }

                break;
        default:
                break;
        }

        return 0;
}

#if defined(GV7601_ENABLE_ANCILLARY_DATA)
static int gv7601_init_ancillary(struct spi_device *spi)
{
        u16 value;
        int offset; 

        int status = 0;

        /* Set up ancillary data filtering */
        if ( 0 == status )
                status = gv7601_write_register( spi, 
                                       GV7601_REG_ANC_TYPE1,
                                       0x4100 ); // SMPTE-352M Payload ID (0x4101)
        if ( 0 == status )
                status = gv7601_write_register( spi, 
                                       GV7601_REG_ANC_TYPE2,
                                       0x5f00 ); // Placeholder

        if ( 0 == status )
                status = gv7601_write_register( spi, 
                                       GV7601_REG_ANC_TYPE3,
                                       0x6000 ); // Placeholder

        if ( 0 == status )
                status = gv7601_write_register( spi, 
                                       GV7601_REG_ANC_TYPE4,
                               0x6100 ); // SMPTE 334 - SDID 01=EIA-708, 02=EIA-608
        if ( 0 == status ) 
                status = gv7601_write_register( spi, 
                               GV7601_REG_ANC_TYPE5,
                               0x6200 ); // SMPTE 334 - SDID 01=Program description, 02=Data broadcast, 03=VBI data
        
        if ( 0 == gv7601_read_register( spi, GV7601_REG_ANC_TYPE1, &value ) ) {
                dev_dbg( &spi->dev, "REG_ANC_TYPE1 value x%04x\n",
                                 (unsigned int)value );
        }
        if ( 0 == gv7601_read_register( spi, GV7601_REG_ANC_TYPE2, &value ) ) {
                dev_dbg( &spi->dev, "REG_ANC_TYPE2 value x%04x\n",
                                 (unsigned int)value );
        }


        /* Clear old ancillary data */
        if ( 0 == status ) 
                status = gv7601_write_register( spi,
                                                GV7601_REG_ANC_CONFIG,
                                                GV7601_ANC_CONFIG_ANC_DATA_SWITCH
                );

        /* Step 2: start writing to other bank */
        if ( 0 == status ) 
                status = gv7601_write_register( spi,
                                                GV7601_REG_ANC_CONFIG,
                                                0
                        );
        
        return status;
}
#endif

/* gv7601_initialize :
 * This function will set the video format standard
 */
static int gv7601_initialize(struct v4l2_subdev *sd)
{
        struct gv7601_channel *ch;
        struct spi_device *spi;
        int status = 0;

        ch = to_gv7601(sd);
     
        try_module_get(THIS_MODULE);

        spi = gv7601_get_spi( ch );

#if 0
        gv7601_write_register( spi,
                               GV7601_REG_VIDEO_CONFIG,
                               (GV7601_REG_VIDEO_CONFIG_861_PIN_DISABLE_MASK|
                                GV7601_REG_VIDEO_CONFIG_TIMING_861_MASK)
                );
#endif

#if defined(GV7601_ENABLE_ANCILLARY_DATA)
        gv7601_init_ancillary(spi);
#endif


#if 0
        if ( gv7601_timer.function == NULL ) {
                init_timer(&gv7601_timer);
                gv7601_timer.function = gv7601_timer_function;
        }
        mod_timer( &gv7601_timer, jiffies + msecs_to_jiffies( gv7601_timeout_ms ) );
#endif     

        return status;
}

static int gv7601_deinitialize(struct v4l2_subdev *sd)
{
	int ch_id;
        struct gv7601_channel *ch;
        struct device *dev;

        ch = to_gv7601( sd );

        dev = gv7601_get_dev( ch );
        if ( NULL != dev ) {
             dev_dbg( dev, "GV7601 deinitialization\n");
        }

#if 0
        del_timer_sync( &gv7601_timer );
#endif

        module_put(THIS_MODULE);

        return 0;
}

static int gv7601_s_std(struct v4l2_subdev *sd, v4l2_std_id std)
{
        
	v4l2_dbg(1, debug, sd, "Start of adv7611_setstd 0x%llx..\n", std );

	v4l2_dbg(1, debug, sd, "End of adv7611 set standard...\n");

        return 0;
}


static int gv7601_querystd(struct v4l2_subdev *sd, v4l2_std_id *id)
{
     struct spi_device     *spi = NULL;
     struct gv7601_channel *ch  = NULL;

     u16 std_lock_value;
     u16 sync_lock_value;
     u16 words_per_actline_value;
     u16 words_per_line_value;
     u16 lines_per_frame_value;
     u16 actlines_per_frame_value;
     u16 interlaced_flag;
     int status;
     u16 sd_audio_status_value;
     u16 hd_audio_status_value;
     u16 sd_audio_config_value;
     u16 hd_audio_config_value;
     u16 readback_value;

     ch = to_gv7601( sd );
     spi = gv7601_get_spi( ch );
     if ( NULL == spi ) {
          return -ENODEV;
     }


//     *id = V4L2_STD_1080P_60;
//     return 0;

     status = gv7601_read_register( spi, 
                                    GV7601_REG_STD_LOCK, 
                                    &std_lock_value );
     actlines_per_frame_value = std_lock_value & GV7601_REG_STD_LOCK_ACT_LINES_MASK;
     interlaced_flag = std_lock_value & GV7601_REG_STD_LOCK_INT_PROG_MASK;
     interlaced_flag = !!interlaced_flag;

     if ( 0 == status ) {
          status = gv7601_read_register( spi, 
                                         GV7601_REG_HVLOCK, 
                                         &sync_lock_value );
     }
     if ( 0 == status ) {
          status = gv7601_read_register( spi, 
                                         GV7601_REG_WORDS_PER_ACTLINE, 
                                         &words_per_actline_value );
          words_per_actline_value &= GV7601_REG_WORDS_PER_ACTLINE_MASK;
     }
     if ( 0 == status ) {
          status = gv7601_read_register( spi, 
                                         GV7601_REG_WORDS_PER_LINE, 
                                         &words_per_line_value );
          words_per_line_value &= GV7601_REG_WORDS_PER_LINE_MASK;
     }
     if ( 0 == status ) {
          status = gv7601_read_register( spi, 
                                         GV7601_REG_LINES_PER_FRAME, 
                                         &lines_per_frame_value );
          lines_per_frame_value &= GV7601_REG_LINES_PER_FRAME_MASK;
     }

     if ( 0 == status ) {
             v4l2_dbg(1, debug, sd, "Words per line %u/%u Lines per frame %u/%u\n",
                      (unsigned int) words_per_actline_value,
                      (unsigned int) words_per_line_value,
                      (unsigned int) actlines_per_frame_value,
                      (unsigned int) lines_per_frame_value );
             v4l2_dbg(1, debug, sd, "SyncLock: %s %s StdLock: 0x%04x\n",
                   (sync_lock_value & GV7601_REG_HVLOCK_VLOCK_MASK) ? "Vsync" : "NoVsync",
                   (sync_lock_value & GV7601_REG_HVLOCK_HLOCK_MASK) ? "Hsync" : "NoHsync",
                   (unsigned int)(std_lock_value) );
     }

     if ( 0 == status ) {
          status = gv7601_read_register( spi, GV7601_REG_SD_AUDIO_STATUS, &sd_audio_status_value );
          v4l2_dbg(1, debug, sd, "SD audio status 0x%x\n", (unsigned int)sd_audio_status_value );
     }
     if ( 0 == status ) { 
          status = gv7601_read_register( spi, GV7601_REG_HD_AUDIO_STATUS, &hd_audio_status_value );
          v4l2_dbg(1, debug, sd, "HD audio status 0x%x\n", (unsigned int)hd_audio_status_value );
     }

     if ( 0 == lines_per_frame_value ) {
          return -EINVAL;
     }

     if ( interlaced_flag != 0  
          && lines_per_frame_value == 525 ) {
          *id = V4L2_STD_525_60;
     }
     else if ( interlaced_flag != 0 
               && lines_per_frame_value == 625 ) {
          *id = V4L2_STD_625_50;
     }
     else if ( interlaced_flag == 0 
               && lines_per_frame_value == 525 ) {
          *id = V4L2_STD_525P_60;
     }
     else if ( interlaced_flag == 0 
               && lines_per_frame_value == 625 ) {
          *id = V4L2_STD_625P_50;
     }
     else if ( interlaced_flag == 0
               && 749 <= lines_per_frame_value 
               && lines_per_frame_value <= 750 ) {

          if ( words_per_line_value > 1650  ) {
               *id = V4L2_STD_720P_50;
          } else {
               *id = V4L2_STD_720P_60;
          }
     } 
     else if ( interlaced_flag == 0 
               && 1124 <= lines_per_frame_value
               && lines_per_frame_value <= 1125 ) {
          if ( words_per_line_value >= 2200+550 ) {
               *id = V4L2_STD_1080P_24;
          } 
          else if ( words_per_line_value >= 2200+440 ) {
               *id = V4L2_STD_1080P_25;
          }
          else {
//               *id = V4L2_STD_1080P_30;
               *id = V4L2_STD_1080P_60;
          }
     }
     else if ( interlaced_flag == 1 
               && 1124 <= lines_per_frame_value
               && lines_per_frame_value <= 1125 ) {

          if ( words_per_line_value >= 2200+440 ) {
               *id = V4L2_STD_1080I_50;
          } 
          else {
               *id = V4L2_STD_1080I_60;
          }
     }
     else {
          dev_err( &spi->dev, "Std detection failed: interlaced_flag: %u words per line %u/%u Lines per frame %u/%u SyncLock: %s %s StdLock: 0x%04x\n", 
                   (unsigned int) interlaced_flag,
                   (unsigned int) words_per_actline_value,
                   (unsigned int) words_per_line_value,
                   (unsigned int) actlines_per_frame_value,
                   (unsigned int) lines_per_frame_value,
                   (sync_lock_value & GV7601_REG_HVLOCK_VLOCK_MASK) ? "Vsync" : "NoVsync",
                   (sync_lock_value & GV7601_REG_HVLOCK_HLOCK_MASK) ? "Hsync" : "NoHsync",
                   (unsigned int)(std_lock_value) );
          return -EINVAL;
     }

     sd_audio_config_value = 0xaaaa; // 16-bit, right-justified

     if ( 0 == status ) {
          status = gv7601_write_register( spi,
                                          GV7601_REG_SD_AUDIO_CONFIG,
                                          sd_audio_config_value ); 
     }
     if ( 0 == status ) {
          status = gv7601_read_register( spi,
                                         GV7601_REG_SD_AUDIO_CONFIG,
                                         &readback_value );
          if ( 0 == status ) {
               if ( sd_audio_config_value != readback_value ) {
                    dev_dbg( &spi->dev, "SD audio readback failed, wanted x%04x, got x%04x\n",
                             (unsigned int) sd_audio_config_value,
                             (unsigned int) readback_value
                         );
               }
          }
     }

     if ( 0 == status ) {
          hd_audio_config_value = 0x0aa4; // 16-bit, right-justified

          status = gv7601_write_register( spi,
                                     GV7601_REG_HD_AUDIO_CONFIG,
                                     hd_audio_config_value );

     }

     if ( 0 == status ) {
          status = gv7601_read_register( spi,
                                         GV7601_REG_HD_AUDIO_CONFIG,
                                         &readback_value );
          if ( 0 == status ) {
               if ( hd_audio_config_value != readback_value ) {
                    dev_dbg( &spi->dev, "HD audio readback failed, wanted x%04x, got x%04x\n",
                             (unsigned int) hd_audio_config_value,
                             (unsigned int) readback_value
                         );
               }
          }
     }


     switch ( *id ) {
     case V4L2_STD_525_60:
     case V4L2_STD_625_50:
          dev_dbg( &spi->dev, "Set SD-SDI input, 8-bits width\n");
//          dec_device->if_type = INTERFACE_TYPE_BT656;
          gv7601_write_register( spi,
                                 GV7601_REG_ANC_CONFIG,
                                 2 );

          z3_fpga_si_20bit( 0 );
          break;

     default:
          dev_dbg( &spi->dev, "Set HD-SDI input, 16-bits width\n");
//          dec_device->if_type = INTERFACE_TYPE_BT1120;
          gv7601_write_register( spi,
                                 GV7601_REG_ANC_CONFIG,
                                 0 );
          z3_fpga_si_20bit( 1 );
          break;
     }

#if 0
     status = gv7601_write_register( spi,
                                     GV7601_REG_AUDIO_CONFIG,
                                     (GV7601_REG_AUDIO_CONFIG_SCLK_INV_MASK | GV7601_REG_AUDIO_CONFIG_MCLK_SEL_128FS)
          );
#endif

     return 0;
}

static int gv7601_s_stream(struct v4l2_subdev *sd, int enable)
{
	v4l2_dbg(1, debug, sd, "Start of gv7601_s_stream %d\n", enable );

        return 0;
}



/*-------------------------------------------------------------------------*/

int gv7601_char_open (struct inode *inode, struct file *file)
{
        int ch_id = 0; //iminor(inode)
        struct gv7601_channel *ch;

 	if (file->f_mode == FMODE_WRITE) {
		printk (KERN_ERR "TX Not supported\n");
		return -EACCES;
	}

        if ( ch_id >= GV7601_NUM_CHANNELS ){
            printk(KERN_ERR "gv7601: Bad channel ID\n");
             return -EINVAL;
        }

        ch = &gv7601_channel_info[ch_id];

        file->private_data =  ch;

	if (atomic_inc_return(&reference_count) > 1) {
                printk(KERN_ERR "gv7601: already open\n");
		atomic_dec(&reference_count);
		return -EACCES;
	}

	return 0;
}


int gv7601_char_release (struct inode *inode, struct file *file)
{
	atomic_dec(&reference_count);
	return 0;
}



ssize_t gv7601_char_read (struct file *file, 
                          char __user *buff, 
                          size_t size, 
                          loff_t *loff)
{

     struct gv7601_channel *channel = file->private_data;
     struct spi_device     *spi;
     int                    status;

     if ( NULL == channel ) {
          printk( KERN_ERR "gv7601: No channel\n" );
          return -ENODEV;
     }

     if ( NULL == (spi = gv7601_get_spi(channel)) ) {
          printk( KERN_ERR "gv7601: No spi dev\n" );
          return -ENODEV;
     }

     /* we only allow one key read at a time */
     size = (size/2)*2;
     if (size > GV7601_ANC_BANK_SIZE) {
          size = GV7601_ANC_BANK_SIZE;
     }

begin:
     /* Step 2: start writing to other bank */
     gv7601_write_register( spi,
                            GV7601_REG_ANC_CONFIG,
                            GV7601_ANC_CONFIG_ANC_DATA_SWITCH );


     status = gv7601_read_buffer(spi, GV7601_ANC_BANK_REG, channel->anc_buf, size/2);
     if ( 0 != status ) {
          printk( KERN_ERR "gv7601: Read buffer fail\n" );
          return status;
     }

     /* Step 3: switch reads to other bank */
     gv7601_write_register( spi,
                            GV7601_REG_ANC_CONFIG,
                            0 );

     if ( channel->anc_buf[1] != 0 ) {
          /* there is a key to be read.. */
          if (copy_to_user(buff, channel->anc_buf, size) != 0)
               return -EFAULT;
     } else {

          /* tell non blocking applications to come again */
          if (file->f_flags & O_NONBLOCK) {
               return -EAGAIN;
          } else { /* ask blocking applications to sleep comfortably */
               msleep(20);
               goto begin;
          }
     }

     return size;
}



long gv7601_char_ioctl ( struct file *filp, unsigned int cmd, unsigned long arg )
{
     return -ENOTTY;
}

static struct file_operations gv7601_char_fops = {
     .owner   = THIS_MODULE,
     .open    = gv7601_char_open,
     .read    = gv7601_char_read,
     .release = gv7601_char_release,
     .unlocked_ioctl   = gv7601_char_ioctl,
};



/*-------------------------------------------------------------------------*/


static int gv7601_probe(struct spi_device *spi)
{
     struct gv7601_spidata	*spidata;
     int			status = 0;
     struct gv7601_channel      *channel;
     struct v4l2_subdev         *sd;

     dev_dbg( &spi->dev, "Enter gv7601_probe\n" );

     /* Allocate driver data */
     spidata = kzalloc(sizeof(*spidata), GFP_KERNEL);
     if (!spidata)
          return -ENOMEM;

     /* Initialize the driver data */
     spidata->spi = spi;
     mutex_init(&spidata->buf_lock);

     channel = &gv7601_channel_info[0];
     channel->spidata = spidata;
     sd = &channel->sd;

     v4l2_spi_subdev_init( sd, spi, &gv7601_ops );

#if 1
     if ( 0 == status ) {
          status = alloc_chrdev_region(&dev_entry, 0, 1, DRIVER_NAME);
          if (status < 0) {
               printk("\ngv7601: Module intialization failed.\
		could not register character device");
               return -ENODEV;
          }
          /* Initialize of character device */
          cdev_init(&c_dev, &gv7601_char_fops);
          c_dev.owner = THIS_MODULE;
          c_dev.ops = &gv7601_char_fops;

          /* addding character device */
          status = cdev_add(&c_dev, dev_entry, 1);

          if (status) {
               printk(KERN_ERR "gv7601 :Error %d adding cdev\
				 ..error no:", status);
               unregister_chrdev_region(dev_entry, 1);
               return status;
          }

          /* registeration of     character device */
          register_chrdev(MAJOR(dev_entry), DRIVER_NAME, &gv7601_char_fops);
     }

     if ( 0 == status ) {
	gv7601_class = class_create(THIS_MODULE, "gv7601");

	if (!gv7601_class) {
             cdev_del(&c_dev);
             unregister_chrdev_region(dev_entry, 1);
             unregister_chrdev(MAJOR(dev_entry), DRIVER_NAME);

             status =-EIO;
	}
     }

     if ( 0 == status ) {
          device_create(gv7601_class, &spi->dev, dev_entry, channel, DRIVER_NAME);
     }
#endif

     if ( 0 == status ) {
             status = gv7601_initialize(sd);
     }

     printk (KERN_ERR "gv7601_probe returns %d\n",status );
     return status;
}

static int gv7601_remove(struct spi_device *spi)
{
	struct v4l2_subdev *sd = spi_get_drvdata(spi);
        struct gv7601_channel *ch  = NULL;

        ch = to_gv7601( sd );

        gv7601_deinitialize(sd);

        kfree( ch->spidata );
        ch->spidata = NULL;

	v4l2_device_unregister_subdev(sd);

	return 0;
}


static struct spi_driver gv7601_spi = {
	.driver = {
		.name =		"gv7601",
		.owner =	THIS_MODULE,
	},
	.probe =	gv7601_probe,
	.remove =	__devexit_p(gv7601_remove),

	/* NOTE:  suspend/resume methods are not necessary here.
	 * We don't do anything except pass the requests to/from
	 * the underlying controller.  The refrigerator handles
	 * most issues; the controller driver handles the rest.
	 */
};


static int __init gv7601_init(void)
{
     int status;

     status = spi_register_driver(&gv7601_spi);

     return status;
}

static void __exit gv7601_exit(void)
{
	spi_unregister_driver(&gv7601_spi);
}






module_init(gv7601_init);
module_exit(gv7601_exit);
MODULE_LICENSE("GPL");
