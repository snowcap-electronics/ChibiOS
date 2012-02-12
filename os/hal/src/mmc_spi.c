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
/*
   Parts of this file have been contributed by Matthias Blaicher.
 */

/**
 * @file    mmc_spi.c
 * @brief   MMC over SPI driver code.
 *
 * @addtogroup MMC_SPI
 * @{
 */

#include <string.h>

#include "ch.h"
#include "hal.h"

#if HAL_USE_MMC_SPI || defined(__DOXYGEN__)

/*===========================================================================*/
/* Driver local definitions.                                                 */
/*===========================================================================*/

/*===========================================================================*/
/* Driver exported variables.                                                */
/*===========================================================================*/

/*===========================================================================*/
/* Driver local variables.                                                   */
/*===========================================================================*/

/*===========================================================================*/
/* Driver local functions.                                                   */
/*===========================================================================*/

/**
 * @brief   Inserion monitor timer callback function.
 *
 * @param[in] p         pointer to the @p MMCDriver object
 *
 * @notapi
 */
static void tmrfunc(void *p) {
  MMCDriver *mmcp = p;

  chSysLockFromIsr();
  if (mmcp->cnt > 0) {
    if (mmcp->is_inserted()) {
      if (--mmcp->cnt == 0) {
        mmcp->state = MMC_INSERTED;
        chEvtBroadcastI(&mmcp->inserted_event);
      }
    }
    else
      mmcp->cnt = MMC_POLLING_INTERVAL;
  }
  else {
    if (!mmcp->is_inserted()) {
      mmcp->state = MMC_WAIT;
      mmcp->cnt = MMC_POLLING_INTERVAL;
      chEvtBroadcastI(&mmcp->removed_event);
    }
  }
  chVTSetI(&mmcp->vt, MS2ST(MMC_POLLING_DELAY), tmrfunc, mmcp);
  chSysUnlockFromIsr();
}

/**
 * @brief   Waits an idle condition.
 *
 * @param[in] mmcp      pointer to the @p MMCDriver object
 *
 * @notapi
 */
static void wait(MMCDriver *mmcp) {
  int i;
  uint8_t buf[4];

  for (i = 0; i < 16; i++) {
    spiReceive(mmcp->spip, 1, buf);
    if (buf[0] == 0xFF)
      return;
  }
  /* Looks like it is a long wait.*/
  while (TRUE) {
    spiReceive(mmcp->spip, 1, buf);
    if (buf[0] == 0xFF)
      break;
#ifdef MMC_NICE_WAITING
    /* Trying to be nice with the other threads.*/
    chThdSleep(1);
#endif
  }
}

/**
 * @brief   Sends a command header.
 *
 * @param[in] mmcp      pointer to the @p MMCDriver object
 * @param[in] cmd       the command id
 * @param[in] arg       the command argument
 *
 * @notapi
 */
static void send_hdr(MMCDriver *mmcp, uint8_t cmd, uint32_t arg) {
  uint8_t buf[6];

  /* Wait for the bus to become idle if a write operation was in progress.*/
  wait(mmcp);

  buf[0] = 0x40 | cmd;
  buf[1] = arg >> 24;
  buf[2] = arg >> 16;
  buf[3] = arg >> 8;
  buf[4] = arg;
  buf[5] = 0x95;        /* Valid for CMD0 ignored by other commands. */
  spiSend(mmcp->spip, 6, buf);
}

/**
 * @brief   Receives a single byte response.
 *
 * @param[in] mmcp      pointer to the @p MMCDriver object
 * @return              The response as an @p uint8_t value.
 * @retval 0xFF         timed out.
 *
 * @notapi
 */
static uint8_t recvr1(MMCDriver *mmcp) {
  int i;
  uint8_t r1[1];

  for (i = 0; i < 9; i++) {
    spiReceive(mmcp->spip, 1, r1);
    if (r1[0] != 0xFF)
      return r1[0];
  }
  return 0xFF;
}

/**
 * @brief   Receives a three byte response.
 *
 * @param[in] mmcp      pointer to the @p MMCDriver object
 * @param[out] buffer   pointer to four bytes wide buffer
 * @return              First response byte as an @p uint8_t value.
 * @retval 0xFF         timed out.
 *
 * @notapi
 */
static uint8_t recvr3(MMCDriver *mmcp, uint8_t* buffer) {
  uint8_t r1;

  r1 = recvr1(mmcp);
  spiReceive(mmcp->spip, 4, buffer);

  return r1;
}

