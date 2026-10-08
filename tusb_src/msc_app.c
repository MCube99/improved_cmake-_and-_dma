/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#include <stdio.h>
#include "tusb.h"
#include "ff.h"
#include "diskio.h"

//------------- Elm Chan FatFS -------------//
static CFG_TUH_MEM_SECTION FATFS fatfs[CFG_TUH_DEVICE_MAX]; // for simplicity only support 1 LUN per device
static volatile bool              _disk_busy[CFG_TUH_DEVICE_MAX];

static CFG_TUH_MEM_SECTION FIL     file1, file2;

#ifndef CFG_EXAMPLE_MSC_FILE_EXPLORER_RW_BUFSIZE
#define CFG_EXAMPLE_MSC_FILE_EXPLORER_RW_BUFSIZE 4096
#endif
static CFG_TUH_MEM_SECTION uint8_t rw_buf[CFG_EXAMPLE_MSC_FILE_EXPLORER_RW_BUFSIZE];

// define the buffer to be place in USB/DMA memory with correct alignment/cache line size
CFG_TUH_MEM_SECTION static struct {
  TUH_EPBUF_TYPE_DEF(scsi_inquiry_resp_t, inquiry);
} scsi_resp;


//--------------------------------------------------------------------+
// MACRO TYPEDEF CONSTANT ENUM DECLARATION
//--------------------------------------------------------------------+
static scsi_inquiry_resp_t inquiry_resp;

static bool inquiry_complete_cb(uint8_t dev_addr, tuh_msc_complete_data_t const * cb_data) {
  msc_cbw_t const* cbw = cb_data->cbw;
  msc_csw_t const* csw = cb_data->csw;

  if (csw->status != 0) {
    printf("Inquiry failed\r\n");
    return false;
  }

  // Print out Vendor ID, Product ID and Rev
  printf("%.8s %.16s %.4s\r\n", inquiry_resp.vendor_id, inquiry_resp.product_id, inquiry_resp.product_rev);

  // Get capacity of device
  uint32_t const block_count = tuh_msc_get_block_count(dev_addr, cbw->lun);
  uint32_t const block_size = tuh_msc_get_block_size(dev_addr, cbw->lun);

  printf("Disk Size: %" PRIu32 " %" PRIu32 "-byte blocks: %" PRIu32 " MB\r\n",
    block_count, block_size, block_count / ((1024 * 1024) / block_size));

  return true;
}

//------------- IMPLEMENTATION -------------//

void msc_app_init(void) {
  for (size_t i = 0; i < CFG_TUH_DEVICE_MAX; i++) {
    _disk_busy[i] = false;
  }
}


void tuh_msc_mount_cb(uint8_t dev_addr) {
  printf("A MassStorage device is mounted\r\n");

  uint8_t const lun = 0;
  tuh_msc_inquiry(dev_addr, lun, &inquiry_resp, inquiry_complete_cb, 0);
}

void tuh_msc_umount_cb(uint8_t dev_addr) {
  (void) dev_addr;
  printf("A MassStorage device is unmounted\r\n");
}


//--------------------------------------------------------------------+
// DiskIO
//--------------------------------------------------------------------+

static void wait_for_disk_io(BYTE pdrv) {
  while (_disk_busy[pdrv]) {
    tuh_task();
  }
}

static bool disk_io_complete(uint8_t dev_addr, const tuh_msc_complete_data_t *cb_data) {
  (void)dev_addr;
  (void)cb_data;
  _disk_busy[dev_addr - 1] = false;
  return true;
}

DSTATUS disk_status(BYTE pdrv /* Physical drive nmuber to identify the drive */
) {
  uint8_t dev_addr = pdrv + 1;
  return tuh_msc_mounted(dev_addr) ? 0 : STA_NODISK;
}

DSTATUS disk_initialize(BYTE pdrv /* Physical drive nmuber to identify the drive */
) {
  (void)pdrv;
  return 0;                       // nothing to do
}

DRESULT disk_read(BYTE  pdrv,     /* Physical drive nmuber to identify the drive */
                  BYTE *buff,     /* Data buffer to store read data */
                  LBA_t sector,   /* Start sector in LBA */
                  UINT  count     /* Number of sectors to read */
) {
  const uint8_t dev_addr = pdrv + 1;
  const uint8_t lun      = 0;

  _disk_busy[pdrv] = true;
  tuh_msc_read10(dev_addr, lun, buff, sector, (uint16_t)count, disk_io_complete, 0);
  wait_for_disk_io(pdrv);

  return RES_OK;
}

#if FF_FS_READONLY == 0

DRESULT disk_write(BYTE        pdrv,   /* Physical drive nmuber to identify the drive */
                   const BYTE *buff,   /* Data to be written */
                   LBA_t       sector, /* Start sector in LBA */
                   UINT        count   /* Number of sectors to write */
) {
  const uint8_t dev_addr = pdrv + 1;
  const uint8_t lun      = 0;

  _disk_busy[pdrv] = true;
  tuh_msc_write10(dev_addr, lun, buff, sector, (uint16_t)count, disk_io_complete, 0);
  wait_for_disk_io(pdrv);

  return RES_OK;
}

#endif

DRESULT disk_ioctl(BYTE  pdrv, /* Physical drive nmuber (0..) */
                   BYTE  cmd,  /* Control code */
                   void *buff  /* Buffer to send/receive control data */
) {
  const uint8_t dev_addr = pdrv + 1;
  const uint8_t lun      = 0;
  switch (cmd) {
    case CTRL_SYNC:
      // nothing to do since we do blocking
      return RES_OK;

    case GET_SECTOR_COUNT:
      *((DWORD *)buff) = (DWORD)tuh_msc_get_block_count(dev_addr, lun);
      return RES_OK;

    case GET_SECTOR_SIZE:
      *((WORD *)buff) = (WORD)tuh_msc_get_block_size(dev_addr, lun);
      return RES_OK;

    case GET_BLOCK_SIZE:
      *((DWORD *)buff) = 1; // erase block size in units of sector size
      return RES_OK;

    default:
      return RES_PARERR;
  }
}