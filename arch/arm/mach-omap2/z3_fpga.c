#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/ctype.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/io.h>
#include <linux/proc_fs.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/clk.h>

#include <asm/uaccess.h>
#include <mach/gpio.h>
#include <mach/hardware.h>
#include <mach/z3_app.h>
#include <mach/z3_fpga.h>

#include "mux.h"
#include "cm81xx.h"

static void *__iomem z3_fpga_base_addr;
static void *__iomem z3_latch_base_addr;
static void *__iomem z3_tppss_gbl_base_addr;
static void *__iomem z3_tppss_stc0_27mcnt;

static u16 z3_fpga_latch1_value;
static u16 z3_fpga_latch2_value;

static int z3_fpga_latch1_offset = 0;
static int z3_fpga_latch2_offset = 8;

static enum z3_fpga_pll_setting z3_fpga_pll_setting = Z3_FPGA_PLL_VIDIN_74M176;

//static int z3_fpga_audio_enable_value = 0;
//static enum z3_fpga_audio_mode z3_fpga_audio_select_value = Z3_FPGA_AUDIO_TRISTATE;

static int z3_fpga_aic_ext_master = 0;

static struct timer_list z3_fpga_timer;
static unsigned int z3_fpga_timeout_ms = 60000;

/* Sample rate converter setting */
static enum z3_fpga_audio_src   z3_fpga_audio_src = Z3_FPGA_AUDIO_SRC_ENABLE;

static enum z3_fpga_audio_input z3_fpga_audio_input = Z3_FPGA_AUDIO_INPUT_1_2;

static unsigned int z3_fpga_aic_ext_master_div_mask = 0x2000; // divide by 8 (48kHz)

static unsigned int z3_fpga_pll_load_pf = 6;

static unsigned int z3_fpga_pll_load_scanned = 0;

static int z3_fpga_load_pf_min_lock = -1;
static int z3_fpga_load_pf_max_lock = -1;


#define z3_fpga_valid_board_id() \
     (z3_fpga_board_id_value != 0xffff)

#define z3_fpga_valid_sdi_board_id() \
     (z3_fpga_board_id_value == Z3_BOARD_ID_APP_31)

#define z3_fpga_valid_aic_disable_board_id() \
     (z3_fpga_board_id_value == Z3_BOARD_ID_APP_31 || z3_fpga_board_id_value == Z3_BOARD_ID_APP_02)

static u16 z3_fpga_board_id_value = 0xffff;


static struct i2c_client *z3_fpga_pll_client = NULL;

static struct mutex z3_fpga_pll_load_scan_mutex;

static unsigned int z3_fpga_pll_ctrl_divide = 0;
static unsigned int z3_fpga_pll_ctrl_vidin  = 0;

static const char *z3_fpga_tso_pll_clk_name = "audio_pll_clk2_ck";

/* I2C - for APP-03 and up */
/*
 */

static int z3_fpga_pll_adapter_id = 1;
static struct i2c_board_info pll_board_info = {
        .type = "PLL",
        .addr = 0x64,
        .platform_data = NULL,
};


int z3_fpga_read(u32 offset)
{
        int retval;

	if ( z3_fpga_base_addr != 0) {
                if (offset < Z3_FPGA_ADDR_SIZE) {
                        retval = __raw_readw(z3_fpga_base_addr + offset);
//                        printk(KERN_ERR "read fpga offset x%x value x%x\n",
//                               (unsigned int)offset, (unsigned int) retval);
                        
                        return retval;
                }
                else
                        printk(KERN_ERR "offset exceeds Z3 fpga address space\n");
	}
	return -1;
}
EXPORT_SYMBOL(z3_fpga_read);

int z3_fpga_latch_write(u16 *value)
{       
	if ( z3_latch_base_addr != 0) {
                if  ( value == &z3_fpga_latch1_value ) {
                        __raw_writew(*value, z3_latch_base_addr + z3_fpga_latch1_offset);
                } else {

                        /* Latch2 only present on certain boards */
                        if ( !z3_fpga_valid_sdi_board_id() )
                                return 0;

                        if ( cpu_is_ti816x() ) {
                                omap_mux_init_signal( "gpmc_a3", TI81XX_MUX_PULLDIS );
                        }

                        __raw_writew(*value, z3_latch_base_addr + z3_fpga_latch2_offset);

                        if ( cpu_is_ti816x() ) {
                                if ( 0 == (Z3_APP31_Y2_FPGA_PROG & z3_fpga_latch1_value) ) {
                                        omap_mux_init_signal( "gp0_io11", TI81XX_MUX_PULLDIS );
                                }
                        }
                }
	}

	return 0;
}
EXPORT_SYMBOL(z3_fpga_write);


int z3_fpga_latch1_read(u16 *value)
{
        int ret = -1;

	if ( z3_latch_base_addr != 0 ) {
                *value = __raw_readw(z3_latch_base_addr + z3_fpga_latch1_offset);
                ret = 0;
        }

	return ret;
}

int z3_fpga_read_version(void)
{
	if (z3_fpga_base_addr != 0) {
                return __raw_readw(z3_fpga_base_addr + Z3_FPGA_VERSION_READ_OFFSET);
	}
	return -1;
}



int z3_fpga_write(u16 val, u32 offset)
{
	if ( z3_fpga_base_addr != 0) {
             if (offset < Z3_FPGA_ADDR_SIZE) {
//                     printk(KERN_ERR "write fpga offset x%x value x%x\n",
//                            (unsigned int)offset, (unsigned int) val );

                     __raw_writew(val, z3_fpga_base_addr + offset);
                     return val;
             } else
                  printk(KERN_ERR "offset exceeds Z3 fpga address space\n");
	}
	return -1;
}


int 
z3_fpga_i2c_pll_wr(u8 offset, u8 byte)
{
        int ret = 0;
        struct i2c_msg msg[1];
        unsigned char writedata[2];
        int retries = 3;
        int err;

        if ( z3_fpga_pll_client == NULL ) {
                ret = -ENODEV;
        } else if ( z3_fpga_pll_client->adapter == NULL ) {             
                ret = -ENODEV;
        }
          
        if ( 0 == ret ) {
                do { 
                        msg[0].addr = z3_fpga_pll_client->addr;
                        msg[0].flags = 0;
                        msg[0].len = 2;
                        msg[0].buf = writedata;

                        writedata[0] = offset | 0x80; //bit 7 is byte operation flag
                        writedata[1] = byte;

                        err = i2c_transfer(z3_fpga_pll_client->adapter, msg, 1);
                        ret = (err<0) ? err : 0;

                        if ( err < 0 ) {
                                msleep(10);
                        }               
                } while ( err < 0 && --retries > 0);
        }

        if ( retries <= 0 )
                return -EIO;
          

        printk( KERN_DEBUG "PLL %02x <= %02x ret %d\n", offset, byte, ret );
        return ret;
}
EXPORT_SYMBOL(z3_fpga_i2c_pll_wr);