/**
 * @brief   Sends a command an returns a single byte response.
 *
 * @param[in] mmcp      pointer to the @p MMCDriver object
 * @param[in] cmd       the command id
 * @param[in] arg       the command argument
 * @return              The response as an @p uint8_t value.
 * @retval 0xFF         timed out.
 *
 * @notapi
 */
static uint8_t send_command_R1(MMCDriver *mmcp, uint8_t cmd, uint32_t arg) {
  uint8_t r1;

  spiSelect(mmcp->spip);
  send_hdr(mmcp, cmd, arg);
  r1 = recvr1(mmcp);
  spiUnselect(mmcp->spip);
  return r1;
}

/**
 * @brief   Sends a command which returns a five bytes response (R3).
 *
 * @param[in] mmcp      pointer to the @p MMCDriver object
 * @param[in] cmd       the command id
 * @param[in] arg       the command argument
 * @param[out] response pointer to four bytes wide uint8_t buffer
 * @return              The first byte of the response (R1) as an @p
 *                      uint8_t value.
 * @retval 0xFF         timed out.
 *
 * @notapi
 */
static uint8_t send_command_R3(MMCDriver *mmcp, uint8_t cmd, uint32_t arg,
                               uint8_t *response) {
  uint8_t r1;
  
  spiSelect(mmcp->spip);
  send_hdr(mmcp, cmd, arg);
  r1 = recvr3(mmcp, response);
  spiUnselect(mmcp->spip);
  return r1;
}

/**
 * @brief   Waits that the card reaches an idle state.
 *
 * @param[in] mmcp      pointer to the @p MMCDriver object
 *
 * @notapi
 */
static void sync(MMCDriver *mmcp) {
  uint8_t buf[1];

  spiSelect(mmcp->spip);
  while (TRUE) {
    spiReceive(mmcp->spip, 1, buf);
    if (buf[0] == 0xFF)
      break;
#ifdef MMC_NICE_WAITING
    chThdSleep(1);      /* Trying to be nice with the other threads.*/
#endif
  }
  spiUnselect(mmcp->spip);
}

/*===========================================================================*/
/* Driver exported functions.                                                */
/*===========================================================================*/

/**
 * @brief   MMC over SPI driver initialization.
 * @note    This function is implicitly invoked by @p halInit(), there is
 *          no need to explicitly initialize the driver.
 *
 * @init
 */
void mmcInit(void) {

}

/**
 * @brief   Initializes an instance.
 *
 * @param[out] mmcp         pointer to the @p MMCDriver object
 * @param[in] spip          pointer to the SPI driver to be used as interface
 * @param[in] lscfg         low speed configuration for the SPI driver
 * @param[in] hscfg         high speed configuration for the SPI driver
 * @param[in] is_protected  function that returns the card write protection
 *                          setting
 * @param[in] is_inserted   function that returns the card insertion sensor
 *                          status
 *
 * @init
 */
void mmcObjectInit(MMCDriver *mmcp, SPIDriver *spip,
                   const SPIConfig *lscfg, const SPIConfig *hscfg,
                   mmcquery_t is_protected, mmcquery_t is_inserted) {

  mmcp->state = MMC_STOP;
  mmcp->config = NULL;
  mmcp->spip = spip;
  mmcp->lscfg = lscfg;
  mmcp->hscfg = hscfg;
  mmcp->is_protected = is_protected;
  mmcp->is_inserted = is_inserted;
  mmcp->block_addresses = FALSE;
  chEvtInit(&mmcp->inserted_event);
  chEvtInit(&mmcp->removed_event);
}

/**
 * @brief   Configures and activates the MMC peripheral.
 *
 * @param[in] mmcp      pointer to the @p MMCDriver object
 * @param[in] config    pointer to the @p MMCConfig object. Must be @p NULL.
 *
 * @api
 */
void mmcStart(MMCDriver *mmcp, const MMCConfig *config) {

  chDbgCheck((mmcp != NULL) && (config == NULL), "mmcStart");

  chSysLock();
  chDbgAssert(mmcp->state == MMC_STOP, "mmcStart(), #1", "invalid state");
  mmcp->config = config;
  mmcp->state = MMC_WAIT;
  mmcp->cnt = MMC_POLLING_INTERVAL;
  chVTSetI(&mmcp->vt, MS2ST(MMC_POLLING_DELAY), tmrfunc, mmcp);
  chSysUnlock();
}

