// smart_plug/components/ade9153a/ade9153a_driver.c
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "ade9153a_api.h"

static const char *TAG = "ADE9153A_DRV";

/*===============================================================================
  SPI Transaction Helpers - EXACT timing match with original
  ===============================================================================*/

static void spi_write_16_raw(spi_device_handle_t spi, uint16_t reg, uint16_t data)
{
    uint16_t cmd[2];
    
    // Command format: [addr<<4] & 0xFFF0 (same as original)
    cmd[0] = ((reg << 4) & 0xFFF0);
    cmd[1] = data;
    
    spi_transaction_t trans = {
        .length = 32,           // 16-bit address + 16-bit data
        .tx_buffer = cmd,
        .flags = 0,
    };
    
    spi_device_transmit(spi, &trans);
}

static uint16_t spi_read_16_raw(spi_device_handle_t spi, uint16_t reg)
{
    uint16_t cmd;
    uint16_t rx_data = 0;
    
    // Command format: (((addr<<4) & 0xFFF0) + 8) (same as original)
    cmd = (((reg << 4) & 0xFFF0) + 8);
    
    spi_transaction_t trans = {
        .length = 16,           // 16-bit command
        .rxlength = 16,         // 16-bit response
        .tx_buffer = &cmd,
        .rx_buffer = &rx_data,
        .flags = 0,
    };
    
    spi_device_transmit(spi, &trans);
    return rx_data;
}

static uint32_t spi_read_32_raw(spi_device_handle_t spi, uint16_t reg)
{
    uint16_t cmd;
    uint32_t rx_data = 0;
    uint16_t rx_buffer[2];
    
    // Command format: (((addr<<4) & 0xFFF0) + 8) (same as original)
    cmd = (((reg << 4) & 0xFFF0) + 8);
    
    spi_transaction_t trans = {
        .length = 16,           // 16-bit command
        .rxlength = 32,         // 32-bit response
        .tx_buffer = &cmd,
        .rx_buffer = rx_buffer,
        .flags = 0,
    };
    
    spi_device_transmit(spi, &trans);
    
    // Combine high and low (same as original: returnData = temp_highpacket << 16; returnData = returnData + temp_lowpacket;)
    rx_data = ((uint32_t)rx_buffer[0] << 16) | rx_buffer[1];
    return rx_data;
}

/*===============================================================================
  Public API Implementation
  ===============================================================================*/

bool ade9153a_init(ade9153a_t *dev, uint32_t spi_speed, int cs_pin, 
                   int sck_pin, int mosi_pin, int miso_pin)
{
    esp_err_t ret;
    
    if (!dev) {
        ESP_LOGE(TAG, "Invalid device pointer");
        return false;
    }
    
    // Configure CS pin
    gpio_set_direction(cs_pin, GPIO_MODE_OUTPUT);
    gpio_set_level(cs_pin, 1);  // CS high (inactive)
    
    // SPI bus configuration
    spi_bus_config_t buscfg = {
        .mosi_io_num = mosi_pin,
        .miso_io_num = miso_pin,
        .sclk_io_num = sck_pin,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 32,
        .flags = SPICOMMON_BUSFLAG_MASTER,
    };
    
    // SPI device configuration - EXACT timing match
    spi_device_interface_config_t devcfg = {
        .mode = 0,                          // SPI mode 0 (CPOL=0, CPHA=0)
        .clock_speed_hz = spi_speed,        // 1 MHz default
        .spics_io_num = -1,                  // Manual CS control
        .queue_size = 7,
        .flags = SPI_DEVICE_HALFDUPLEX,      // Half duplex for read operations
        .pre_cb = NULL,
        .post_cb = NULL,
    };
    
    // Initialize SPI bus
    ret = spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SPI bus: %s", esp_err_to_name(ret));
        return false;
    }
    
    // Add device to the bus
    ret = spi_bus_add_device(SPI2_HOST, &devcfg, &dev->spi_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add SPI device: %s", esp_err_to_name(ret));
        spi_bus_free(SPI2_HOST);
        return false;
    }
    
    dev->cs_pin = cs_pin;
    dev->initialized = true;
    
    ESP_LOGI(TAG, "ADE9153A SPI initialized at %lu Hz", spi_speed);
    return true;
}

void ade9153a_write_16(ade9153a_t *dev, uint16_t address, uint16_t data)
{
    if (!dev || !dev->initialized) {
        ESP_LOGE(TAG, "Device not initialized");
        return;
    }
    
    // Manual CS control - same as original digitalWrite(_chipSelect_Pin, LOW);
    gpio_set_level(dev->cs_pin, 0);
    
    // Small delay to match original timing (approximately 1us)
    esp_rom_delay_us(1);
    
    spi_write_16_raw(dev->spi_handle, address, data);
    
    // Small delay to match original timing
    esp_rom_delay_us(1);
    
    // CS high - same as original digitalWrite(_chipSelect_Pin, HIGH);
    gpio_set_level(dev->cs_pin, 1);
}

void ade9153a_write_32(ade9153a_t *dev, uint16_t address, uint32_t data)
{
    if (!dev || !dev->initialized) {
        ESP_LOGE(TAG, "Device not initialized");
        return;
    }
    
    uint16_t high_word = (data >> 16) & 0xFFFF;
    uint16_t low_word = data & 0xFFFF;
    uint16_t cmd;
    
    // Manual CS control
    gpio_set_level(dev->cs_pin, 0);
    
    // Small delay
    esp_rom_delay_us(1);
    
    // Send address
    cmd = ((address << 4) & 0xFFF0);
    spi_transaction_t trans_addr = {
        .length = 16,
        .tx_buffer = &cmd,
        .flags = 0,
    };
    spi_device_transmit(dev->spi_handle, &trans_addr);
    
    // Send high word
    spi_transaction_t trans_high = {
        .length = 16,
        .tx_buffer = &high_word,
        .flags = 0,
    };
    spi_device_transmit(dev->spi_handle, &trans_high);
    
    // Send low word
    spi_transaction_t trans_low = {
        .length = 16,
        .tx_buffer = &low_word,
        .flags = 0,
    };
    spi_device_transmit(dev->spi_handle, &trans_low);
    
    // Small delay
    esp_rom_delay_us(1);
    
    // CS high
    gpio_set_level(dev->cs_pin, 1);
}

uint16_t ade9153a_read_16(ade9153a_t *dev, uint16_t address)
{
    if (!dev || !dev->initialized) {
        ESP_LOGE(TAG, "Device not initialized");
        return 0;
    }
    
    uint16_t result;
    
    // Manual CS control
    gpio_set_level(dev->cs_pin, 0);
    
    // Small delay
    esp_rom_delay_us(1);
    
    result = spi_read_16_raw(dev->spi_handle, address);
    
    // Small delay
    esp_rom_delay_us(1);
    
    // CS high
    gpio_set_level(dev->cs_pin, 1);
    
    return result;
}

uint32_t ade9153a_read_32(ade9153a_t *dev, uint16_t address)
{
    if (!dev || !dev->initialized) {
        ESP_LOGE(TAG, "Device not initialized");
        return 0;
    }
    
    uint32_t result;
    
    // Manual CS control
    gpio_set_level(dev->cs_pin, 0);
    
    // Small delay
    esp_rom_delay_us(1);
    
    result = spi_read_32_raw(dev->spi_handle, address);
    
    // Small delay
    esp_rom_delay_us(1);
    
    // CS high
    gpio_set_level(dev->cs_pin, 1);
    
    return result;
}