int 
z3_fpga_pll_setup(enum z3_fpga_pll_setting pll_setting)
{
     int ret = 0;

/* Setup CDCE925 PLL so that
 Y0: 27.000 MHz
 Y3: 12.288 MHz
 Y5: 74.25 MHz or 74.176 MHz
*/

     switch ( pll_setting ) {
     case Z3_FPGA_PLL_VIDOUT_74M176:
          // SDI out needs a steady clock => disable VCXO
          ret |= z3_fpga_i2c_pll_wr( 0x01, 0x00);
          break;

     default:
          ret |= z3_fpga_i2c_pll_wr( 0x01, 0x04 );
          break;
     }

     if ( Z3_FPGA_PLL_OUTPUT_DISABLE == pll_setting ) {
             ret |= z3_fpga_i2c_pll_wr( 0x02, 0x80 );
     } else {
             ret |= z3_fpga_i2c_pll_wr( 0x02, 0x34 );
     }
     ret |= z3_fpga_i2c_pll_wr( 0x04, 0x02 );
     ret |= z3_fpga_i2c_pll_wr( 0x05, z3_fpga_pll_load_pf*8 );	// Crystal load capacitance
     ret |= z3_fpga_i2c_pll_wr( 0x06, 0x60 );

     if ( Z3_FPGA_PLL_OUTPUT_DISABLE == pll_setting ) {
             ret |= z3_fpga_i2c_pll_wr( 0x14, 0x80 );
     } else {
             ret |= z3_fpga_i2c_pll_wr( 0x14, 0x6d );
     }
     ret |= z3_fpga_i2c_pll_wr( 0x16, 0x29 );
     ret |= z3_fpga_i2c_pll_wr( 0x17, 0x09 );
     ret |= z3_fpga_i2c_pll_wr( 0x18, 0x60 );
     ret |= z3_fpga_i2c_pll_wr( 0x19, 0x04 );
     ret |= z3_fpga_i2c_pll_wr( 0x1a, 0x82 );


     switch ( pll_setting ) {
     case Z3_FPGA_PLL_VIDOUT_27M:
          ret |= z3_fpga_i2c_pll_wr( 0x24, 0xef ); // PLL Bypass
          ret |= z3_fpga_i2c_pll_wr( 0x26, 0x08 ); 
          ret |= z3_fpga_i2c_pll_wr( 0x27, 0x01 ); // divisor of 1
          break;

     case Z3_FPGA_PLL_OUTPUT_DISABLE:
          ret |= z3_fpga_i2c_pll_wr( 0x24, 0x80 ); // PLL Bypass
          ret |= z3_fpga_i2c_pll_wr( 0x26, 0x08 ); 
          ret |= z3_fpga_i2c_pll_wr( 0x27, 0x01 ); // divisor of 1
          break;

     case Z3_FPGA_PLL_VIDIN_148M5:
     case Z3_FPGA_PLL_VIDIN_148M352:
          ret |= z3_fpga_i2c_pll_wr( 0x24, 0x6f );
          ret |= z3_fpga_i2c_pll_wr( 0x26, 0x00 );
          ret |= z3_fpga_i2c_pll_wr( 0x27, 0x01 );
          break;

     default:
          ret |= z3_fpga_i2c_pll_wr( 0x24, 0x6f );
          ret |= z3_fpga_i2c_pll_wr( 0x26, 0x08 );
          ret |= z3_fpga_i2c_pll_wr( 0x27, 0x03 );
          break;
     }

     switch ( pll_setting ) {
     case Z3_FPGA_PLL_VIDIN_74M25:
          ret |= z3_fpga_i2c_pll_wr( 0x28, 0xff );
          ret |= z3_fpga_i2c_pll_wr( 0x29, 0xc7 );
          ret |= z3_fpga_i2c_pll_wr( 0x2a, 0xc2 );
          ret |= z3_fpga_i2c_pll_wr( 0x2b, 0x07 );
          break;

     case Z3_FPGA_PLL_VIDIN_148M5:
          ret |= z3_fpga_i2c_pll_wr( 0x28, 0xaf );
          ret |= z3_fpga_i2c_pll_wr( 0x29, 0x50 );
          ret |= z3_fpga_i2c_pll_wr( 0x2a, 0x02 );
          ret |= z3_fpga_i2c_pll_wr( 0x2b, 0xc9 );
          break;

     case Z3_FPGA_PLL_VIDIN_148M352:
          ret |= z3_fpga_i2c_pll_wr( 0x28, 0x9c );
          ret |= z3_fpga_i2c_pll_wr( 0x29, 0x4d );
          ret |= z3_fpga_i2c_pll_wr( 0x2a, 0xea );
          ret |= z3_fpga_i2c_pll_wr( 0x2b, 0xa9 );
          break;

     default:
          ret |= z3_fpga_i2c_pll_wr( 0x28, 0xea );
          ret |= z3_fpga_i2c_pll_wr( 0x29, 0x66 );
          ret |= z3_fpga_i2c_pll_wr( 0x2a, 0xe2 );
          ret |= z3_fpga_i2c_pll_wr( 0x2b, 0x07 );
          break;
     }


     switch ( pll_setting ) {
     case Z3_FPGA_PLL_VIDIN_13M5:
          z3_fpga_pll_ctrl_divide = 0x01;
          break;

     case Z3_FPGA_PLL_VIDIN_27M:
          z3_fpga_pll_ctrl_divide = 0x11;
          break;


     case Z3_FPGA_PLL_VIDIN_148M5:
     case Z3_FPGA_PLL_VIDIN_148M352:
          z3_fpga_pll_ctrl_divide = 0x3b;
          break;

     default:
          // PLL_PCLK reference
          z3_fpga_pll_ctrl_divide = 0x2a;
          break;
     }

     z3_fpga_write( z3_fpga_pll_ctrl_vidin|z3_fpga_pll_ctrl_divide, Z3_FPGA_PLL_CTRL_OFFSET);

     if ( 0 == ret )
          z3_fpga_pll_setting = pll_setting;

     return ret;
}
EXPORT_SYMBOL(z3_fpga_pll_setup);


int z3_fpga_si_20bit( int enable )
{
     if ( enable ) {
             z3_fpga_latch2_value |=  Z3_APP31_Y3_SI_20BIT;
     } else {
             z3_fpga_latch2_value &=  ~Z3_APP31_Y3_SI_20BIT;
     }

     return z3_fpga_latch_write( &z3_fpga_latch2_value );
}
EXPORT_SYMBOL(z3_fpga_si_20bit);

int z3_fpga_so_set( unsigned int value )
{
     value &= Z3_FPGA_LATCH_SO_MASK;

     z3_fpga_latch2_value = (z3_fpga_latch2_value & ~Z3_FPGA_LATCH_SO_MASK) | value;

     return z3_fpga_latch_write( &z3_fpga_latch2_value );
}
EXPORT_SYMBOL(z3_fpga_so_set);



void z3_fpga_stc_read(u64 *base, u32 *ext, struct timespec *ptimespec, u64 *stc0_27mhz, u32 *gptimer_27mhz)
{
     int dummy_value=0;
     unsigned long flags;

     static u64 oldbase;
     static u64 wrapcount;
     static int oldbasevalid;
     static u64 stc0_27mhz_wrapcount;
     static u64 stc0_27mhz_old;
     static int stc0_27mhz_valid;

     u32    stc0_27mhz_current = 0;
     int    has_tppss = 0;
     int    is_ti816x_predicate = 0;
     u32    gptimer2_27mhz_current = 0;

     is_ti816x_predicate = cpu_is_ti816x();

     if ( is_ti816x_predicate ) {
             if ( z3_tppss_stc0_27mcnt != NULL 
                  && 0 == (__raw_readl(TI816X_CM_DEFAULT_TPPSS_CLKCTRL) & 0x30000 ) ) {
                     has_tppss = 1;
             }
     }
     local_irq_save(flags);

     getnstimeofday(ptimespec);

     /* Write to latch value */
     z3_fpga_write( dummy_value, Z3_FPGA_STC_EXT_OFFSET );

     if ( has_tppss ) {
             stc0_27mhz_current = __raw_readl( z3_tppss_stc0_27mcnt );
     }


     if ( is_ti816x_predicate ) {
             gptimer2_27mhz_current = __raw_readl( TI81XX_L4_SLOW_IO_ADDRESS(0x4804003c) );
     }

     /* Read values */
     (*base) = (z3_fpga_read( Z3_FPGA_STC_BASE_H_OFFSET ) & Z3_FPGA_STC_BASE_H_MASK);
     (*base) <<= 16;
     (*base) |= z3_fpga_read( Z3_FPGA_STC_BASE_M_OFFSET );
     (*base) <<= 16;
     (*base) |= z3_fpga_read( Z3_FPGA_STC_BASE_L_OFFSET );

     (*ext) = z3_fpga_read( Z3_FPGA_STC_EXT_OFFSET );

     local_irq_restore(flags);


     if ( oldbasevalid ) {
          if ( ((oldbase >> 32) & 1) == 1
               && (((*base) >> 32) & 1) == 0 ) {
               wrapcount += (1ULL<<33);
          }
     }
     oldbase = *base;
     oldbasevalid = 1;

     (*base) += wrapcount;

     /* Now FPGA is restamping and comparing PCR to STC .... */
     /* (*base) += 0x1feffffffULL; // detect wrap-around issues early */


     if ( z3_tppss_stc0_27mcnt ) {
             if ( stc0_27mhz_valid ) {
                     if ( ((stc0_27mhz_old >> 31) & 1) == 1
                          && ((stc0_27mhz_current >> 31) & 1) == 0 ) {
                             stc0_27mhz_wrapcount += (1ULL<<32);
                     }
             }

             stc0_27mhz_valid = 1;
             stc0_27mhz_old = stc0_27mhz_current;

             *stc0_27mhz = stc0_27mhz_current;
             *stc0_27mhz += stc0_27mhz_wrapcount;
     } else {
             *stc0_27mhz = 0;
     }


     *gptimer_27mhz = gptimer2_27mhz_current;
}
EXPORT_SYMBOL(z3_fpga_stc_read);


