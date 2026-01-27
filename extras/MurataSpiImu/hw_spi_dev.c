#include "hw.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <linux/spi/spidev.h>

// Simple Linux spidev implementation for host testing.
// Uses /dev/spidev0.0 by default, or override with HW_SPI_DEV env var.

static int spi_fd = -1;
static uint32_t spi_speed_hz = 1000000; // default speed (matches main.cpp)

void hw_init(void)
{
    if (spi_fd >= 0) return; // already initialized

    const char *dev = getenv("HW_SPI_DEV");
    if (!dev) dev = "/dev/spidev0.0";

    spi_fd = open(dev, O_RDWR);
    if (spi_fd < 0) {
        fprintf(stderr, "hw_init: open(%s) failed: %s\n", dev, strerror(errno));
        return;
    }

    uint8_t mode = SPI_MODE_0;
    uint8_t bits = 8;

    if (ioctl(spi_fd, SPI_IOC_WR_MODE, &mode) < 0) {
        fprintf(stderr, "hw_init: SPI_IOC_WR_MODE failed: %s\n", strerror(errno));
        close(spi_fd);
        spi_fd = -1;
        return;
    }
    if (ioctl(spi_fd, SPI_IOC_WR_BITS_PER_WORD, &bits) < 0) {
        fprintf(stderr, "hw_init: SPI_IOC_WR_BITS_PER_WORD failed: %s\n", strerror(errno));
        close(spi_fd);
        spi_fd = -1;
        return;
    }
    if (ioctl(spi_fd, SPI_IOC_WR_MAX_SPEED_HZ, &spi_speed_hz) < 0) {
        fprintf(stderr, "hw_init: SPI_IOC_WR_MAX_SPEED_HZ failed: %s\n", strerror(errno));
        close(spi_fd);
        spi_fd = -1;
        return;
    }
}

void hw_EXTRESN_High(void)
{
    // No-op on Linux host. If you need to toggle a GPIO, implement here (e.g. via libgpiod).
}

void hw_EXTRESN_Low(void)
{
    // No-op on Linux host.
}

void hw_CS_High(void)
{
    // spidev toggles CS automatically. Keep as no-op for host tests.
}

void hw_CS_Low(void)
{
    // no-op
}

void hw_delay(uint32_t ms)
{
    usleep((useconds_t)ms * 1000);
}

uint64_t hw_SPI48_Send_Request(uint64_t Request)
{
    if (spi_fd < 0) {
        hw_init();
        if (spi_fd < 0) {
            // initialization failed
            return 0;
        }
    }

    uint8_t tx[6] = {0};
    uint8_t rx[6] = {0};

    // MSB first packing: same as main.cpp
    tx[0] = (Request >> 40) & 0xFF;
    tx[1] = (Request >> 32) & 0xFF;
    tx[2] = (Request >> 24) & 0xFF;
    tx[3] = (Request >> 16) & 0xFF;
    tx[4] = (Request >> 8)  & 0xFF;
    tx[5] = (Request >> 0)  & 0xFF;

    struct spi_ioc_transfer tr;
    memset(&tr, 0, sizeof(tr));
    tr.tx_buf = (unsigned long)tx;
    tr.rx_buf = (unsigned long)rx;
    tr.len = 6;
    tr.speed_hz = spi_speed_hz;
    tr.bits_per_word = 8;

    if (ioctl(spi_fd, SPI_IOC_MESSAGE(1), &tr) < 0) {
        fprintf(stderr, "hw_SPI48_Send_Request: SPI_IOC_MESSAGE failed: %s\n", strerror(errno));
        return 0;
    }

    uint64_t resp = 0;
    resp |= (uint64_t)rx[0] << 40;
    resp |= (uint64_t)rx[1] << 32;
    resp |= (uint64_t)rx[2] << 24;
    resp |= (uint64_t)rx[3] << 16;
    resp |= (uint64_t)rx[4] << 8;
    resp |= (uint64_t)rx[5] << 0;

    // // helpful debug print similar to main.cpp
    // fprintf(stdout, "hw TX: %02X %02X %02X %02X %02X %02X\n",
    //         tx[0], tx[1], tx[2], tx[3], tx[4], tx[5]);
    // fprintf(stdout, "hw RX: %02X %02X %02X %02X %02X %02X\n",
    //         rx[0], rx[1], rx[2], rx[3], rx[4], rx[5]);

    return resp;
}

void hw_timer_setFreq(uint32_t freq)
{
    // No-op in Linux host implementation. Implement if needed.
    (void)freq;
}

void hw_timer_startIT(void)
{
    // No-op in Linux host implementation.
}

void hw_timer_stopIT(void)
{
    // No-op in Linux host implementation.
}
