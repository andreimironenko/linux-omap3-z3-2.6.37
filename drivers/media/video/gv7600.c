#define DEBUG
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
//#include <asm/arch/dm368_fpga.h>

#include <media/v4l2-ioctl.h>
#include <linux/videodev2.h>
#include <media/v4l2-device.h>
#include <media/v4l2-chip-ident.h>
#include <asm/uaccess.h>

#include "gv7600.h"

#define GV7600_SPI_TRANSFER_MAX 1024

//#define GV7600_ENABLE_ANCILLARY_DATA

#define DRIVER_NAME     "gv7600"

/* Debug functions */
static int debug = 0;
module_param(debug, int, 0644);
MODULE_PARM_DESC(debug, "Debug level (0-2)");

/* Function Prototypes */
static int gv7600_initialize(struct v4l2_subdev *sd);
static int gv7600_deinitialize(struct v4l2_subdev *sd);
static int gv7600_s_std(struct v4l2_subdev *sd, v4l2_std_id std);
static int gv7600_s_stream(struct v4l2_subdev *sd, int enable);


struct gv7600_spidata {
	struct spi_device	*spi;

	struct mutex		buf_lock;
	unsigned		users;
	u8			*buffer;
};

struct gv7600_params {
	v4l2_std_id std;
	int         inputidx;
	struct v4l2_format fmt;
};

struct gv7600_channel {
     struct v4l2_subdev    sd;
     struct gv7600_spidata *spidata;
     struct gv7600_params   params;
};


static struct gv7600_channel gv7600_channel_info[GV7600_NUM_CHANNELS] = {
     [0] = { 
          .spidata = NULL,
     },
};


static atomic_t reference_count = ATOMIC_INIT(0);

/* For registeration of	charatcer device*/
static struct cdev c_dev;
/* device structure	to make	entry in device*/
static dev_t dev_entry;

static struct class *gv7600_class;

static const struct v4l2_subdev_video_ops gv7600_video_ops = {
//	.querystd = gv7600_querystd,
	.s_stream = gv7600_s_stream,
//	.g_input_status = gv7600_g_input_status,
};

static const struct v4l2_subdev_core_ops gv7600_core_ops = {
        .g_chip_ident = NULL,
	.s_std = gv7600_s_std,
};

static const struct v4l2_subdev_ops gv7600_ops = {
	.core = &gv7600_core_ops,
	.video = &gv7600_video_ops,
};



static inline struct gv7600_channel *to_gv7600(struct v4l2_subdev *sd)
{
	return container_of(sd, struct gv7600_channel, sd);
}


static struct spi_device *gv7600_get_spi( struct gv7600_channel *ch )
{
     if ( ch == NULL 
          || ch->spidata == NULL ) 
          return NULL;

     return ch->spidata->spi;
         
}

static struct device *gv7600_get_dev( struct gv7600_channel *ch )
{
     if ( ch == NULL 
          || ch->spidata == NULL  
          || ch->spidata->spi == NULL ) 
          return NULL;

     return &ch->spidata->spi->dev;
}