void z3_fpga_tso_pcr_delta_read(s64 *delta_27mhz_ticks)
{
        s16 high = 0;
        u32 low  = 0;
        s64 retval;
        
	if ( z3_fpga_base_addr != 0) {
                low = __raw_readl( z3_fpga_base_addr + Z3_FPGA_TSO_STC_PCR_DELTA_LOW );
                
                high = __raw_readw( z3_fpga_base_addr + Z3_FPGA_TSO_STC_PCR_DELTA_HIGH );
        }

        retval = high;
        retval <<= 16;
        retval += low;

        *delta_27mhz_ticks = retval;
}
EXPORT_SYMBOL(z3_fpga_tso_pcr_delta_read);

void z3_fpga_set_pll_vidin_reference( enum z3_fpga_pll_vidin_reference ref)
{
        switch (ref)
        {
                
        case Z3_FPGA_PLL_VIDIN0_REFERENCE:
        default:
                z3_fpga_pll_ctrl_vidin = 0;
                break;

        case Z3_FPGA_PLL_VIDIN1_REFERENCE:
                z3_fpga_pll_ctrl_vidin = 0x100;
                break;
        }

        z3_fpga_write( z3_fpga_pll_ctrl_vidin|z3_fpga_pll_ctrl_divide, Z3_FPGA_PLL_CTRL_OFFSET);
}
EXPORT_SYMBOL(z3_fpga_set_pll_vidin_reference);



#ifdef CONFIG_PROC_FS

static int
z3_fpga_tso_pcr_delta_proc_read(char *page, char **start, off_t off, int count, int *eof,
		    void *data)
{
     char *p = page;
     int len;

     int retries;

     s64 val1; 
     s64 val2;

     
     z3_fpga_tso_pcr_delta_read( &val1 );
     for ( retries = 0, val2=val1^1;
           val2 != val1 ;
           retries++ ) {

             if ( retries >= 3 ) { 
                     printk( KERN_ERR "%s: value not stable\n", __FUNCTION__ );

                     return -EINVAL;
             }

             z3_fpga_tso_pcr_delta_read( &val2 );
     }

     p += sprintf(p, "%lld\n", val2 );
     len = (p - page) - off;
     if (len < 0)
          len = 0;

     *eof = (len <= count) ? 1 : 0;
     *start = page + off;

     return len;
}

static int
z3_fpga_stc_proc_read(char *page, char **start, off_t off, int count, int *eof,
		    void *data)
{
     char *p = page;
     int len;
     u64 base;
     u32 ext;
     struct timespec ts;
     u64 ns;

     u64 stc0_27mhz;

     u32 gptimer2_27mhz;

     z3_fpga_stc_read(&base, &ext, &ts, &stc0_27mhz, &gptimer2_27mhz);

     ns = ts.tv_sec;
     ns *= NSEC_PER_SEC;
     ns += ts.tv_nsec;

     p += sprintf(p, "%10llu:%03u %llu %llu %lu\n", base, ext, ns, stc0_27mhz, (unsigned long)gptimer2_27mhz );
     len = (p - page) - off;
     if (len < 0)
          len = 0;

     *eof = (len <= count) ? 1 : 0;
     *start = page + off;

     return len;
}



static int
z3_fpga_proc_read(u16 value, char *page, char **start, off_t off, int count, int *eof,
		    void *data)
{
	char *p = page;
	int len;

        p += sprintf(p, "0x%04x\n", value );
	len = (p - page) - off;
	if (len < 0)
		len = 0;

	*eof = (len <= count) ? 1 : 0;
	*start = page + off;

	return len;
}

static int z3_fpga_proc_write(const char *reg_name, int reg_offset,
                                 struct file *file, const char __user *buffer,
                                 unsigned long count, void *data)
{
        char scanbuf[32];
        int  len = 0;
        int  status;
        unsigned long new_latch_value;

        len = count;
        if ( len > sizeof(scanbuf)-1 ) {
             len = sizeof(scanbuf)-1;
        }

        if ( copy_from_user( scanbuf, buffer, len ) ) {
             return -EFAULT;
        }

        scanbuf[len] = '\0';

        status = strict_strtoul( scanbuf, 0, &new_latch_value );
        if ( status ) {
             printk( KERN_ERR "/proc/fpga/%s: Invalid value %s\n", reg_name, scanbuf );
             return status;
        }

        if ( new_latch_value >= 65536 ) {
             printk( KERN_ERR "/proc/fpga/%s: Invalid value %s\n", reg_name, scanbuf );
             return -EINVAL;
        }

        z3_fpga_write( (u16) new_latch_value, reg_offset);
        
	return count;
}


static int
z3_fpga_latch_proc_read(char *page, char **start, off_t off, int count, int *eof,
                        void *data, u16 *latch_value)
{

     return z3_fpga_proc_read( *latch_value,
                               page, start, off, count, eof, data );
}

static int 
z3_fpga_latch_proc_write(const char *reg_name, struct file *file, const char __user *buffer,
                         unsigned long count, void *data, u16 *latch_value)
{
        char scanbuf[32];
        int  len = 0;
        int  status;
        unsigned long new_latch_value;

        len = count;
        if ( len > sizeof(scanbuf)-1 ) {
             len = sizeof(scanbuf)-1;
        }

        if ( copy_from_user( scanbuf, buffer, len ) ) {
             return -EFAULT;
        }

        scanbuf[len] = '\0';

        status = strict_strtoul( scanbuf, 0, &new_latch_value );
        if ( status ) {
             printk( KERN_ERR "/proc/fpga/%s: Invalid value %s\n", reg_name, scanbuf );
             return status;
        }

        if ( new_latch_value >= 65536 ) {
             printk( KERN_ERR "/proc/fpga/%s: Invalid value %s\n", reg_name, scanbuf );
             return -EINVAL;
        }

        *latch_value = new_latch_value; 
        z3_fpga_latch_write( latch_value );
        
	return count;
}

static int 
z3_fpga_latch_proc_write_mask(const char *reg_name, struct file *file, const char __user *buffer,
                              unsigned long count, void *data, u16 *latch_value, u16 mask)
{
        char scanbuf[32];
        int  len = 0;
        int  status;
        unsigned long new_latch_value;

        len = count;
        if ( len > sizeof(scanbuf)-1 ) {
             len = sizeof(scanbuf)-1;
        }

        if ( copy_from_user( scanbuf, buffer, len ) ) {
             return -EFAULT;
        }

        scanbuf[len] = '\0';

        status = strict_strtoul( scanbuf, 0, &new_latch_value );
        if ( status ) {
             printk( KERN_ERR "/proc/fpga/%s: Invalid value %s\n", reg_name, scanbuf );
             return status;
        }

        if ( new_latch_value >= 65536 ) {
             printk( KERN_ERR "/proc/fpga/%s: Invalid value %s\n", reg_name, scanbuf );
             return -EINVAL;
        }

        new_latch_value = ((*latch_value) & ~mask) | (new_latch_value ? mask : 0 );

        *latch_value = new_latch_value; 
        z3_fpga_latch_write( latch_value );
        
	return count;
}


static int
z3_fpga_latch1_proc_read(char *page, char **start, off_t off, int count, int *eof,
                         void *data)
{
        return z3_fpga_latch_proc_read( page, start, off,
                                 count, eof, data, &z3_fpga_latch1_value );
}

static int 
z3_fpga_latch1_proc_write(struct file *file, const char __user *buffer,
                         unsigned long count, void *data)
{
        return z3_fpga_latch_proc_write( "latch1", file, buffer, 
                                  count, data, &z3_fpga_latch1_value );
}

static int
z3_fpga_latch2_proc_read(char *page, char **start, off_t off, int count, int *eof,
                         void *data)
{
        return z3_fpga_latch_proc_read( page, start, off,
                                 count, eof, data, &z3_fpga_latch2_value );
}


static int 
z3_fpga_latch2_proc_write(struct file *file, const char __user *buffer,
                         unsigned long count, void *data)
{
        return z3_fpga_latch_proc_write( "latch2", file, buffer, 
                                  count, data, &z3_fpga_latch2_value );
}



