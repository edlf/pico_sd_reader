/*
This library is derived from ChaN's FatFs - Generic FAT Filesystem Module.
*/
#include "stdio.h"
#include "stdlib.h"
#include "pico/stdlib.h"
#include "spi_sdmmc.h"
#include "storage_driver.h"

static uint8_t dummy_block[SDMMC_SECT_SIZE];

void sdmmc_spi_cs_high(sdmmc_data_t *sdmmc);
void sdmmc_spi_cs_low(sdmmc_data_t *sdmmc);
static int sdmmc_wait_ready(uint timeout, sdmmc_data_t *sdmmc);
static void sdmmc_init_spi(sdmmc_data_t *sdmmc);

static void sdmmc_deselect(sdmmc_data_t *sdmmc)
{
    uint8_t src = 0xFF;
    sdmmc_spi_cs_high(sdmmc);
    spi_write_blocking(sdmmc->spiPort, &src, 1);
}

/*-----------------------------------------------------------------------*/
/* Select card and wait for ready                                        */
/*-----------------------------------------------------------------------*/
static int sdmmc_select(sdmmc_data_t *sdmmc) /* 1:OK, 0:Timeout */
{
    uint8_t src = 0xFF;
    sdmmc_spi_cs_low(sdmmc);
    spi_write_blocking(sdmmc->spiPort, &src, 1);

    /* Wait for card ready */
    if (sdmmc_wait_ready(500, sdmmc)) {
        return 1;
    }

    sdmmc_deselect(sdmmc);

    /* Timeout */
    return 0;
}

uint64_t sdmmc_get_sector_count(sdmmc_data_t *sdmmc) {
    uint8_t n, csd[16];
    uint32_t st, ed, csize;

    uint64_t sectorCounter;

    uint8_t src = 0xFF;

    if ((sdmmc_send_cmd(CMD9, 0, sdmmc) == 0) && sdmmc_read_datablock(csd, 16, sdmmc)) {
        if ((csd[0] >> 6) == 1) {
            // SDC CSD ver 2
            csize = csd[9] + ((uint16_t)csd[8] << 8) + ((uint32_t)(csd[7] & 63) << 16) + 1;
            sectorCounter = csize << 10;
        } else {
            // SDC CSD ver 1 or MMC
            n = (csd[5] & 15) + ((csd[10] & 128) >> 7) + ((csd[9] & 3) << 1) + 2;
            csize = (csd[8] >> 6) + ((uint16_t)csd[7] << 2) + ((uint16_t)(csd[6] & 3) << 10) + 1;
            sectorCounter = csize << (n - 9);
        }
    } else {
        sectorCounter = 0;
    }

    sdmmc_deselect(sdmmc);

    #ifdef LIMIT_CARD_SIZE
    uint64_t sectorLimit =  LIMIT_CARD_SIZE;

    if (sectorCounter > sectorLimit) {
        sectorCounter = sectorLimit;
    }
    #endif

    return sectorCounter;
}

uint8_t  sdmmc_read_sector(uint32_t sector, uint8_t* buff, uint32_t len, sdmmc_data_t *sdmmc) {
    uint8_t ret=0;
    uint count;
    count = (len % sdmmc->sectSize)  ? ((len / sdmmc->sectSize) + 1) : (len / sdmmc->sectSize);

    /* Check parameter */
    if (!count) {
        return ret;
    }

    /* LBA ot BA conversion (byte addressing cards) */
    if (!(sdmmc->cardType & CT_BLOCK)) {
        sector *= sdmmc->sectSize;
    }

    led_on();

    if (count == 1) {
        /* Single sector read */
        if ((sdmmc_send_cmd(CMD17, sector, sdmmc) == 0) && sdmmc_read_datablock(buff, sdmmc->sectSize, sdmmc)) {
            ret = 1;
        }

    } else {
        /* Multiple sector read */
        if (sdmmc_send_cmd(CMD18, sector, sdmmc) == 0) {
            do {
                if (!sdmmc_read_datablock(buff, sdmmc->sectSize, sdmmc)) {
                    break;
                }
                buff += sdmmc->sectSize;

            } while (--count);

            sdmmc_send_cmd(CMD12, 0, sdmmc); /* STOP_TRANSMISSION */
            ret = 1;
        }
    }

    sdmmc_deselect(sdmmc); // sdmmc_select() is called in function sdmmc_send_cmd()
    led_off();
    return ret;
}