/**
 * @brief   Disables the MMC peripheral.
 *
 * @param[in] mmcp      pointer to the @p MMCDriver object
 *
 * @api
 */
void mmcStop(MMCDriver *mmcp) {

  chDbgCheck(mmcp != NULL, "mmcStop");

  chSysLock();
  chDbgAssert((mmcp->state != MMC_UNINIT) &&
              (mmcp->state != MMC_READING) &&
              (mmcp->state != MMC_WRITING),
              "mmcStop(), #1", "invalid state");
  if (mmcp->state != MMC_STOP) {
    mmcp->state = MMC_STOP;
    chVTResetI(&mmcp->vt);
  }
  chSysUnlock();
  spiStop(mmcp->spip);
}

/**
 * @brief   Performs the initialization procedure on the inserted card.
 * @details This function should be invoked when a card is inserted and
 *          brings the driver in the @p MMC_READY state where it is possible
 *          to perform read and write operations.
 * @note    It is possible to invoke this function from the insertion event
 *          handler.
 *
 * @param[in] mmcp      pointer to the @p MMCDriver object
 * @return              The operation status.
 * @retval FALSE        the operation succeeded and the driver is now
 *                      in the @p MMC_READY state.
 * @retval TRUE         the operation failed.
 *
 * @api
 */
bool_t mmcConnect(MMCDriver *mmcp) {
  unsigned i;
  bool_t result;

  chDbgCheck(mmcp != NULL, "mmcConnect");

  chDbgAssert((mmcp->state != MMC_UNINIT) && (mmcp->state != MMC_STOP),
              "mmcConnect(), #1", "invalid state");

  if (mmcp->state == MMC_INSERTED) {
    /* Slow clock mode and 128 clock pulses.*/
    spiStart(mmcp->spip, mmcp->lscfg);
    spiIgnore(mmcp->spip, 16);

    /* SPI mode selection.*/
    i = 0;
    while (TRUE) {
      if (send_command_R1(mmcp, MMC_CMDGOIDLE, 0) == 0x01)
        break;
      if (++i >= MMC_CMD0_RETRY)
        return TRUE;
      chThdSleepMilliseconds(10);
    }

    /* Try to detect if this is a high capacity card and switch to block
     * addresses if possible.
     *
     * This method is based on "How to support SDC Ver2 and high capacity cards"
     * by ElmChan.
     *
     * */
    uint8_t r3[4];
    if(send_command_R3(mmcp, MMC_CMDINTERFACE_CONDITION, 0x01AA, r3) != 0x05){

      /* Switch to SDHC mode */
      i = 0;
      while (TRUE) {
        if ((send_command_R1(mmcp, MMC_CMDAPP, 0) == 0x01) &&
            (send_command_R3(mmcp, MMC_ACMDOPCONDITION, 0x400001aa, r3) == 0x00))
          break;

        if (++i >= MMC_ACMD41_RETRY)
          return TRUE;
        chThdSleepMilliseconds(10);
      }

      /* Execute dedicated read on OCR register */
      send_command_R3(mmcp, MMC_CMDREADOCR, 0, r3);

      /* Check if CCS is set in response. Card operates in block mode if set */
      if(r3[0] & 0x40)
        mmcp->block_addresses = TRUE;
    }

    /* Initialization. */
    i = 0;
    while (TRUE) {
      uint8_t b = send_command_R1(mmcp, MMC_CMDINIT, 0);
      if (b == 0x00)
        break;
      if (b != 0x01)
        return TRUE;
      if (++i >= MMC_CMD1_RETRY)
        return TRUE;
      chThdSleepMilliseconds(10);
    }

    /* Initialization complete, full speed. */
    spiStart(mmcp->spip, mmcp->hscfg);

    /* Setting block size.*/
    if (send_command_R1(mmcp, MMC_CMDSETBLOCKLEN, MMC_SECTOR_SIZE) != 0x00)
      return TRUE;

    /* Transition to MMC_READY state (if not extracted).*/
    chSysLock();
    if (mmcp->state == MMC_INSERTED) {
      mmcp->state = MMC_READY;
      result = FALSE;
    }
    else
      result = TRUE;
    chSysUnlock();
    return result;
  }
  if (mmcp->state == MMC_READY)
    return FALSE;
  /* Any other state is invalid.*/
  return TRUE;
}