static int
z3_fpga_gpio_proc_read(int gpio, char *page, char **start, off_t off, int count, int *eof,
                       void *data)
{
        u16 value;

        value = gpio_get_value( gpio);

        return z3_fpga_proc_read( value,
                               page, start, off, count, eof, data );

}

static int z3_fpga_gpio_proc_write(char *reg_name, int gpio,
                                 struct file *file, const char __user *buffer,
                                 unsigned long count, void *data)
{
        char scanbuf[32];
        int  len = 0;
        int  status;
        unsigned long new_gpio_value;

        len = count;
        if ( len > sizeof(scanbuf)-1 ) {
             len = sizeof(scanbuf)-1;
        }

        if ( copy_from_user( scanbuf, buffer, len ) ) {
             return -EFAULT;
        }

        scanbuf[len] = '\0';

        status = strict_strtoul( scanbuf, 0, &new_gpio_value );
        if ( status ) {
             printk( KERN_ERR "/proc/fpga/%s: Invalid value %s\n", reg_name, scanbuf );
             return status;
        }

        gpio_set_value( gpio, new_gpio_value );
        
	return count;
}



static int
z3_fpga_fpga_initb_proc_read(char *page, char **start, off_t off, int count, int *eof,
		    void *data)
{

        return z3_fpga_gpio_proc_read( Z3_APP_31_GPIO_FPGA_INITB,
                                  page, start, off, count, eof, data );
}

static int 
z3_fpga_fpga_initb_proc_write(struct file *file, const char __user *buffer,
                            unsigned long count, void *data)
{
        return z3_fpga_gpio_proc_write( "fpga_initb", Z3_APP_31_GPIO_FPGA_INITB,
                                   file, buffer, count, data );
}


static int
z3_fpga_fpga_csb_proc_read(char *page, char **start, off_t off, int count, int *eof,
		    void *data)
{

        return z3_fpga_gpio_proc_read( Z3_APP_31_GPIO_FPGA_CSB,
                                  page, start, off, count, eof, data );
}

static int 
z3_fpga_fpga_csb_proc_write(struct file *file, const char __user *buffer,
                            unsigned long count, void *data)
{
        return z3_fpga_gpio_proc_write( "fpga_csb", Z3_APP_31_GPIO_FPGA_CSB,
                                   file, buffer, count, data );
}



static int
z3_fpga_asi_ctl_proc_read(char *page, char **start, off_t off, int count, int *eof,
		    void *data)
{

     return z3_fpga_proc_read( z3_fpga_read(Z3_FPGA_ASI_CTL_OFFSET),
                                  page, start, off, count, eof, data );
}

static int 
z3_fpga_asi_ctl_proc_write(struct file *file, const char __user *buffer,
                            unsigned long count, void *data)
{
     return z3_fpga_proc_write( "asi_ctl", Z3_FPGA_ASI_CTL_OFFSET,
                                   file, buffer, count, data );
}

static int
z3_fpga_tso_pcr_pid_proc_read(char *page, char **start, off_t off, int count, int *eof,
		    void *data)
{

     return z3_fpga_proc_read( z3_fpga_read(Z3_FPGA_TSO_PCR_PID_OFFSET),
                                  page, start, off, count, eof, data );
}

static int 
z3_fpga_tso_pcr_pid_proc_write(struct file *file, const char __user *buffer,
                            unsigned long count, void *data)
{
     return z3_fpga_proc_write( "tso_pcr_pid", Z3_FPGA_TSO_PCR_PID_OFFSET,
                                   file, buffer, count, data );
}


static int
z3_fpga_sdi_out_proc_read(char *page, char **start, off_t off, int count, int *eof,
		    void *data)
{

     return z3_fpga_proc_read( z3_fpga_read(Z3_FPGA_SDI_OUT_OFFSET),
                                  page, start, off, count, eof, data );
}

static int 
z3_fpga_sdi_out_proc_write(struct file *file, const char __user *buffer,
                            unsigned long count, void *data)
{
     return z3_fpga_proc_write( "sdi_out", Z3_FPGA_SDI_OUT_OFFSET,
                                   file, buffer, count, data );
}

static int
z3_fpga_fpga_prog_proc_read(char *page, char **start, off_t off, int count, int *eof,
		    void *data)
{
        u16 value;

        value = (z3_fpga_latch1_value & Z3_APP31_Y2_FPGA_PROG) ? 1 : 0;

        return z3_fpga_proc_read( value,
                                  page, start, off, count, eof, data );
}

static int
z3_fpga_fpga_prog_proc_write(struct file *file, const char __user *buffer,
                            unsigned long count, void *data)
{

        char scanbuf[32];
        int  len = 0;
        int  status;
        unsigned long new_latch_value;

        len = count;
        if ( len > sizeof(scanbuf)-1 ) {
             len = sizeof(scanbuf)-1;
        }

        if ( copy_from_user( scanbuf, buffer, len ) ) {
             return -EFAULT;
        }

        scanbuf[len] = '\0';

        status = strict_strtoul( scanbuf, 0, &new_latch_value );
        if ( status ) {
             printk( KERN_ERR "/proc/fpga/fpga_prog: Invalid value %s\n", scanbuf );
             return status;
        }

        if ( new_latch_value == 0 ) {
          if ( cpu_is_ti816x() ) {
                  /* Convert GPMC_A[3-6] to GPIO's for FPGA M[2:0] */
                  omap_mux_init_signal( "gp0_io11", TI81XX_MUX_PULLDIS );
                  omap_mux_init_signal( "gp0_io12", TI81XX_MUX_PULLDIS );
                  omap_mux_init_signal( "gp0_io13", TI81XX_MUX_PULLDIS );
                  omap_mux_init_signal( "gp0_io14", TI81XX_MUX_PULLDIS );
          }
        }

        return z3_fpga_latch_proc_write_mask( "fpga_prog", file, buffer, 
                                              count, data, &z3_fpga_latch1_value, Z3_APP31_Y2_FPGA_PROG );

}

static int
z3_fpga_fpga_done_proc_read(char *page, char **start, off_t off, int count, int *eof,
		    void *data)
{
        u16 value = 0;

        z3_fpga_latch1_read(&value);
        value = (value& Z3_APP31_Y4_FPGA_DONE) ? 1 : 0;

        if ( value != 0 ) {
          if ( cpu_is_ti816x() ) {
                  omap_mux_init_signal( "gpmc_a3", TI81XX_MUX_PULLDIS );
                  omap_mux_init_signal( "gpmc_a4", TI81XX_MUX_PULLDIS );
                  omap_mux_init_signal( "gpmc_a5", TI81XX_MUX_PULLDIS );
                  omap_mux_init_signal( "gpmc_a6", TI81XX_MUX_PULLDIS );
          }
        }

        return z3_fpga_proc_read( value,
                                  page, start, off, count, eof, data );
}

static const char *z3_pll_settings[] = {
     [Z3_FPGA_PLL_NONE] = "none",
     [Z3_FPGA_PLL_VIDIN_74M25]   = "vidin_74m25",
     [Z3_FPGA_PLL_VIDIN_74M176]  = "vidin_74m176",
     [Z3_FPGA_PLL_VIDIN_148M5]   = "vidin_148m5",
     [Z3_FPGA_PLL_VIDIN_148M352]  = "vidin_148m352",
     [Z3_FPGA_PLL_VIDIN_27M]     = "vidin_27m",
     [Z3_FPGA_PLL_VIDIN_13M5]     = "vidin_13m5",
     [Z3_FPGA_PLL_VIDOUT_74M176] = "vidout_74m176",
     [Z3_FPGA_PLL_VIDOUT_27M]    = "vidout_27m",
     [Z3_FPGA_PLL_OUTPUT_DISABLE] = "output_disable",
};



static const char *z3_fpga_pll_setting_to_string( enum z3_fpga_pll_setting setting )
{
     if ( setting < Z3_FPGA_PLL_SETTING_COUNT 
          && z3_pll_settings[setting] != NULL ) 
     {
          return z3_pll_settings[setting];
     } 
     else 
     {
          return "unknown";
     }
}

static enum z3_fpga_pll_setting z3_fpga_pll_string_to_setting( const char *str )
{
     enum z3_fpga_pll_setting setting = Z3_FPGA_PLL_NONE;
     int len;

