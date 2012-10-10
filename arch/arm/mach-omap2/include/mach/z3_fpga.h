#ifndef _Z3_FPGA_H
#define _Z3_FPGA_H

#include <linux/time.h>

#define Z3_LATCH_BASE_ADDR	(0x01000000)
#define Z3_LATCH_ADDR_SIZE	(0x00010000)
#define Z3_FPGA_BASE_ADDR	(0x02000000)
#define Z3_FPGA_ADDR_SIZE	(0x01000000)

#define Z3_FPGA_LATCH_SO_MASK           (0xFF80)

#define Z3_FPGA_VERSION_READ_OFFSET       (0x00)

#define Z3_FPGA_SDI_OUT_OFFSET            (0x02)

#define Z3_FPGA_AUDIO_HDMI_OFFSET         (0x04) 

#define Z3_FPGA_PLL_CTRL_OFFSET           (0x06)

#define Z3_FPGA_PLL_CTRL_LOCK_MASK        (0x8000)

#define Z3_FPGA_STC_EXT_OFFSET            (0x08)
#define Z3_FPGA_STC_BASE_L_OFFSET         (0x0a)
#define Z3_FPGA_STC_BASE_M_OFFSET         (0x0c)
#define Z3_FPGA_STC_BASE_H_OFFSET         (0x0e)

#define Z3_FPGA_STC_BASE_H_MASK           (0x0001)

#define Z3_FPGA_ASI_CTL_OFFSET            (0x10)

#define Z3_FPGA_TSO_STC_PCR_DELTA_LOW     (0x30)
#define Z3_FPGA_TSO_STC_PCR_DELTA_MID     (0x32)
#define Z3_FPGA_TSO_STC_PCR_DELTA_HIGH    (0x34)
#define Z3_FPGA_TSO_PCR_PID_OFFSET        (0x36)

enum z3_fpga_audio_mode {
     Z3_FPGA_AUDIO_TRISTATE = 0,

     Z3_FPGA_AUDIO_HDMI,
     Z3_FPGA_AUDIO_SDI,
     Z3_FPGA_AUDIO_GENERATE_48KHZ_32FS,
     Z3_FPGA_AUDIO_GENERATE_32KHZ_32FS,
     Z3_FPGA_AUDIO_GENERATE_48KHZ_64FS,
     Z3_FPGA_AUDIO_GENERATE_32KHZ_64FS,
};

enum z3_fpga_audio_control {
     Z3_FPGA_AUDIO_CONTROL_AIC_MASTER,
     Z3_FPGA_AUDIO_CONTROL_FPGA_IN_MASTER,
     Z3_FPGA_AUDIO_CONTROL_FPGA_GEN_MASTER,
};


enum z3_fpga_pll_setting {
     Z3_FPGA_PLL_NONE,
     Z3_FPGA_PLL_VIDIN_74M25,
     Z3_FPGA_PLL_VIDIN_74M176,
     Z3_FPGA_PLL_VIDIN_148M5,
     Z3_FPGA_PLL_VIDIN_148M352,
     Z3_FPGA_PLL_VIDIN_27M,
     Z3_FPGA_PLL_VIDIN_13M5,
     Z3_FPGA_PLL_VIDOUT_74M176,
     Z3_FPGA_PLL_VIDOUT_27M,
     Z3_FPGA_PLL_OUTPUT_DISABLE,
     Z3_FPGA_PLL_SETTING_COUNT
};

enum z3_fpga_audio_src {
     Z3_FPGA_AUDIO_SRC_DISABLE,
     Z3_FPGA_AUDIO_SRC_ENABLE,
     Z3_FPGA_AUDIO_SRC_SETTING_COUNT,
};

enum z3_fpga_audio_input {
     Z3_FPGA_AUDIO_INPUT_1_2 = 0,
     Z3_FPGA_AUDIO_INPUT_3_4 = 1,
     Z3_FPGA_AUDIO_INPUT_5_6 = 2,
     Z3_FPGA_AUDIO_INPUT_7_8 = 3,
     Z3_FPGA_AUDIO_INPUT_SETTING_COUNT,
};

enum z3_fpga_vcap {
     Z3_FPGA_VCAP_TRISTATE,
     Z3_FPGA_VCAP_VIDIN,
     Z3_FPGA_VCAP_ASIIN,
};

enum z3_fpga_pll_vidin_reference {
        Z3_FPGA_PLL_VIDIN0_REFERENCE,
        Z3_FPGA_PLL_VIDIN1_REFERENCE,
};

int z3_fpga_read(u32 offset);
int z3_fpga_write(u16 val, u32 offset);

int z3_fpga_si_20bit( int enable );
int z3_fpga_so_set( unsigned int value );

int z3_fpga_audio_control(enum z3_fpga_audio_control);
int z3_fpga_audio_select(enum z3_fpga_audio_mode);

int z3_fpga_hdmi_hsync_counter(void);
void z3_fpga_stc_read(u64 *base, u32 *ext, struct timespec *ptimespec, u64 *stc0_27mhz, u32 *gptimer_27mhz);

void z3_fpga_set_aic_disable( int value );

int z3_fpga_get_aic_disable( void );

void z3_fpga_set_aic_ext_master( int value );

int z3_fpga_get_aic_ext_master( void );

int z3_fpga_set_aic_ext_master_sample_rate(unsigned int rate);

void z3_fpga_set_pll_vidin_reference( enum z3_fpga_pll_vidin_reference ref);

u16 z3_fpga_board_id(void);

int z3_fpga_init(int board_id_gpio);


#endif
