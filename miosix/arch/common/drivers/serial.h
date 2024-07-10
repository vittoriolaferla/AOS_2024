
//Serial code is common for all the cortex M cores, so it has been put here

#ifdef _ARCH_ARM7_LPC2000
#include "serial_lpc2000.h"
#elif defined(_ARCH_CORTEXM0_STM32F0) || defined(_ARCH_CORTEXM3_STM32F1) \
   || defined(_ARCH_CORTEXM4_STM32F4) || defined(_ARCH_CORTEXM3_STM32F2) \
   || defined(_ARCH_CORTEXM3_STM32L1) || defined(_ARCH_CORTEXM7_STM32F7) \
   || defined(_ARCH_CORTEXM7_STM32H7) || defined(_ARCH_CORTEXM4_STM32F3) \
   || defined(_ARCH_CORTEXM4_STM32L4) || defined(_ARCH_CORTEXM0PLUS_STM32L0)
#include "serial_stm32.h"
#elif defined(_ARCH_CORTEXM3_EFM32GG) || defined(_ARCH_CORTEXM3_EFM32G)
#include "efm32_serial.h"
#elif defined(_ARCH_CORTEXM4_ATSAM4L)
#include "serial_atsam4l.h"
#elif defined(_ARCH_CORTEXM0PLUS_RP2040)
#include "rp2040_serial.h"
#else
#error "Unknown arch"
#endif