uint8_t sdmmc_write_sector(uint32_t sector, uint8_t *buff, uint32_t len, sdmmc_data_t *sdmmc) {
    uint8_t ret=0;
    uint count;
    count = (len % sdmmc->sectSize)  ? ((len / sdmmc->sectSize)+1) : (len / sdmmc->sectSize);

    /* Check parameter */
    if (!count) {
        return ret;
    }

	/* LBA ==> BA conversion (byte addressing cards) */
    if (!(sdmmc->cardType & CT_BLOCK)) {
        sector *= sdmmc->sectSize;
	}

    led_on();

    if (count == 1) {
		/* Single sector write */
        if ((sdmmc_send_cmd(CMD24, sector, sdmmc) == 0) && sdmmc_write_datablock(buff, 0xFE, sdmmc)) {
            ret = 1;
        }
    } else {
		/* Multiple sector write */
        if (sdmmc->cardType & CT_SDC) {
			/* Predefine number of sectors */
            sdmmc_send_cmd(ACMD23, count, sdmmc);
		}

        if (sdmmc_send_cmd(CMD25, sector, sdmmc) == 0) {
			/* WRITE_MULTIPLE_BLOCK */
            do {
                if (!sdmmc_write_datablock(buff, 0xFC, sdmmc)) {
                    break;
				}

                buff += sdmmc->sectSize;
            } while (--count);

			// LED blinking off
            if (!sdmmc_write_datablock(0, 0xFD, sdmmc)) {
                count = 1; /* STOP_TRAN token */
			}
            ret =1;
        }
    }

    sdmmc_deselect(sdmmc); // sdmmc_select() is called in function sdmmc_send_cmd
    led_off();
}

/* sdmmc spi port initialize*/
void sdmmc_spi_port_init(sdmmc_data_t *sdmmc) {
    spi_init(sdmmc->spiPort, SPI_BAUDRATE_LOW);
    gpio_set_function(SDMMC_PIN_MISO, GPIO_FUNC_SPI);
    gpio_set_function(sdmmc->csPin, GPIO_FUNC_SIO);
    gpio_set_function(SDMMC_PIN_SCK, GPIO_FUNC_SPI);
    gpio_set_function(SDMMC_PIN_MOSI, GPIO_FUNC_SPI);
    gpio_set_dir(sdmmc->csPin, GPIO_OUT);
    sdmmc_spi_cs_high(sdmmc); // deselect

    sdmmc->spiInit = true; // already initialized
}

/* config spi dma*/
#ifdef __SPI_SDMMC_DMA
void config_spi_dma(sdmmc_data_t *sdmmc) {
    sdmmc->read_dma_ch = dma_claim_unused_channel(true);
    sdmmc->write_dma_ch = dma_claim_unused_channel(true);
    sdmmc->dma_rc = dma_channel_get_default_config(sdmmc->read_dma_ch);
    sdmmc->dma_wc = dma_channel_get_default_config(sdmmc->write_dma_ch);
    channel_config_set_transfer_data_size(&(sdmmc->dma_rc), DMA_SIZE_8);
    channel_config_set_transfer_data_size(&(sdmmc->dma_wc), DMA_SIZE_8);
    channel_config_set_read_increment(&(sdmmc->dma_rc), false);
    channel_config_set_write_increment(&(sdmmc->dma_rc), true);
    channel_config_set_read_increment(&(sdmmc->dma_wc), true);
    channel_config_set_write_increment(&(sdmmc->dma_wc), false);
    channel_config_set_dreq(&(sdmmc->dma_rc), spi_get_dreq(sdmmc->spiPort, false));
    channel_config_set_dreq(&(sdmmc->dma_wc), spi_get_dreq(sdmmc->spiPort, true));

    for (int i = 0; i < SDMMC_SECT_SIZE; i++) {
        dummy_block[i] = 0xFF;
    }

    dma_channel_configure(sdmmc->read_dma_ch,
                          &(sdmmc->dma_rc),
                          NULL,
                          &spi_get_hw(sdmmc->spiPort)->dr,
                          sdmmc->sectSize, false);
    dma_channel_configure(sdmmc->write_dma_ch,
                          &(sdmmc->dma_wc),
                          &spi_get_hw(sdmmc->spiPort)->dr,
                          NULL,
                          sdmmc->sectSize, false);
    sdmmc->dmaInit = true;
}
#endif