     for ( setting = 0;
           setting < Z3_FPGA_PLL_SETTING_COUNT;
           setting++ ) {
          if ( NULL == z3_pll_settings[setting] )
               continue;

          len = strlen(z3_pll_settings[setting]);
          if ( 0 == strnicmp( str, z3_pll_settings[setting], len ) ) {
               break;
          }
     }
           
     return setting;
}

static int
z3_fpga_pll_setting_proc_read(char *page, char **start, off_t off, int count, int *eof,
		    void *data)
{
	char *p = page;
	int len;

        p += sprintf(p, "%s\n", z3_fpga_pll_setting_to_string(z3_fpga_pll_setting) );
	len = (p - page) - off;
	if (len < 0)
		len = 0;

	*eof = (len <= count) ? 1 : 0;
	*start = page + off;

	return len;
}

static int 
z3_fpga_pll_setting_proc_write(struct file *file, const char __user *buffer,
                            unsigned long count, void *data)
{
     enum z3_fpga_pll_setting setting;
     char scanbuf[32];
     int len = 0;

     len = count;
     if ( len > sizeof(scanbuf)-1 ) {
          len = sizeof(scanbuf)-1;
     }
     
     if ( copy_from_user( scanbuf, buffer, len ) ) {
          return -EFAULT;
     }
     
     scanbuf[len] = '\0';

     setting = z3_fpga_pll_string_to_setting(scanbuf);

     if ( setting < Z3_FPGA_PLL_SETTING_COUNT ) {
          if ( 0 == z3_fpga_pll_setup(setting) )
               return count;
          else
               return -EIO;
     } else {
          return -EINVAL;
     }
}

static int
z3_fpga_version_proc_read(char *page, char **start, off_t off, int count, int *eof,
                              void *data)
{
        return z3_fpga_proc_read( z3_fpga_read_version(),
                                     page, start, off, count, eof, data );
}



 static int 
z3_fpga_pll_load_pf_proc_write(struct file *file, const char __user *buffer,
                            unsigned long count, void *data)
{
     char scanbuf[32];
     int len = 0;
     int status;
     unsigned long new_value;

     len = count;
     if ( len > sizeof(scanbuf)-1 ) {
          len = sizeof(scanbuf)-1;
     }
     
     if ( copy_from_user( scanbuf, buffer, len ) ) {
          return -EFAULT;
     }
     
      scanbuf[len] = '\0';

     if (scanbuf[0] == '0' && scanbuf[1] == 'x' ) {
        status = strict_strtoul( scanbuf, 16, &new_value );
     }
     else {
          status = strict_strtoul( scanbuf, 10, &new_value );
     }
     if ( status ) {
          printk( KERN_ERR "/proc/fpga/pll_load_pf: Invalid value %s\n", scanbuf );
          return status;
     }

     z3_fpga_pll_load_pf = (new_value & 0xffff);

     if ( z3_fpga_pll_load_pf > 20 ) {
             z3_fpga_pll_load_pf = 20;
             printk( KERN_DEBUG "/proc/fpga/pll_load_pf: Value [%s] too large, set to max %u\n", scanbuf, z3_fpga_pll_load_pf );
     }

     if ( z3_fpga_pll_client != NULL ) {
             status = z3_fpga_i2c_pll_wr( 0x05, z3_fpga_pll_load_pf*8 );	// Crystal load capacitance

             if ( status < 0 ) 
                     return status;
     }

     return count;
}

static int
z3_fpga_pll_load_pf_proc_read(char *page, char **start, off_t off, int count, int *eof,
                              void *data)
{
	char *p = page;
	int len;

        p += sprintf(p, "%u\n", z3_fpga_pll_load_pf );
	len = (p - page) - off;
	if (len < 0)
		len = 0;

	*eof = (len <= count) ? 1 : 0;
	*start = page + off;

	return len;
}

static int
z3_fpga_pll_lock_proc_read(char *page, char **start, off_t off, int count, int *eof,
                              void *data)
{
	char *p = page;
	int len;
       
        int reg_value;
        int lock_value; 


        reg_value = (z3_fpga_read(Z3_FPGA_PLL_CTRL_OFFSET)) ;
        lock_value = (reg_value & Z3_FPGA_PLL_CTRL_LOCK_MASK) ? 1 : 0;

        p += sprintf(p, "%u\n", lock_value );
	len = (p - page) - off;
	if (len < 0)
		len = 0;

	*eof = (len <= count) ? 1 : 0;
	*start = page + off;

	return len;
}

static int z3_fpga_do_pll_load_scan(void)
{
        int load_pf;
        int load_pf_min = 0;
        int load_pf_max = 20;

        int ret = 0;

        int sleep_ms;
        int sleep_factor;

        int reg_value;

        switch (z3_fpga_pll_setting) 
        {
        case Z3_FPGA_PLL_VIDIN_148M5:
        case Z3_FPGA_PLL_VIDIN_148M352:
                /* Incoming clock is stable now, use normal wait */
                sleep_factor = 1;
                break;
        default:
                sleep_factor = 1;
                break;
        }

        z3_fpga_load_pf_min_lock = -1;
        z3_fpga_load_pf_max_lock = -1;
        z3_fpga_pll_load_scanned = 0;

        mutex_lock( &z3_fpga_pll_load_scan_mutex );
        
        for (load_pf = load_pf_min, sleep_ms = 200*sleep_factor; 
             load_pf <= load_pf_max; 
             load_pf+=2, sleep_ms=100*sleep_factor ) {

                ret |= z3_fpga_i2c_pll_wr( 0x05, load_pf*8 );
                if ( msleep_interruptible( sleep_ms ) ) {
                        ret =  -ERESTARTSYS;
                        goto unlock_out;
                }

                reg_value = (z3_fpga_read(Z3_FPGA_PLL_CTRL_OFFSET)) ;
                if  (reg_value & Z3_FPGA_PLL_CTRL_LOCK_MASK) {
                        if ( z3_fpga_load_pf_min_lock < 0 ) {
                                z3_fpga_load_pf_min_lock = load_pf;
                        } 

                        if ( z3_fpga_load_pf_max_lock < load_pf ) {
                                z3_fpga_load_pf_max_lock = load_pf;
                        }
                } else {
                        /* Range must be continuous */
                        if ( z3_fpga_load_pf_min_lock >= 0 ) {
                                break;
                        }
                }
        }


        /* Restore load value */
        ret |= z3_fpga_i2c_pll_wr( 0x05, z3_fpga_pll_load_pf*8 );
        (void) msleep_interruptible( 100 );

        z3_fpga_pll_load_scanned = 1;

unlock_out:
        mutex_unlock( &z3_fpga_pll_load_scan_mutex );
        return ret;
}

static int 
z3_fpga_pll_load_scan_proc_write(struct file *file, const char __user *buffer,
                            unsigned long count, void *data)
{
        int ret;

        ret = z3_fpga_do_pll_load_scan();
        if ( -ERESTARTSYS == ret )
                return ret;
        else if ( 0 == ret )
                return count;
        else 
                return -EIO;
}


static int
z3_fpga_pll_load_scan_proc_read(char *page, char **start, off_t off, int count, int *eof,
                                   void *data)
{
	char *p = page;
	int len;
       
        p += sprintf(p, "%d %d\n",
                     z3_fpga_load_pf_min_lock,
                     z3_fpga_load_pf_max_lock
                );
	len = (p - page) - off;
	if (len < 0)
		len = 0;

	*eof = (len <= count) ? 1 : 0;
	*start = page + off;

	return len;
}


static int 
z3_fpga_tso_pll_rate_proc_write(struct file *file, const char __user *buffer,
                            unsigned long count, void *data)
{
        char scanbuf[32];
        int len = 0;
        int status;
        struct clk *clk;
        unsigned long target_rate;
        const unsigned long target_rate_min = 64000;

        len = count;
        if ( len > sizeof(scanbuf)-1 ) {
                len = sizeof(scanbuf)-1;
        }
     
        if ( copy_from_user( scanbuf, buffer, len ) ) {
                return -EFAULT;
        }
     
        scanbuf[len] = '\0';

        clk = clk_get( NULL, z3_fpga_tso_pll_clk_name );
        if ( NULL == clk ) 
                return -EINVAL;

        status = strict_strtoul( scanbuf, 10, &target_rate );
        if ( status ) 
        {
                printk( KERN_ERR "%s: Invalid value %s\n", __FUNCTION__, scanbuf );
        }
        if ( target_rate < target_rate_min ) {
                printk( KERN_ERR "%s: Value %lu too small\n", __FUNCTION__, target_rate );
                count = -ERANGE;
        } else {
                clk_set_rate( clk, target_rate );
        }

        clk_put( clk );

        return count;
}