/**
 * @brief   Brings the driver in a state safe for card removal.
 *
 * @param[in] mmcp      pointer to the @p MMCDriver object
 * @return              The operation status.
 * @retval FALSE        the operation succeeded and the driver is now
 *                      in the @p MMC_INSERTED state.
 * @retval TRUE         the operation failed.
 *
 * @api
 */
bool_t mmcDisconnect(MMCDriver *mmcp) {
  bool_t status;

  chDbgCheck(mmcp != NULL, "mmcDisconnect");

  chDbgAssert((mmcp->state != MMC_UNINIT) && (mmcp->state != MMC_STOP),
              "mmcDisconnect(), #1", "invalid state");
  switch (mmcp->state) {
  case MMC_READY:
    /* Wait for the pending write operations to complete.*/
    sync(mmcp);
    chSysLock();
    if (mmcp->state == MMC_READY)
      mmcp->state = MMC_INSERTED;
    chSysUnlock();
  case MMC_INSERTED:
    status = FALSE;
  default:
    status = TRUE;
  }
  spiStop(mmcp->spip);
  return status;
}

/**
 * @brief   Starts a sequential read.
 *
 * @param[in] mmcp      pointer to the @p MMCDriver object
 * @param[in] startblk  first block to read
 * @return              The operation status.
 * @retval FALSE        the operation succeeded.
 * @retval TRUE         the operation failed.
 *
 * @api
 */
bool_t mmcStartSequentialRead(MMCDriver *mmcp, uint32_t startblk) {

  chDbgCheck(mmcp != NULL, "mmcStartSequentialRead");

  chSysLock();
  if (mmcp->state != MMC_READY) {
    chSysUnlock();
    return TRUE;
  }
  mmcp->state = MMC_READING;
  chSysUnlock();

  spiStart(mmcp->spip, mmcp->hscfg);
  spiSelect(mmcp->spip);

  if(mmcp->block_addresses)
    send_hdr(mmcp, MMC_CMDREADMULTIPLE, startblk);
  else
    send_hdr(mmcp, MMC_CMDREADMULTIPLE, startblk * MMC_SECTOR_SIZE);

  if (recvr1(mmcp) != 0x00) {
    spiUnselect(mmcp->spip);
    chSysLock();
    if (mmcp->state == MMC_READING)
      mmcp->state = MMC_READY;
    chSysUnlock();
    return TRUE;
  }
  return FALSE;
}

/**
 * @brief   Reads a block within a sequential read operation.
 *
 * @param[in] mmcp      pointer to the @p MMCDriver object
 * @param[out] buffer   pointer to the read buffer
 * @return              The operation status.
 * @retval FALSE        the operation succeeded.
 * @retval TRUE         the operation failed.
 *
 * @api
 */
bool_t mmcSequentialRead(MMCDriver *mmcp, uint8_t *buffer) {
  int i;

  chDbgCheck((mmcp != NULL) && (buffer != NULL), "mmcSequentialRead");

  chSysLock();
  if (mmcp->state != MMC_READING) {
    chSysUnlock();
    return TRUE;
  }
  chSysUnlock();

  for (i = 0; i < MMC_WAIT_DATA; i++) {
    spiReceive(mmcp->spip, 1, buffer);
    if (buffer[0] == 0xFE) {
      spiReceive(mmcp->spip, MMC_SECTOR_SIZE, buffer);
      /* CRC ignored. */
      spiIgnore(mmcp->spip, 2);
      return FALSE;
    }
  }
  /* Timeout.*/
  spiUnselect(mmcp->spip);
  chSysLock();
  if (mmcp->state == MMC_READING)
    mmcp->state = MMC_READY;
  chSysUnlock();
  return TRUE;
}

/**
 * @brief   Stops a sequential read gracefully.
 *
 * @param[in] mmcp      pointer to the @p MMCDriver object
 * @return              The operation status.
 * @retval FALSE        the operation succeeded.
 * @retval TRUE         the operation failed.
 *
 * @api
 */