/* set spi cs low (select)*/
void sdmmc_spi_cs_low(sdmmc_data_t *sdmmc) {
    gpio_put(sdmmc->csPin, 0);
}

/* set spi cs high (deselect)*/
void sdmmc_spi_cs_high(sdmmc_data_t *sdmmc) {
    gpio_put(sdmmc->csPin, 1);
}

/* Initialize SDMMC SPI interface */
static void sdmmc_init_spi(sdmmc_data_t *sdmmc)
{
    sdmmc_spi_port_init(sdmmc); // if not initialized, init it
#ifdef __SPI_SDMMC_DMA
    if (!sdmmc->dmaInit) {
        config_spi_dma(sdmmc);
    }
#endif

    sleep_ms(10);
}

/* Receive a sector data (512 uint8_ts) */
static void sdmmc_read_spi_dma(
    uint8_t *buff, /* Pointer to data buffer */
    uint btr,      /* Number of uint8_ts to receive (even number) */
    sdmmc_data_t *sdmmc) {
#ifdef __SPI_SDMMC_DMA
    dma_channel_set_read_addr(sdmmc->write_dma_ch, dummy_block, false);
    dma_channel_set_trans_count(sdmmc->write_dma_ch, btr, false);

    dma_channel_set_write_addr(sdmmc->read_dma_ch, buff, false);
    dma_channel_set_trans_count(sdmmc->read_dma_ch, btr, false);

    dma_start_channel_mask((1u << (sdmmc->read_dma_ch)) | (1u << (sdmmc->write_dma_ch)));
    dma_channel_wait_for_finish_blocking(sdmmc->read_dma_ch);
#else
    spi_read_blocking(sdmmc->spiPort, 0xFF, buff, btr);
#endif
}


/* Send a sector data (512 uint8_ts) */
static void sdmmc_write_spi_dma(
    const uint8_t *buff, /* Pointer to the data */
    uint btx,            /* Number of uint8_ts to send (even number) */
    sdmmc_data_t *sdmmc) {
#ifdef __SPI_SDMMC_DMA
    dma_channel_set_read_addr(sdmmc->write_dma_ch, buff, false);
    dma_channel_set_trans_count(sdmmc->write_dma_ch, btx, false);
    dma_channel_start(sdmmc->write_dma_ch);
    dma_channel_wait_for_finish_blocking(sdmmc->write_dma_ch);
#else
    spi_write_blocking(sdmmc->spiPort, buff, btx);
#endif
}

/*-----------------------------------------------------------------------*/
/* Wait for card ready                                                   */
/*-----------------------------------------------------------------------*/
static int sdmmc_wait_ready(uint timeout, sdmmc_data_t *sdmmc)
{
    uint8_t dst;
    absolute_time_t timeout_time = make_timeout_time_ms(timeout);
    do {
        spi_read_blocking(sdmmc->spiPort, 0xFF, &dst, 1);
    } while (dst != 0xFF && 0 < absolute_time_diff_us(get_absolute_time(), timeout_time)); /* Wait for card goes ready or timeout */

    return (dst == 0xFF) ? 1 : 0;
}