static int
z3_fpga_tso_pll_rate_proc_read(char *page, char **start, off_t off, int count, int *eof,
                                   void *data)
{
        struct clk *clk;
        unsigned long rate;
	char *p = page;
	int len;

        clk = clk_get( NULL, z3_fpga_tso_pll_clk_name );
        if ( NULL == clk ) 
                return -EINVAL;

        rate = clk_get_rate( clk );        

        clk_put( clk );

        p += sprintf(p, "%lu\n", rate );
	len = (p - page) - off;
	if (len < 0)
		len = 0;

	*eof = (len <= count) ? 1 : 0;
	*start = page + off;

	return len;
}

static int z3_fpga_pll_init( void )
{
     struct i2c_adapter *i2c_adap;

     if ( z3_fpga_board_id_value == Z3_BOARD_ID_APP_31 ) {
             if ( NULL == z3_fpga_pll_client ) {
                     i2c_adap = i2c_get_adapter(z3_fpga_pll_adapter_id);
                     if ( NULL != i2c_adap ) {
                             z3_fpga_pll_client = i2c_new_device(i2c_adap, &pll_board_info);
                     } else {
                             printk( KERN_DEBUG "%s: ERROR: PLL adapter %d\n", __FUNCTION__, z3_fpga_pll_adapter_id );
                     }

                     if ( NULL != z3_fpga_pll_client ) {
                             i2c_set_clientdata(z3_fpga_pll_client, NULL);
                             
                             z3_fpga_pll_setup(z3_fpga_pll_setting);
                     }
             } 
     }

     return 0;
}

static int
z3_fpga_board_id_proc_read(char *page, char **start, off_t off, int count, int *eof,
                              void *data)
{

     return z3_fpga_proc_read( z3_fpga_board_id_value,
                                  page, start, off, count, eof, data );
}

static int 
z3_fpga_board_id_proc_write(struct file *file, const char __user *buffer,
                            unsigned long count, void *data)
{
     char scanbuf[32];
     int len = 0;
     int status;
     unsigned long new_value;

     len = count;
     if ( len > sizeof(scanbuf)-1 ) {
          len = sizeof(scanbuf)-1;
     }
     
     if ( copy_from_user( scanbuf, buffer, len ) ) {
          return -EFAULT;
     }
     
      scanbuf[len] = '\0';

     if (scanbuf[0] == '0' && scanbuf[1] == 'x' ) {
        status = strict_strtoul( scanbuf, 16, &new_value );
     }
     else {
          status = strict_strtoul( scanbuf, 2, &new_value );
     }
     if ( status ) {
          printk( KERN_ERR "/proc/fpga/board_id: Invalid value %s\n", scanbuf );
          return status;
     }

     z3_fpga_board_id_value = (new_value & 0xffff);

     z3_fpga_pll_init();

     return count;
}


u16 z3_fpga_board_id(void)
{
        return z3_fpga_board_id_value;
}
EXPORT_SYMBOL(z3_fpga_board_id);


static const char *z3_fpga_audio_src_settings[] = {
     [Z3_FPGA_AUDIO_SRC_DISABLE] = "disable",
     [Z3_FPGA_AUDIO_SRC_ENABLE]  = "enable",
};

static const char *z3_fpga_audio_src_to_string( enum z3_fpga_audio_src setting )
{
     if ( setting < Z3_FPGA_AUDIO_SRC_SETTING_COUNT 
          && z3_fpga_audio_src_settings[setting] != NULL ) 
     {
          return z3_fpga_audio_src_settings[setting];
     } 
     else 
     {
          return "unknown";
     }
}

static enum z3_fpga_audio_src z3_fpga_audio_src_string_to_setting( const char *str )
{
     enum z3_fpga_audio_src setting;
     int len;

     for ( setting = 0;
           setting < Z3_FPGA_AUDIO_SRC_SETTING_COUNT;
           setting++ ) {
          if ( NULL == z3_fpga_audio_src_settings[setting] )
               continue;

          len = strlen(z3_fpga_audio_src_settings[setting]);
          if ( 0 == strnicmp( str, z3_fpga_audio_src_settings[setting], len ) ) {
               break;
          }
     }
           
     return setting;
}


static int
z3_fpga_audio_src_proc_read(char *page, char **start, off_t off, int count, int *eof,
                               void *data)
{
	char *p = page;
	int len;

        p += sprintf(p, "%s\n", z3_fpga_audio_src_to_string(z3_fpga_audio_src) );
	len = (p - page) - off;
	if (len < 0)
		len = 0;

	*eof = (len <= count) ? 1 : 0;
	*start = page + off;

	return len;
}

static int z3_fpga_audio_update(void)
{
        u16 audio_src_mask;

        if ( z3_fpga_get_aic_disable() ) {
                if ( z3_fpga_audio_src == Z3_FPGA_AUDIO_SRC_ENABLE ) 
                        audio_src_mask = 0x10;
                else
                        audio_src_mask = 0x00;
                        
                z3_fpga_write( 0x103|audio_src_mask, Z3_FPGA_AUDIO_HDMI_OFFSET );
        } else {
                if ( z3_fpga_aic_ext_master ) {
                        /* Must set SRC to get master clocks */
                        // SCLK = MCLK/8, drive MCLK+SCLK_LRCK, LRCLK = SCLK/32 (16 bits per sample)
                        z3_fpga_write( (z3_fpga_aic_ext_master_div_mask|0x012), Z3_FPGA_AUDIO_HDMI_OFFSET );
                } else {
                        z3_fpga_write( 0x0, Z3_FPGA_AUDIO_HDMI_OFFSET );
                }
        }

        return 0;
}

int
z3_fpga_audio_src_setup(enum z3_fpga_audio_src setting)
{
     z3_fpga_audio_src = setting;

     return z3_fpga_audio_update();
}

static int 
z3_fpga_audio_src_proc_write(struct file *file, const char __user *buffer,
                                unsigned long count, void *data)
{
     enum z3_fpga_audio_src setting;

     setting = z3_fpga_audio_src_string_to_setting(buffer);
     if ( setting < Z3_FPGA_AUDIO_SRC_SETTING_COUNT ) {
          if ( 0 == z3_fpga_audio_src_setup(setting) )
               return count;
          else
               return -EIO;
     } else {
          return -EINVAL;
     }
}



static const char *z3_fpga_audio_input_settings[] = {
     [Z3_FPGA_AUDIO_INPUT_1_2] =  "1+2",
     [Z3_FPGA_AUDIO_INPUT_3_4]  = "3+4",
     [Z3_FPGA_AUDIO_INPUT_5_6]  = "5+6",
     [Z3_FPGA_AUDIO_INPUT_7_8]  = "7+8",
};

static const char *z3_fpga_audio_input_to_string( enum z3_fpga_audio_input setting )
{
     if ( setting < Z3_FPGA_AUDIO_INPUT_SETTING_COUNT 
          && z3_fpga_audio_input_settings[setting] != NULL ) 
     {
          return z3_fpga_audio_input_settings[setting];
     } 
     else 
     {
          return "unknown";
     }
}

static enum z3_fpga_audio_input z3_fpga_audio_input_string_to_setting( const char *str )
{
     enum z3_fpga_audio_input setting;
     int len;

     for ( setting = 0;
           setting < Z3_FPGA_AUDIO_INPUT_SETTING_COUNT;
           setting++ ) {
          if ( NULL == z3_fpga_audio_input_settings[setting] )
               continue;

          len = strlen(z3_fpga_audio_input_settings[setting]);
          if ( 0 == strnicmp( str, z3_fpga_audio_input_settings[setting], len ) ) {
               break;
          }
     }
           
     return setting;
}


static int
z3_fpga_audio_input_proc_read(char *page, char **start, off_t off, int count, int *eof,
                               void *data)
{
	char *p = page;
	int len;

        p += sprintf(p, "%s\n", z3_fpga_audio_input_to_string(z3_fpga_audio_input) );
	len = (p - page) - off;
	if (len < 0)
		len = 0;

	*eof = (len <= count) ? 1 : 0;
	*start = page + off;

	return len;
}

