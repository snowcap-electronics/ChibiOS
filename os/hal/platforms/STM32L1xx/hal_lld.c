/*
    ChibiOS/RT - Copyright (C) 2006,2007,2008,2009,2010,
                 2011 Giovanni Di Sirio.

    This file is part of ChibiOS/RT.

    ChibiOS/RT is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    ChibiOS/RT is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/**
 * @file    STM32L1xx/hal_lld.c
 * @brief   STM32L1xx HAL subsystem low level driver source.
 *
 * @addtogroup HAL
 * @{
 */

#include "ch.h"
#include "hal.h"

#define AIRCR_VECTKEY           0x05FA0000

/*===========================================================================*/
/* Driver exported variables.                                                */
/*===========================================================================*/

/*===========================================================================*/
/* Driver local variables.                                                   */
/*===========================================================================*/

/*===========================================================================*/
/* Driver local functions.                                                   */
/*===========================================================================*/

/*===========================================================================*/
/* Driver interrupt handlers.                                                */
/*===========================================================================*/

/*===========================================================================*/
/* Driver exported functions.                                                */
/*===========================================================================*/

/**
 * @brief   Low level HAL driver initialization.
 *
 * @notapi
 */
void hal_lld_init(void) {

  /* Reset of all peripherals.*/
  rccResetAHB(!RCC_AHBRSTR_FLITFRST);
  rccResetAPB1(!0);
  rccResetAPB2(!RCC_APB2RSTR_SYSCFGRST);

  /* SysTick initialization using the system clock.*/
  SysTick->LOAD = STM32_HCLK / CH_FREQUENCY - 1;
  SysTick->VAL = 0;
  SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk |
                  SysTick_CTRL_ENABLE_Msk |
                  SysTick_CTRL_TICKINT_Msk;

#if defined(STM32_DMA_REQUIRED)
  dmaInit();
#endif
}

/**
 * @brief   STM32L1xx voltage, clocks and PLL initialization.
 * @note    All the involved constants come from the file @p board.h.
 * @note    This function should be invoked just after the system reset.
 *
 * @special
 */
#if defined(STM32L1XX_MD) || defined(__DOXYGEN__)
/**
 * @brief   Clocks and internal voltage initialization.
 */
void stm32_clock_init(void) {

#if !STM32_NO_INIT
  /* PWR clock enable.*/
  RCC->APB1ENR = RCC_APB1ENR_PWREN;

  /* Core voltage setup.*/
  while ((PWR->CSR & PWR_CSR_VOSF) != 0)
    ;                           /* Waits until regulator is stable.         */
  PWR->CR = STM32_VOS;
  while ((PWR->CSR & PWR_CSR_VOSF) != 0)
    ;                           /* Waits until regulator is stable.         */

  /* Initial clocks setup and wait for MSI stabilization, the MSI clock is
     always enabled because it is the fallback clock when PLL the fails.
     Trim fields are not altered from reset values.*/
  RCC->CFGR  = 0;
  RCC->ICSCR = (RCC->ICSCR & ~STM32_MSIRANGE_MASK) | STM32_MSIRANGE;
  RCC->CSR   = RCC_CSR_RMVF;
  RCC->CR    = RCC_CR_MSION;
  while ((RCC->CR & RCC_CR_MSIRDY) == 0)
    ;                           /* Waits until MSI is stable.               */

#if STM32_HSI_ENABLED
  /* HSI activation.*/
  RCC->CR |= RCC_CR_HSION;
  while ((RCC->CR & RCC_CR_HSIRDY) == 0)
    ;                           /* Waits until HSI is stable.               */
#endif

#if STM32_HSE_ENABLED
  /* HSE activation.*/
  RCC->CR |= RCC_CR_HSEON;
  while ((RCC->CR & RCC_CR_HSERDY) == 0)
    ;                           /* Waits until HSE is stable.               */
#endif

#if STM32_LSI_ENABLED
  /* LSI activation.*/
  RCC->CSR |= RCC_CSR_LSION;
  while ((RCC->CSR & RCC_CSR_LSIRDY) == 0)
    ;                           /* Waits until LSI is stable.               */
#endif

#if STM32_LSE_ENABLED
  /* LSE activation, have to unlock the register.*/
  if ((RCC->CSR & RCC_CSR_LSEON) == 0) {
    PWR->CR |= PWR_CR_DBP;
    RCC->CSR |= RCC_CSR_LSEON;
    PWR->CR &= ~PWR_CR_DBP;
  }
  while ((RCC->CSR & RCC_CSR_LSERDY) == 0)
    ;                           /* Waits until LSE is stable.               */
#endif

#if STM32_ACTIVATE_PLL
  /* PLL activation.*/
  RCC->CFGR |= STM32_PLLDIV | STM32_PLLMUL | STM32_PLLSRC;
  RCC->CR   |= RCC_CR_PLLON;
  while (!(RCC->CR & RCC_CR_PLLRDY))
    ;                           /* Waits until PLL is stable.               */
#endif

  /* Other clock-related settings (dividers, MCO etc).*/
  RCC->CR   |= STM32_RTCPRE;
  RCC->CFGR |= STM32_MCOPRE | STM32_MCOSEL |
               STM32_PPRE2 | STM32_PPRE1 | STM32_HPRE;
  RCC->CSR  |= STM32_RTCSEL;

  /* Flash setup and final clock selection.   */
#if defined(STM32_FLASHBITS1)
  FLASH->ACR = STM32_FLASHBITS1;
#endif
#if defined(STM32_FLASHBITS2)
  FLASH->ACR = STM32_FLASHBITS2;
#endif

  /* Switching to the configured clock source if it is different from MSI.*/
#if (STM32_SW != STM32_SW_MSI)
  RCC->CFGR |= STM32_SW; /* Switches on the selected clock source.   */
  while ((RCC->CFGR & RCC_CFGR_SWS) != (STM32_SW << 2))
    ;
#endif
#endif /* STM32_NO_INIT */
}
#else
void stm32_clock_init(void) {}
#endif

/** @} */