/*-----------------------------------------------------------------------*/
/* Receive a data packet from the MMC                                    */
/*-----------------------------------------------------------------------*/
// static
bool sdmmc_read_datablock(             /* 1:OK, 0:Error */
                         uint8_t *buff, /* Data buffer */
                         uint btr,     /* Data block length (uint8_t) */
                         sdmmc_data_t *sdmmc)
{
    uint8_t token;
    absolute_time_t timeout_time = make_timeout_time_ms(200);
    do {
        /* Wait for DataStart token in timeout of 200ms */
        spi_read_blocking(sdmmc->spiPort, 0xFF, &token, 1);

    } while ((token == 0xFF) && 0 < absolute_time_diff_us(get_absolute_time(), timeout_time));

    if (token != 0xFE) {
        return false;
    }

    sdmmc_read_spi_dma(buff, btr, sdmmc);
    // Discard CRC
    spi_read_blocking(sdmmc->spiPort, 0xFF, &token, 1);
    spi_read_blocking(sdmmc->spiPort, 0xFF, &token, 1);
    return true; // Function succeeded
}

/*-----------------------------------------------------------------------*/
/* Send a data packet to the MMC                                         */
/*-----------------------------------------------------------------------*/
bool sdmmc_write_datablock(const uint8_t *buff, /* Ponter to 512 uint8_t data to be sent */
                           uint8_t token,        /* Token */
                           sdmmc_data_t *sdmmc)
{
    uint8_t resp;
    if (!sdmmc_wait_ready(500, sdmmc)) {
        return false; /* Wait for card ready */
    }

    // Send token : 0xFE--single block, 0xFC -- multiple block write start, 0xFD -- StopTrans
    spi_write_blocking(sdmmc->spiPort, &token, 1);
    if (token != 0xFD) {
        /* Send data if token is other than StopTran */
        sdmmc_write_spi_dma(buff, sdmmc->sectSize, sdmmc); /* Data */

        token = 0xFF;
        spi_write_blocking(sdmmc->spiPort, &token, 1); // Dummy CRC
        spi_write_blocking(sdmmc->spiPort, &token, 1);

        spi_read_blocking(sdmmc->spiPort, 0xFF, &resp, 1);
        // receive response token: 0x05 -- accepted, 0x0B -- CRC error, 0x0C -- Write Error
        if ((resp & 0x1F) != 0x05) {
            /* Function fails if the data packet was not accepted */
            return false;
        }
    }
    return true;
}

/*-----------------------------------------------------------------------*/
/* Send a command packet to the MMC                                      */
/*-----------------------------------------------------------------------*/
/* Return value: R1 resp (bit7==1:Failed to send) */
uint8_t sdmmc_send_cmd(uint8_t cmd, uint32_t arg, sdmmc_data_t *sdmmc) {
    uint8_t n, res;
    uint8_t tcmd[5];

    /* Send a CMD55 prior to ACMD<n> */
    if (cmd & 0x80) {
        cmd &= 0x7F;
        res = sdmmc_send_cmd(CMD55, 0, sdmmc);
        if (res > 1) {
            return res;
        }
    }

    /* Select the card and wait for ready except to stop multiple block read */
    if (cmd != CMD12)
    {
        sdmmc_deselect(sdmmc);
        if (!sdmmc_select(sdmmc)) {
            return 0xFF;
        }
    }

    /* Send command packet */
    tcmd[0] = 0x40 | cmd;           // 0 1 cmd-index(6) --> 01xxxxxx(b)
    tcmd[1] = (uint8_t)(arg >> 24); // 32 bits argument
    tcmd[2] = (uint8_t)(arg >> 16);
    tcmd[3] = (uint8_t)(arg >> 8);
    tcmd[4] = (uint8_t)arg;
    spi_write_blocking(sdmmc->spiPort, tcmd, 5);
    n = 0x01; /* Dummy CRC + Stop */
    if (cmd == CMD0) {
        /* Valid CRC for CMD0(0) */
        n = 0x95;
    } else if (cmd == CMD8) {
        /* Valid CRC for CMD8(0x1AA) */
        n = 0x87;
    }

    spi_write_blocking(sdmmc->spiPort, &n, 1);

    /* Receive command resp */
    if (cmd == CMD12) {
        /* Diacard following one uint8_t when CMD12 */
        spi_read_blocking(sdmmc->spiPort, 0xFF, &res, 1);
    }

    n = 10; /* Wait for response (10 uint8_ts max) */
    do {
        spi_read_blocking(sdmmc->spiPort, 0xFF, &res, 1);
    } while ((res & 0x80) && --n);

    return res; /* Return received response */
}

