/*
This library is derived from ChaN's FatFs - Generic FAT Filesystem Module.
*/
#ifndef SPI_SDMMC_H
#define SPI_SDMMC_H
#include "hardware/spi.h"
#include "hardware/dma.h"

#define __SPI_SDMMC_DMA

/* SDMMC SPI pins*/
#define SDMMC_SPI_PORT spi0
#define SDMMC_PIN_MISO 16
#define SDMMC_PIN_CS   17
#define SDMMC_PIN_SCK  18
#define SDMMC_PIN_MOSI 19
/* ====================== */

/* MMC/SD command */
#define CMD0 (0)           /* GO_IDLE_STATE */
#define CMD1 (1)           /* SEND_OP_COND (MMC) */
#define ACMD41 (0x80 + 41) /* SEND_OP_COND (SDC) */
#define CMD8 (8)           /* SEND_IF_COND */
#define CMD9 (9)           /* SEND_CSD */
#define CMD10 (10)           /* SEND_CID */
#define CMD12 (12)           /* STOP_TRANSMISSION */
#define ACMD13 (0x80 + 13) /* SD_STATUS (SDC) */
#define CMD16 (16)           /* SET_BLOCKLEN */
#define CMD17 (17)           /* READ_SINGLE_BLOCK */
#define CMD18 (18)           /* READ_MULTIPLE_BLOCK */
#define CMD23 (23)           /* SET_BLOCK_COUNT (MMC) */
#define ACMD23 (0x80 + 23) /* SET_WR_BLK_ERASE_COUNT (SDC) */
#define CMD24 (24)           /* WRITE_BLOCK */
#define CMD25 (25)           /* WRITE_MULTIPLE_BLOCK */
#define CMD32 (32)           /* ERASE_ER_BLK_START */
#define CMD33 (33)           /* ERASE_ER_BLK_END */
#define CMD38 (38)           /* ERASE */
#define CMD55 (55)           /* APP_CMD */
#define CMD58 (58)           /* READ_OCR */

#define SDMMC_SECT_SIZE 512

typedef struct {
    spi_inst_t *spiPort;
    bool spiInit;
    uint csPin;
    uint8_t cardType;
    uint16_t sectSize;
    uint32_t sectCount;
#ifdef __SPI_SDMMC_DMA
    uint read_dma_ch;
    uint write_dma_ch;
    dma_channel_config dma_rc;
    dma_channel_config dma_wc;
    bool dmaInit;
#endif
    bool initialized;
} sdmmc_data_t;

bool sdmmc_disk_initialize(spi_inst_t *spi, uint cs_pin, sdmmc_data_t *sdmmc);
bool sdmmc_read_datablock(uint8_t *buff, uint btr, sdmmc_data_t *sdmmc);
bool sdmmc_write_datablock(const uint8_t *buff, uint8_t token, sdmmc_data_t *sdmmc);

// static
uint8_t sdmmc_send_cmd(uint8_t cmd,  uint32_t arg, sdmmc_data_t *sdmmc);
uint8_t sdmmc_write_sector(uint32_t sector, uint8_t *buff, uint32_t len, sdmmc_data_t *sdmmc);
uint8_t sdmmc_read_sector(uint32_t sector, uint8_t* buff, uint32_t len, sdmmc_data_t *sdmmc);

/* MMC card type flags (MMC_GET_TYPE) */
#define CT_NO_CARD 0x00 // No card
#define CT_MMC3    0x01 // MMC ver 3
#define CT_MMC4    0x02 // MMC ver 4+
#define CT_MMC     0x03 // MMC
#define CT_SDC1    0x02 // SDC ver 1
#define CT_SDC2    0x04 // SDC ver 2+
#define CT_SDC     0x0C // SDC
#define CT_BLOCK   0x10 // Block addressing

#endif