int 
z3_fpga_audio_input_setup(enum z3_fpga_audio_input setting)
{
     z3_fpga_audio_input = setting;

     return z3_fpga_audio_update();
}

static int 
z3_fpga_audio_input_proc_write(struct file *file, const char __user *buffer,
                                unsigned long count, void *data)
{
     enum z3_fpga_audio_input setting;

     setting = z3_fpga_audio_input_string_to_setting(buffer);
     if ( setting < Z3_FPGA_AUDIO_INPUT_SETTING_COUNT ) {
          if ( 0 == z3_fpga_audio_input_setup(setting) )
               return count;
          else
               return -EIO;
     } else {
          return -EINVAL;
     }
}


void 
z3_fpga_set_aic_ext_master( int value )
{
        z3_fpga_aic_ext_master = value;
}
EXPORT_SYMBOL( z3_fpga_set_aic_ext_master );

int 
z3_fpga_get_aic_ext_master( void )
{
        return z3_fpga_aic_ext_master;
}
EXPORT_SYMBOL( z3_fpga_get_aic_ext_master );


int z3_fpga_set_aic_ext_master_sample_rate(unsigned int rate)
{
        switch (rate) {

        case 48000: // divide by 8
                z3_fpga_aic_ext_master_div_mask = 0x2000;
                break;
        case 32000: // divide by 12
                z3_fpga_aic_ext_master_div_mask = 0x3000;
                break;
        case 24000:
                z3_fpga_aic_ext_master_div_mask = 0x4000;
                break;
        case 16000:
                z3_fpga_aic_ext_master_div_mask = 0x5000;
                break;
        default:
                return -EINVAL;
        }

        return 0;
}

void
z3_fpga_set_aic_disable( int value )
{
        if ( !z3_fpga_valid_aic_disable_board_id() ) { 
                return;
        }

        if ( value ) {
                z3_fpga_latch1_value |= Z3_APP31_Y2_AIC_DIS;
        } else {
                z3_fpga_latch1_value &= ~Z3_APP31_Y2_AIC_DIS;
        }

        z3_fpga_latch_write( &z3_fpga_latch1_value );

        z3_fpga_audio_update();
}
EXPORT_SYMBOL( z3_fpga_set_aic_disable );


int
z3_fpga_get_aic_disable( void )
{
        if ( !z3_fpga_valid_aic_disable_board_id() ) { 
                return 0;
        }

        if ( 0 != (Z3_APP31_Y2_AIC_DIS & z3_fpga_latch1_value) ) {
                return 1;
        } else { 
                return 0;
        }
}
EXPORT_SYMBOL( z3_fpga_get_aic_disable );

#endif

static void z3_fpga_timer_function(unsigned long data)
{
     

     mod_timer( &z3_fpga_timer, jiffies + msecs_to_jiffies( z3_fpga_timeout_ms ) );
}


