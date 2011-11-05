# List of the ChibiOS/RT Cortex-M3 STM32 port files.
PORTSRC = $(CHIBIOS)/os/ports/GCC/ARMCMx/crt0.c \
          $(CHIBIOS)/os/ports/GCC/ARMCMx/STM32F4xx/vectors.c \
          ${CHIBIOS}/os/ports/GCC/ARMCMx/chcore.c \
          ${CHIBIOS}/os/ports/GCC/ARMCMx/chcore_v7m.c \
          ${CHIBIOS}/os/ports/GCC/ARMCMx/nvic.c

PORTASM =

PORTINC = ${CHIBIOS}/os/ports/common/ARMCMx/CMSIS/include \
          ${CHIBIOS}/os/ports/GCC/ARMCMx \
          ${CHIBIOS}/os/ports/GCC/ARMCMx/STM32F4xx

PORTLD  = ${CHIBIOS}/os/ports/GCC/ARMCMx/STM32F4xx/ld