int gv7600_read_buffer(struct spi_device *spi, u16 offset, u16 *values, int length )
{
     struct spi_message msg;
     struct spi_transfer spi_xfer;
     int status;
     u16 txbuf[GV7600_SPI_TRANSFER_MAX+1] = {
          0x9000,  // read, auto-increment
     };
     u16 rxbuf[GV7600_SPI_TRANSFER_MAX+1];

     if ( NULL == spi ) {
          return -ENODEV;
     }

     if ( length > GV7600_SPI_TRANSFER_MAX ) {
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

int gv7600_read_register(struct spi_device *spi, u16 offset, u16 *value)
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

int gv7600_write_buffer(struct spi_device *spi, u16 offset, u16 *values, int length)
{
     struct spi_message msg;
     struct spi_transfer spi_xfer;
     int status;
     u16 txbuf[GV7600_SPI_TRANSFER_MAX] = {
          0x1000,  // write, auto-increment
     };
     u16 rxbuf[GV7600_SPI_TRANSFER_MAX] = {
     };

     if ( NULL == spi ) {
          return -ENODEV;
     }

     if ( length > GV7600_SPI_TRANSFER_MAX-1 ) {
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


int gv7600_write_register(struct spi_device *spi, u16 offset, u16 value)
{
     return gv7600_write_buffer(spi, offset, &value, 1);
}


static int gv7600_dump_regs(struct spi_device *spi)
{
        u16 value;
        int offset; 

        for ( offset=0; offset < 64; offset++ ) {
                if ( offset == 5
                     || offset == 6
                     || (0x08<=offset && offset<=0x11)
                     || (0x16<=offset && offset<=0x23)
                     || (0x2e<=offset && offset<=0x3f) )
                        continue;
                     
                if ( 0 == gv7600_read_register( spi, offset, &value ) ) {
                        if ( value != 0 ) {
                                dev_dbg( &spi->dev, "vid_core offset %u value x%04x\n",
                                         (unsigned int)offset, (unsigned int)value );
                        }
                }
        }

        for (offset=0x20e; offset<=0x210; offset+=2 ) {
                if ( 0 == gv7600_read_register( spi, offset, &value ) ) {
                        if ( value != 0 ) {
                                dev_dbg( &spi->dev, "drive strength offset %u value x%04x\n",
                                         (unsigned int)offset, (unsigned int)value );
                        }
                }
        }


        for ( offset=0x800; offset < 0x810; offset++ ) {
                if ( 0 == gv7600_read_register( spi, offset, &value ) ) {
                        if ( value != 0 ) {
                                dev_dbg( &spi->dev, "hd_audio offset %u value x%04x\n",
                                         (unsigned int)offset, (unsigned int)value );
                        }
                }
        }
        
        for ( offset=0x400; offset < 0x410; offset++ ) {
                if ( 0 == gv7600_read_register( spi, offset, &value ) ) {
                        if ( value != 0 ) {
                                dev_dbg( &spi->dev, "sd_audio offset %u value x%04x\n",
                                         (unsigned int)offset, (unsigned int)value );
                        }
                }
                
        }

        return 0;
}



/* gv7600_initialize :
 * This function will set the video format standard
 */
static int gv7600_initialize(struct v4l2_subdev *sd)
{
        struct gv7600_channel *ch;
        struct spi_device *spi;
        int status = 0;

        ch = to_gv7600(sd);
     
        try_module_get(THIS_MODULE);


        spi = gv7600_get_spi( ch );

        if ( 0 ) {
                gv7600_dump_regs( spi );
        }


        return status;
}

static int gv7600_deinitialize(struct v4l2_subdev *sd)
{
        struct gv7600_channel *ch;
        struct device *dev;

        ch = to_gv7600( sd );

        dev = gv7600_get_dev( ch );
        if ( NULL != dev ) {
             dev_dbg( dev, "GV7600 deinitialization\n");
        }

        module_put(THIS_MODULE);

        return 0;
}

static int gv7600_s_std(struct v4l2_subdev *sd, v4l2_std_id std)
{
        
	v4l2_dbg(1, debug, sd, "Start of gv7600_setstd 0x%llx..\n", std );
	v4l2_dbg(1, debug, sd, "End of gv7600 set standard...\n");

        return 0;
}



static int gv7600_s_stream(struct v4l2_subdev *sd, int enable)
{
	v4l2_dbg(1, debug, sd, "Start of gv7600_s_stream %d\n", enable );

        return 0;
}



/*-------------------------------------------------------------------------*/

int gv7600_char_open (struct inode *inode, struct file *file)
{
        int ch_id = 0; //iminor(inode)
        struct gv7600_channel *ch;

 	if (file->f_mode == FMODE_WRITE) {
		printk (KERN_ERR "TX Not supported\n");
		return -EACCES;
	}

        if ( ch_id >= GV7600_NUM_CHANNELS ){
            printk(KERN_ERR "gv7600: Bad channel ID\n");
             return -EINVAL;
        }

        ch = &gv7600_channel_info[ch_id];

        file->private_data =  ch;

	if (atomic_inc_return(&reference_count) > 1) {
                printk(KERN_ERR "gv7600: already open\n");
		atomic_dec(&reference_count);
		return -EACCES;
	}

	return 0;
}


int gv7600_char_release (struct inode *inode, struct file *file)
{
	atomic_dec(&reference_count);
	return 0;
}



ssize_t gv7600_char_read (struct file *file, 
                          char __user *buff, 
                          size_t size, 
                          loff_t *loff)
{

     struct gv7600_channel *channel = file->private_data;
     struct spi_device     *spi;

     if ( NULL == channel ) {
          printk( KERN_ERR "gv7600: No channel\n" );
          return -ENODEV;
     }

     if ( NULL == (spi = gv7600_get_spi(channel)) ) {
          printk( KERN_ERR "gv7600: No spi dev\n" );
          return -ENODEV;
     }


     return -EFAULT;
}



long gv7600_char_ioctl ( struct file *filp, unsigned int cmd, unsigned long arg )
{
     return -ENOTTY;
}

static struct file_operations gv7600_char_fops = {
     .owner   = THIS_MODULE,
     .open    = gv7600_char_open,
     .read    = gv7600_char_read,
     .release = gv7600_char_release,
     .unlocked_ioctl   = gv7600_char_ioctl,
};



/*-------------------------------------------------------------------------*/


static int gv7600_probe(struct spi_device *spi)
{
     struct gv7600_spidata	*spidata;
     int			status = 0;
     struct gv7600_channel      *channel;
     struct v4l2_subdev         *sd;

     dev_dbg( &spi->dev, "Enter gv7600_probe\n" );

     /* Allocate driver data */
     spidata = kzalloc(sizeof(*spidata), GFP_KERNEL);
     if (!spidata)
          return -ENOMEM;

     /* Initialize the driver data */
     spidata->spi = spi;
     mutex_init(&spidata->buf_lock);

     channel = &gv7600_channel_info[0];
     channel->spidata = spidata;
     sd = &channel->sd;

     v4l2_spi_subdev_init( sd, spi, &gv7600_ops );

#if 1
     if ( 0 == status ) {
          status = alloc_chrdev_region(&dev_entry, 0, 1, DRIVER_NAME);
          if (status < 0) {
               printk("\ngv7600: Module intialization failed.\
		could not register character device");
               return -ENODEV;
          }
          /* Initialize of character device */
          cdev_init(&c_dev, &gv7600_char_fops);
          c_dev.owner = THIS_MODULE;
          c_dev.ops = &gv7600_char_fops;

          /* addding character device */
          status = cdev_add(&c_dev, dev_entry, 1);

          if (status) {
               printk(KERN_ERR "gv7600 :Error %d adding cdev\
				 ..error no:", status);
               unregister_chrdev_region(dev_entry, 1);
               return status;
          }

          /* registeration of     character device */
          register_chrdev(MAJOR(dev_entry), DRIVER_NAME, &gv7600_char_fops);
     }

     if ( 0 == status ) {
	gv7600_class = class_create(THIS_MODULE, "gv7600");

	if (!gv7600_class) {
             cdev_del(&c_dev);
             unregister_chrdev_region(dev_entry, 1);
             unregister_chrdev(MAJOR(dev_entry), DRIVER_NAME);

             status =-EIO;
	}
     }

     if ( 0 == status ) {
          device_create(gv7600_class, &spi->dev, dev_entry, channel, DRIVER_NAME);
     }
#endif

     if ( 0 == status ) {
             status = gv7600_initialize(sd);
     }

     printk (KERN_ERR "gv7600_probe returns %d\n",status );
     return status;
}

static int gv7600_remove(struct spi_device *spi)
{
	struct v4l2_subdev *sd = spi_get_drvdata(spi);
        struct gv7600_channel *ch  = NULL;

        ch = to_gv7600( sd );

        gv7600_deinitialize(sd);

        kfree( ch->spidata );
        ch->spidata = NULL;

	v4l2_device_unregister_subdev(sd);

	return 0;
}


static struct spi_driver gv7600_spi = {
	.driver = {
		.name =		"gv7600",
		.owner =	THIS_MODULE,
	},
	.probe =	gv7600_probe,
	.remove =	__devexit_p(gv7600_remove),

	/* NOTE:  suspend/resume methods are not necessary here.
	 * We don't do anything except pass the requests to/from
	 * the underlying controller.  The refrigerator handles
	 * most issues; the controller driver handles the rest.
	 */
};


static int __init gv7600_init(void)
{
     int status;

     status = spi_register_driver(&gv7600_spi);

     return status;
}

static void __exit gv7600_exit(void)
{
	spi_unregister_driver(&gv7600_spi);
}






module_init(gv7600_init);
module_exit(gv7600_exit);
MODULE_LICENSE("GPL");
