#ifndef _DM81xx_Z3_APP_H
#define _DM81xx_Z3_APP_H

#include <linux/bitops.h>

/* APP-31 */

/* Latch Y2# - A3 low Write  */
#define Z3_APP31_Y2_RESETN            BIT(0)
#define Z3_APP31_Y2_ASI_IN_ASI        BIT(1)
#define Z3_APP31_Y2_ASI_OUT_ASI       BIT(2)

#define Z3_APP31_Y2_LED0_G            BIT(8)
#define Z3_APP31_Y2_LED0_R            BIT(9)
#define Z3_APP31_Y2_LED1_G            BIT(10)
#define Z3_APP31_Y2_LED1_R            BIT(11)

#define Z3_APP31_Y2_FPGA_PROG         BIT(13)
#define Z3_APP31_Y2_AIC_DIS           BIT(14)
#define Z3_APP31_Y2_DM814X            BIT(15)


#define Z3_APP02_LATCH_HI0RESETN      BIT(0)
#define Z3_APP02_LATCH_ADC0RESETN     BIT(1)
#define Z3_APP02_LATCH_ADC0POWERDN    BIT(2)
#define Z3_APP02_LATCH_CVRESETN       BIT(3)
#define Z3_APP02_LATCH_CVPOWERDN      BIT(4)
#define Z3_APP02_LATCH_HI1RESETN      BIT(5)
#define Z3_APP02_LATCH_ADC1RESETN     BIT(6)
#define Z3_APP02_LATCH_ADC1POWERDN    BIT(7)

#define Z3_APP02_LATCH_LED0_G         BIT(8)
#define Z3_APP02_LATCH_LED0_R         BIT(9)
#define Z3_APP02_LATCH_LED1_G         BIT(10)
#define Z3_APP02_LATCH_LED1_R         BIT(11)
#define Z3_APP02_LATCH_AIC32RESETN    BIT(13)
#define Z3_APP02_LATCH_MCA1_SEL       BIT(14)
#define Z3_APP02_LATCH_DM814X         BIT(15)


/* Latch Y3# - A3 high Write  */
#define Z3_APP31_Y3_SO_GRP2          BIT(15)
#define Z3_APP31_Y3_SO_GRP1          BIT(14)
#define Z3_APP31_Y3_SO_DETTRS        BIT(13)
#define Z3_APP31_Y3_SO_656BYP        BIT(12)
#define Z3_APP31_Y3_SO_ASI           BIT(11)
#define Z3_APP31_Y3_SO_20BIT         BIT(10)
#define Z3_APP31_Y3_SO_RATE1         BIT(9)
#define Z3_APP31_Y3_SO_RATE0         BIT(8)

#define Z3_APP31_Y3_SO_STDBY         BIT(7)
#define Z3_APP31_Y3_SI_STDBY         BIT(6)
#define Z3_APP31_Y3_ADC0_PDN         BIT(5)
#define Z3_APP31_Y3_OE_SI_ASI_656BYP BIT(4)
#define Z3_APP31_Y3_SI_SDHD          BIT(3)
#define Z3_APP31_Y3_SI_656BYP        BIT(2)
#define Z3_APP31_Y3_SI_ASI           BIT(1)
#define Z3_APP31_Y3_SI_20BIT         BIT(0)


/* Latch Y4# - A3 low read  */
#define Z3_APP31_Y4_FPGA_DONE        BIT(7)
#define Z3_APP31_Y4_ASI_OUT_LOCK     BIT(6)
#define Z3_APP31_Y4_ASI_IN_LOCK      BIT(5)
#define Z3_APP31_Y4_SO_LOCKED        BIT(4)
#define Z3_APP31_Y4_SI_LOCKED        BIT(3)
#define Z3_APP31_Y4_SI_656BYP        BIT(2)
#define Z3_APP31_Y4_SI_ASI           BIT(1)


#define Z3_APP22_LATCH_LED0_G         BIT(8)
#define Z3_APP22_LATCH_LED0_R         BIT(9)
#define Z3_APP22_LATCH_LED1_G         BIT(10)
#define Z3_APP22_LATCH_LED1_R         BIT(11)
#define Z3_APP22_LATCH_EDID_WPRT      BIT(12)
#define Z3_APP22_LATCH_RST7002N       BIT(13)
#define Z3_APP22_LATCH_RST7611N       BIT(14)



// INT_HI0  - GP0_IO4
// FPGA_INT - FPGA_INT
// pin F1: CLKOUT: TVP_ECLK


/* GP0_21: Board ID */

#define Z3_BOARD_ID_NONE        0xffff
#define Z3_BOARD_ID_MODULE_ONLY 0x0000
#define Z3_BOARD_ID_APP_02      0x0001
#define Z3_BOARD_ID_APP_31      0x0002
#define Z3_BOARD_ID_APP_21      0x0008

#define Z3_APP_31_GPIO_FPGA_INITB    26
#define Z3_APP_31_GPIO_FPGA_CSB       5
#define Z3_APP_31_GPIO_FPGA_M0       11
#define Z3_APP_31_GPIO_GPMC_A3       11
#define Z3_APP_31_GPIO_FPGA_M1       12
#define Z3_APP_31_GPIO_FPGA_CSOUT_B  13
#define Z3_APP_31_GPIO_FPGA_RDWR     14



#endif