int z3_fpga_init(int board_id_gpio)
{
#ifdef CONFIG_PROC_FS
        struct proc_dir_entry *res_dir;
        struct proc_dir_entry *res_file;
#endif
        int ret = 0;
        int cnt;

        u16 z3_fpga_board_ids[16];

        z3_latch_base_addr = ioremap_nocache(Z3_LATCH_BASE_ADDR,
                                             Z3_LATCH_ADDR_SIZE);
        if (!z3_latch_base_addr) {
                printk(KERN_ERR "Couldn't io map Z3 LATCH registers\n");
                return -ENXIO;
        }


        if ( cpu_is_ti816x() && ti81xx_has_tppss() ) {
                z3_tppss_gbl_base_addr = ioremap_nocache( 0x4a080000,
                                                          4096 );
                if ( z3_tppss_gbl_base_addr ) {
                        z3_tppss_stc0_27mcnt = z3_tppss_gbl_base_addr + 0x818;
                }
        }

        mutex_init(&z3_fpga_pll_load_scan_mutex);


        /* Read board id */

        if (cpu_is_ti814x())
                z3_fpga_latch1_value = Z3_APP31_Y2_DM814X|Z3_APP31_Y2_FPGA_PROG;
        else 
                z3_fpga_latch1_value = Z3_APP31_Y2_FPGA_PROG;
                  

        memset( z3_fpga_board_ids, '\0', sizeof(z3_fpga_board_ids) );
          
        cnt = 0;
        while ( cnt < 8 ) { 
                z3_fpga_board_id_value = 0;
                cnt++;

                z3_fpga_latch1_value |= Z3_APP31_Y2_LED1_R;
                z3_fpga_latch_write(&z3_fpga_latch1_value);
                mdelay(50);

                if ( gpio_get_value( board_id_gpio ) ) {
                        z3_fpga_board_id_value |= 1;
                }

                z3_fpga_latch1_value &= ~Z3_APP31_Y2_LED1_R;
                z3_fpga_latch_write(&z3_fpga_latch1_value);
//                  mdelay(10);

                z3_fpga_board_id_value <<= 1;

                z3_fpga_latch1_value |= Z3_APP31_Y2_LED1_G;
                z3_fpga_latch_write(&z3_fpga_latch1_value);
                mdelay(50);

                if ( gpio_get_value( board_id_gpio ) ) {
                        z3_fpga_board_id_value |= 1;
                }

                z3_fpga_latch1_value &= ~Z3_APP31_Y2_LED1_G;
                z3_fpga_latch_write(&z3_fpga_latch1_value);
//                  mdelay(10);

                z3_fpga_board_id_value <<= 1;
                z3_fpga_latch1_value |= Z3_APP31_Y2_LED0_R;

                z3_fpga_latch_write(&z3_fpga_latch1_value);
                mdelay(50);

                if ( gpio_get_value( board_id_gpio ) ) {
                        z3_fpga_board_id_value |= 1;
                }
                z3_fpga_latch1_value &= ~Z3_APP31_Y2_LED0_R;
                z3_fpga_latch_write(&z3_fpga_latch1_value);
//                  mdelay(10);

                z3_fpga_board_id_value <<= 1;
                z3_fpga_latch1_value |= Z3_APP31_Y2_LED0_G;

                z3_fpga_latch_write(&z3_fpga_latch1_value);
                mdelay(50);

                if ( gpio_get_value( board_id_gpio ) ) {
                        z3_fpga_board_id_value |= 1;
                }
                z3_fpga_latch1_value &= ~Z3_APP31_Y2_LED0_G;
                z3_fpga_latch_write(&z3_fpga_latch1_value);
//                  mdelay(10);

                z3_fpga_board_id_value &= 0xf;
                  
                z3_fpga_board_ids[z3_fpga_board_id_value]++;

                if ( z3_fpga_board_ids[z3_fpga_board_id_value] >= 3 )
                        break;

//                  printk( KERN_EMERG "Read board id try %d value [%u]\n", cnt,
//                          z3_fpga_board_id_value
//                          );
        }
          
        for ( cnt=0; cnt<16; cnt++ ) {
                if ( z3_fpga_board_ids[cnt] > z3_fpga_board_ids[z3_fpga_board_id_value] ) {
                        z3_fpga_board_id_value = cnt;
                }
        }



#ifdef CONFIG_PROC_FS
        res_dir = proc_mkdir("fpga", NULL);
        if (!res_dir)
                return -ENOMEM;

        res_file = create_proc_entry("latch1", S_IWUSR | S_IRUGO, res_dir);

        if (!res_file)
                return -ENOMEM;

        res_file->read_proc  = z3_fpga_latch1_proc_read;
        res_file->write_proc = z3_fpga_latch1_proc_write;

        res_file = create_proc_entry( "board_id", S_IWUSR | S_IRUGO, res_dir );
        if (!res_file)
                return -ENOMEM;
        res_file->read_proc  = z3_fpga_board_id_proc_read;
        res_file->write_proc = z3_fpga_board_id_proc_write;
#endif

        if ( z3_fpga_board_id_value != Z3_BOARD_ID_APP_31 ) {
                return 0;
        }


        z3_fpga_base_addr = ioremap_nocache(Z3_FPGA_BASE_ADDR,
                                            Z3_FPGA_ADDR_SIZE);
        if (!z3_fpga_base_addr) {
                printk(KERN_ERR "Couldn't io map Z3 FPGA registers\n");
                return -ENXIO;
        }


        init_timer( &z3_fpga_timer );
          
        z3_fpga_timer.function = z3_fpga_timer_function;
        z3_fpga_timer.data     = 0;

        /* Wait until FPGA is loaded before reading STC */
        mod_timer( &z3_fpga_timer, jiffies + msecs_to_jiffies( 60000 ) );


#ifdef CONFIG_PROC_FS
        res_file = create_proc_entry("latch2", S_IWUSR | S_IRUGO, res_dir);
        if (!res_file)
                return -ENOMEM;
        res_file->read_proc  = z3_fpga_latch2_proc_read;
        res_file->write_proc = z3_fpga_latch2_proc_write;


        res_file = create_proc_entry( "stc", S_IRUGO, res_dir );
        if (!res_file)
                return -ENOMEM;
        res_file->read_proc  = z3_fpga_stc_proc_read;

        res_file = create_proc_entry("asi_ctl", S_IWUSR | S_IRUGO, res_dir);
        if (!res_file)
                return -ENOMEM;
        res_file->read_proc  = z3_fpga_asi_ctl_proc_read;
        res_file->write_proc = z3_fpga_asi_ctl_proc_write;

        res_file = create_proc_entry("sdi_out", S_IWUSR | S_IRUGO, res_dir);
        if (!res_file)
                return -ENOMEM;
        res_file->read_proc  = z3_fpga_sdi_out_proc_read;
        res_file->write_proc = z3_fpga_sdi_out_proc_write;

        res_file = create_proc_entry("pll_setting", S_IWUSR | S_IRUGO, res_dir);
        if (!res_file)
                return -ENOMEM;
        res_file->read_proc  = z3_fpga_pll_setting_proc_read;
        res_file->write_proc = z3_fpga_pll_setting_proc_write;

        res_file = create_proc_entry("pll_load_pf", S_IWUSR | S_IRUGO, res_dir);
        if (!res_file)
                return -ENOMEM;
        res_file->read_proc  = z3_fpga_pll_load_pf_proc_read;
        res_file->write_proc = z3_fpga_pll_load_pf_proc_write;


        res_file = create_proc_entry("pll_lock", S_IRUGO, res_dir);
        if (!res_file)
                return -ENOMEM;
        res_file->read_proc  = z3_fpga_pll_lock_proc_read;

        res_file = create_proc_entry("version", S_IRUGO, res_dir);
        if (!res_file)
                return -ENOMEM;
        res_file->read_proc  = z3_fpga_version_proc_read;

        res_file = create_proc_entry("pll_load_scan", S_IWUSR | S_IRUGO, res_dir);
        if (!res_file)
                return -ENOMEM;
        res_file->read_proc  = z3_fpga_pll_load_scan_proc_read;
        res_file->write_proc = z3_fpga_pll_load_scan_proc_write;

        if ( cpu_is_ti816x() ) {
                res_file = create_proc_entry("tso_pll_rate", S_IWUSR | S_IRUGO, res_dir);
                if (!res_file)
                        return -ENOMEM;
                res_file->read_proc  = z3_fpga_tso_pll_rate_proc_read;
                res_file->write_proc = z3_fpga_tso_pll_rate_proc_write;
        }


        res_file = create_proc_entry("tso_pcr_delta", S_IRUGO, res_dir);
        if (!res_file)
                return -ENOMEM;
        res_file->read_proc  = z3_fpga_tso_pcr_delta_proc_read;

        res_file = create_proc_entry("tso_pcr_pid", S_IWUSR | S_IRUGO, res_dir);
        if (!res_file)
                return -ENOMEM;
        res_file->read_proc  = z3_fpga_tso_pcr_pid_proc_read;
        res_file->write_proc = z3_fpga_tso_pcr_pid_proc_write;
        
        res_file = create_proc_entry("audio_src", S_IWUSR | S_IRUGO, res_dir);
        if (!res_file)
                return -ENOMEM;
        res_file->read_proc  = z3_fpga_audio_src_proc_read;
        res_file->write_proc = z3_fpga_audio_src_proc_write;

        res_file = create_proc_entry("audio_input", S_IWUSR | S_IRUGO, res_dir);
        if (!res_file)
                return -ENOMEM;
        res_file->read_proc  = z3_fpga_audio_input_proc_read;
        res_file->write_proc = z3_fpga_audio_input_proc_write;

        res_file = create_proc_entry("fpga_prog", S_IWUSR | S_IRUGO, res_dir);
        if (!res_file)
                return -ENOMEM;
        res_file->read_proc  = z3_fpga_fpga_prog_proc_read;
        res_file->write_proc = z3_fpga_fpga_prog_proc_write;

        res_file = create_proc_entry("fpga_done", S_IRUGO, res_dir);
        if (!res_file)
                return -ENOMEM;
        res_file->read_proc  = z3_fpga_fpga_done_proc_read;

        ret = gpio_request(Z3_APP_31_GPIO_FPGA_INITB, "fpga_initb");
        if ( ret == 0 ) {
                ret = gpio_direction_input(Z3_APP_31_GPIO_FPGA_INITB);
        }
        if ( ret == 0 ) {
                res_file = create_proc_entry("fpga_initb", S_IWUSR | S_IRUGO, res_dir);
                if (!res_file)
                        return -ENOMEM;
                res_file->read_proc  = z3_fpga_fpga_initb_proc_read;
                res_file->write_proc = z3_fpga_fpga_initb_proc_write;
        }

        ret = gpio_request(Z3_APP_31_GPIO_FPGA_CSB, "fpga_csb");
        if ( ret == 0 ) {
                ret = gpio_direction_output(Z3_APP_31_GPIO_FPGA_CSB, 1);
        }
        if ( ret == 0 ) {
                res_file = create_proc_entry("fpga_csb", S_IWUSR | S_IRUGO, res_dir);
                if (!res_file)
                        return -ENOMEM;
                res_file->read_proc  = z3_fpga_fpga_csb_proc_read;
                res_file->write_proc = z3_fpga_fpga_csb_proc_write;
        }


        if ( cpu_is_ti816x() ) {
                omap_mux_init_signal( "gp0_io26", TI81XX_MUX_PULLUP_ENABLE );
                omap_mux_init_signal( "gp0_io5", TI81XX_MUX_PULLUP_ENABLE );
        }
          
        /* GPMC_A[] lines */
        ret = gpio_request(Z3_APP_31_GPIO_FPGA_M1, "fpga_m1");
        if ( ret == 0 ) {
                ret = gpio_direction_output(Z3_APP_31_GPIO_FPGA_M1, 1);
                if ( ret == 0 ) {
                        /* Export to SYSFS */
                        ret = gpio_export(Z3_APP_31_GPIO_FPGA_M1, true);
                }

        }

        ret = gpio_request(Z3_APP_31_GPIO_FPGA_M0, "fpga_m0");
        if ( ret == 0 ) {
                ret = gpio_direction_output(Z3_APP_31_GPIO_FPGA_M0, 0);
                if ( ret == 0 ) {
                        /* Export to SYSFS */
                        ret = gpio_export(Z3_APP_31_GPIO_FPGA_M0, true);
                }
        }

        ret = gpio_request(Z3_APP_31_GPIO_FPGA_CSOUT_B, "fpga_csout");
        if ( ret == 0 ) {
                ret = gpio_direction_input(Z3_APP_31_GPIO_FPGA_CSOUT_B);
                if ( ret == 0 ) {
                        /* Export to SYSFS */
                        ret = gpio_export(Z3_APP_31_GPIO_FPGA_CSOUT_B, true);
                }
        }

        ret = gpio_request(Z3_APP_31_GPIO_FPGA_RDWR, "fpga_rdwr");
        if ( ret == 0 ) {
                ret = gpio_direction_output(Z3_APP_31_GPIO_FPGA_RDWR, 0);
                if ( ret == 0 ) {
                        /* Export to SYSFS */
                        ret = gpio_export(Z3_APP_31_GPIO_FPGA_RDWR, true);
                }
        }
#endif

        if ( cpu_is_ti816x() ) {
                omap_mux_init_signal( "gp0_io11", TI81XX_MUX_PULLDIS );
                omap_mux_init_signal( "gp0_io12", TI81XX_MUX_PULLDIS );
                omap_mux_init_signal( "gp0_io14", TI81XX_MUX_PULLDIS );
        }

        return 0;
}

void z3_fpga_cleanup(void)
{
     del_timer_sync( &z3_fpga_timer );
}


module_init(z3_fpga_pll_init);
