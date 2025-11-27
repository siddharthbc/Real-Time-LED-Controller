#ifndef MMA8451_H
#define MMA8451_H
#include <stdint.h>

// Select one of these orientations
#define UP_AXIS_Z 1 // FRDM board laying flat and face-up
// Roll depends on Y vs. Z. Pitch depends on X vs. Y and Z.
/// #define UP_AXIS_X 1 // FDRM board standing on end, USB connectors are up
// Roll depends on Y vs. X. Pitch depends on Z vs. X and Y.

#define MMA_DELAY_TBUF (5) // Delay for I2C to get at least t_BUF (1.3 us) between Stop and Start.

#define MMA_ADDR 0x3A

#define REG_STATUS 0x00
#define REG_XHI 0x01
#define REG_XLO 0x02
#define REG_YHI 0x03
#define REG_YLO 0x04
#define REG_ZHI	0x05
#define REG_ZLO 0x06

#define REG_INT_SOURCE 0x0C
#define REG_WHOAMI 0x0D
#define REG_CTRL1  0x2A
#define MMA_CTRL1_DR(x) ((x&0x07)<<3)
#define REG_CTRL2  0x2B
#define REG_CTRL3  0x2C
#define REG_CTRL4  0x2D
#define REG_CTRL5  0x2E

#define WHOAMI 0x1A

#define MMA_USE_INT1 0
#define MMA_USE_INT2 0
#define MMA_USE_INTERRUPTS (MMA_USE_INT1 | MMA_USE_INT2)

#define MMA_INT_IRQn (PORTA_IRQn)
#define MMA_INT_PORT (PORTA)
#define MMA_INT_GPIO (GPIOA)
#define MMA_INT1_POS (14)
#define MMA_INT2_POS (15)

#define COUNTS_PER_G (16384.0)
#define M_PI (3.14159265)

#define MU 	(0.01)
#define SIGN(x) (x>=0? 1:-1)

int init_mma(void);
uint8_t read_status(void);
void enable_mma_interrupt_generation(uint8_t);
void mma_set_active(uint8_t);
void read_full_xyz(void);
void read_xyz(void);
void convert_xyz_to_roll_pitch(void);
float Acc_Get_Magnitude(void);

extern float roll, pitch;
extern int16_t acc_X, acc_Y, acc_Z;

#endif