bool_t mmcStopSequentialRead(MMCDriver *mmcp) {
  static const uint8_t stopcmd[] = {0x40 | MMC_CMDSTOP, 0, 0, 0, 0, 1, 0xFF};
  bool_t result;

  chDbgCheck(mmcp != NULL, "mmcStopSequentialRead");

  chSysLock();
  if (mmcp->state != MMC_READING) {
    chSysUnlock();
    return TRUE;
  }
  chSysUnlock();

  spiSend(mmcp->spip, sizeof(stopcmd), stopcmd);
/*  result = recvr1(mmcp) != 0x00;*/
  /* Note, ignored r1 response, it can be not zero, unknown issue.*/
  recvr1(mmcp);
  result = FALSE;
  spiUnselect(mmcp->spip);

  chSysLock();
  if (mmcp->state == MMC_READING)
    mmcp->state = MMC_READY;
  chSysUnlock();
  return result;
}

/**
 * @brief   Starts a sequential write.
 *
 * @param[in] mmcp      pointer to the @p MMCDriver object
 * @param[in] startblk  first block to write
 * @return              The operation status.
 * @retval FALSE        the operation succeeded.
 * @retval TRUE         the operation failed.
 *
 * @api
 */
bool_t mmcStartSequentialWrite(MMCDriver *mmcp, uint32_t startblk) {

  chDbgCheck(mmcp != NULL, "mmcStartSequentialWrite");

  chSysLock();
  if (mmcp->state != MMC_READY) {
    chSysUnlock();
    return TRUE;
  }
  mmcp->state = MMC_WRITING;
  chSysUnlock();

  spiStart(mmcp->spip, mmcp->hscfg);
  spiSelect(mmcp->spip);
  if(mmcp->block_addresses)
    send_hdr(mmcp, MMC_CMDWRITEMULTIPLE, startblk);
  else
    send_hdr(mmcp, MMC_CMDWRITEMULTIPLE, startblk * MMC_SECTOR_SIZE);


  if (recvr1(mmcp) != 0x00) {
    spiUnselect(mmcp->spip);
    chSysLock();
    if (mmcp->state == MMC_WRITING)
      mmcp->state = MMC_READY;
    chSysUnlock();
    return TRUE;
  }
  return FALSE;
}

/**
 * @brief   Writes a block within a sequential write operation.
 *
 * @param[in] mmcp      pointer to the @p MMCDriver object
 * @param[out] buffer   pointer to the write buffer
 * @return              The operation status.
 * @retval FALSE        the operation succeeded.
 * @retval TRUE         the operation failed.
 *
 * @api
 */
bool_t mmcSequentialWrite(MMCDriver *mmcp, const uint8_t *buffer) {
  static const uint8_t start[] = {0xFF, 0xFC};
  uint8_t b[1];

  chDbgCheck((mmcp != NULL) && (buffer != NULL), "mmcSequentialWrite");

  chSysLock();
  if (mmcp->state != MMC_WRITING) {
    chSysUnlock();
    return TRUE;
  }
  chSysUnlock();

  spiSend(mmcp->spip, sizeof(start), start);        /* Data prologue.       */
  spiSend(mmcp->spip, MMC_SECTOR_SIZE, buffer);     /* Data.                */
  spiIgnore(mmcp->spip, 2);                         /* CRC ignored.         */
  spiReceive(mmcp->spip, 1, b);
  if ((b[0] & 0x1F) == 0x05) {
    wait(mmcp);
    return FALSE;
  }

  /* Error.*/
  spiUnselect(mmcp->spip);
  chSysLock();
  if (mmcp->state == MMC_WRITING)
    mmcp->state = MMC_READY;
  chSysUnlock();
  return TRUE;
}

/**
 * @brief   Stops a sequential write gracefully.
 *
 * @param[in] mmcp      pointer to the @p MMCDriver object
 * @return              The operation status.
 * @retval FALSE        the operation succeeded.
 * @retval TRUE         the operation failed.
 *
 * @api
 */
bool_t mmcStopSequentialWrite(MMCDriver *mmcp) {
  static const uint8_t stop[] = {0xFD, 0xFF};

  chDbgCheck(mmcp != NULL, "mmcStopSequentialWrite");

  chSysLock();
  if (mmcp->state != MMC_WRITING) {
    chSysUnlock();
    return TRUE;
  }
  chSysUnlock();

  spiSend(mmcp->spip, sizeof(stop), stop);
  spiUnselect(mmcp->spip);

  chSysLock();
  if (mmcp->state == MMC_WRITING) {
    mmcp->state = MMC_READY;
    chSysUnlock();
    return FALSE;
  }
  chSysUnlock();
  return TRUE;
}

#endif /* HAL_USE_MMC_SPI */

/** @} */