/*-----------------------------------------------------------------------*/
/* Initialize disk drive                                                 */
/*-----------------------------------------------------------------------*/
bool sdmmc_init(sdmmc_data_t *sdmmc) {
    uint8_t n, cmd, card_type, src, ocr[4];

    sdmmc->initialized = false;
    // low baudrate
    spi_set_baudrate(sdmmc->spiPort, SPI_BAUDRATE_LOW);
    src = 0xFF;
    sdmmc_spi_cs_low(sdmmc);
    for (n = 10; n; n--) {
        spi_write_blocking(sdmmc->spiPort, &src, 1); // Send 80 dummy clocks
    }
    sdmmc_spi_cs_high(sdmmc);

    card_type = 0;
    /* Put the card SPI/Idle state, R1 bit0=1*/
    if (sdmmc_send_cmd(CMD0, 0, sdmmc) == 1)
    {
        absolute_time_t timeout_time = make_timeout_time_ms(1000);
        if (sdmmc_send_cmd(CMD8, 0x1AA, sdmmc) == 1)
        {                                                     /* SDv2? */
            spi_read_blocking(sdmmc->spiPort, 0xFF, ocr, 4); // R7(5 uint8_ts): R1 read by sdmmc_send_cmd, Get the other 32 bit return value of R7 resp
            if (ocr[2] == 0x01 && ocr[3] == 0xAA)
            { /* Is the card supports vcc of 2.7-3.6V? */
                while ((0 < absolute_time_diff_us(get_absolute_time(), timeout_time)) && sdmmc_send_cmd(ACMD41, 1UL << 30, sdmmc))
                    ; /* Wait for end of initialization with ACMD41(HCS) */
                if ((0 < absolute_time_diff_us(get_absolute_time(), timeout_time)) && sdmmc_send_cmd(CMD58, 0, sdmmc) == 0)
                { /* Check CCS bit in the OCR */
                    spi_read_blocking(sdmmc->spiPort, 0xFF, ocr, 4);
                    card_type = (ocr[0] & 0x40) ? CT_SDC2 | CT_BLOCK : CT_SDC2; /* Card id SDv2 */
                }
            }
        } else {
            /* Not SDv2 card */
            if (sdmmc_send_cmd(ACMD41, 0, sdmmc) <= 1) {
                /* SDv1 or MMC? */
                card_type = CT_SDC1;
                cmd = ACMD41; /* SDv1 (ACMD41(0)) */
            } else {
                card_type = CT_MMC3;
                cmd = CMD1; /* MMCv3 (CMD1(0)) */
            }

            /* Wait for end of initialization */
            while ((0 < absolute_time_diff_us(get_absolute_time(), timeout_time)) && sdmmc_send_cmd(cmd, 0, sdmmc));

            /* Set block length: 512 */
            if (!(0 < absolute_time_diff_us(get_absolute_time(), timeout_time)) || sdmmc_send_cmd(CMD16, SDMMC_SECT_SIZE, sdmmc) != 0) {
                card_type = CT_NO_CARD;
            }
        }
    }

    sdmmc->cardType = card_type; /* Card type */
    sdmmc_deselect(sdmmc);

    if (card_type) {
        // high baudrate
        printf("\nThe actual baudrate(SD/MMC):%d\n",spi_set_baudrate(sdmmc->spiPort, SPI_BAUDRATE_HIGH)); // speed high
        sdmmc->sectSize = SDMMC_SECT_SIZE;
        sdmmc->initialized = true;
    } else {
        sdmmc->initialized = false;
    }

    sdmmc->sectCount = sdmmc_get_sector_count(sdmmc);
    return sdmmc->initialized;
}

bool sdmmc_disk_initialize(spi_inst_t *spi, uint cs_pin, sdmmc_data_t *sdmmc) {
    sdmmc->spiPort = spi;
    sdmmc->csPin = cs_pin;

    if (!sdmmc->spiInit) {
        sdmmc_init_spi(sdmmc);
    }

    return sdmmc_init(sdmmc);
}
