/*
    ChibiOS/RT - Copyright (C) 2006,2007,2008,2009,2010,
                 2011,2012 Giovanni Di Sirio.

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
 * @file    i2s.h
 * @brief   I2S Driver macros and structures.
 *
 * @addtogroup I2S
 * @{
 */

#ifndef _I2S_H_
#define _I2S_H_

#if HAL_USE_I2S || defined(__DOXYGEN__)

/*===========================================================================*/
/* Driver constants.                                                         */
/*===========================================================================*/

/*===========================================================================*/
/* Driver pre-compile time settings.                                         */
/*===========================================================================*/

/*===========================================================================*/
/* Derived constants and error checks.                                       */
/*===========================================================================*/

/*===========================================================================*/
/* Driver data structures and types.                                         */
/*===========================================================================*/

/**
 * @brief   Driver state machine possible states.
 */
typedef enum {
  I2S_UNINIT = 0,                   /**< Not initialized.                   */
  I2S_STOP = 1,                     /**< Stopped.                           */
  I2S_READY = 2,                    /**< Ready.                             */
  I2S_ACTIVE = 3,                   /**< Active.                            */
  I2S_COMPLETE = 4                  /**< Transmission complete.             */
} i2sstate_t;

/**
 * @brief   Type of a structure representing a I2S driver.
 */
typedef struct I2SDriver I2SDriver;

#include "i2s_lld.h"

/*===========================================================================*/
/* Driver macros.                                                            */
/*===========================================================================*/

/**
 * @name    Macro Functions
 * @{
 */
/**
 * @brief   Starts a I2S transmission.
 * @details The transmission is started and it is executes until the specified
 *          buffer has entirely transmitted.
 * @post    A callback is invoked when the buffer has been fully transmitted.
 *
 * @param[in] i2sp      pointer to the @p I2SDriver object
 * @param[in] n         number of samples to send
 * @param[in] txbuf     the pointer to the transmit buffer
 *
 * @iclass
 */
#define i2sStartSendOnceI(i2sp, n, txbuf)                                   \
  i2s_lld_start_send_once(i2sp, n, txbuf)

/**
 * @brief   Starts a continuous I2S transmission.
 * @details The transmission is started and it is executes continuously
 *          until @p i2sStopSend() has been explicitly invoked .
 * @post    A callback is invoked when the buffer has been half transmitted
 *          and fully transmitted.
 *
 * @param[in] i2sp      pointer to the @p I2SDriver object
 * @param[in] n         number of samples to send
 * @param[in] txbuf     the pointer to the transmit buffer
 *
 * @api
 */
#define i2sStartSendContinuousI(i2sp, n, txbuf)                             \
  i2s_lld_start_send_continuous(i2sp, n, txbuf)

/**
 * @brief   Stops the ongoing transmission.
 * @details The ongoing transmission, if any, is stopped, if the driver
 *          was not transmitting the function does nothing.
 *
 * @param[in] i2sp      pointer to the @p I2SDriver object
 *
 * @iclass
 */
#define i2sStopSendI(i2sp)                                                  \
  i2s_lld_stop_send(i2sp)
/** @} */

/*===========================================================================*/
/* External declarations.                                                    */
/*===========================================================================*/

#ifdef __cplusplus
extern "C" {
#endif
  void i2sInit(void);
  void i2sObjectInit(I2SDriver *i2sp);
  void i2sStart(I2SDriver *i2sp, const I2SConfig *config);
  void i2sStop(I2SDriver *i2sp);
  void i2sStartSendOnce(I2SDriver *i2sp,
                        size_t n,
                        const i2ssample_t *txbuf);
  void i2sStartSendContinuous(I2SDriver *i2sp,
                              size_t n,
                              const i2ssample_t *txbuf);
  void i2sStopSend(I2SDriver *i2sp);
#ifdef __cplusplus
}
#endif

#endif /* HAL_USE_I2S */

#endif /* _I2S_H_ */

/** @} */
