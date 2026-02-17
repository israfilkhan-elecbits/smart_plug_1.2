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

// Enable debug to see SPI communication
#define ADE9153A_DEBUG 0

/*===============================================================================
  SPI Transaction Helpers 
  ===============================================================================*/

static void spi_write_bytes(spi_device_handle_t spi, uint8_t *bytes, uint8_t length)
{
    spi_transaction_t trans = {
        .length = length * 8,  // Convert bytes to bits
        .tx_buffer = bytes,
        .rx_buffer = NULL,
        .flags = 0,
    };
    spi_device_transmit(spi, &trans);
}

static void spi_read_bytes(spi_device_handle_t spi, uint16_t cmd, uint8_t *rx_buffer, uint8_t length)
{
    uint8_t tx_buffer[2] = {cmd >> 8, cmd & 0xFF};  // Send command as two bytes
    
    // Send command
    spi_transaction_t trans_cmd = {
        .length = 16,  // 2 bytes * 8 bits
        .tx_buffer = tx_buffer,
        .rx_buffer = NULL,
        .flags = 0,
    };
    spi_device_transmit(spi, &trans_cmd);
    
    esp_rom_delay_us(5);
    
    // Read data bytes
    if (length > 0) {
        spi_transaction_t trans_data = {
            .length = length * 8,
            .rxlength = length * 8,
            .tx_buffer = NULL,
            .rx_buffer = rx_buffer,
            .flags = 0,
        };
        spi_device_transmit(spi, &trans_data);
    }
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
    };
    
    // SPI device configuration - SPI mode 0 
    spi_device_interface_config_t devcfg = {
        .mode = 0,                          // SPI mode 0 (CPOL=0, CPHA=0)
        .clock_speed_hz = spi_speed,        // 1 MHz default
        .spics_io_num = -1,                  // Manual CS control
        .queue_size = 7,
        .flags = 0,
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

/*===============================================================================
  Core SPI Operations - ade9153a_spi_write and ade9153a_spi_read
  ===============================================================================*/

static bool spi_write(ade9153a_t *dev, uint8_t *data, uint8_t length)
{
    if (!dev || !dev->initialized) {
        ESP_LOGE(TAG, "Device not initialized");
        return false;
    }
    
    gpio_set_level(dev->cs_pin, 0);
    esp_rom_delay_us(5);
    
    spi_write_bytes(dev->spi_handle, data, length);
    
    esp_rom_delay_us(5);
    gpio_set_level(dev->cs_pin, 1);
    
    return true;
}

static bool spi_read(ade9153a_t *dev, uint16_t cmd, uint8_t *data, uint8_t length)
{
    if (!dev || !dev->initialized) {
        ESP_LOGE(TAG, "Device not initialized");
        return false;
    }
    
    gpio_set_level(dev->cs_pin, 0);
    esp_rom_delay_us(5);
    
    spi_read_bytes(dev->spi_handle, cmd, data, length);
    
    esp_rom_delay_us(5);
    gpio_set_level(dev->cs_pin, 1);
    
    return true;
}

static uint16_t get_cmd_for(uint16_t address, bool read)
{
    return (((address << 4) & 0xFFF0) | (read ? 8 : 0));
}

/*===============================================================================
  Public API Functions
  ===============================================================================*/

void ade9153a_write_16(ade9153a_t *dev, uint16_t address, uint16_t data)
{
    uint16_t cmd = get_cmd_for(address, false);  // false = write
    
    // Create byte array
    // uint8_t data_bytes[4] = { cmd>>8, cmd, data>>8, data };
    uint8_t data_bytes[4] = {
        (cmd >> 8) & 0xFF,    // Command high byte
        cmd & 0xFF,           // Command low byte
        (data >> 8) & 0xFF,   // Data high byte
        data & 0xFF           // Data low byte
    };
    
#if ADE9153A_DEBUG
    ESP_LOGI(TAG, "write_16: addr=0x%04X, cmd=0x%04X, data=0x%04X", address, cmd, data);
    ESP_LOGI(TAG, "  bytes: [0x%02X, 0x%02X, 0x%02X, 0x%02X]", 
             data_bytes[0], data_bytes[1], data_bytes[2], data_bytes[3]);
#endif
    
    spi_write(dev, data_bytes, 4);
}

void ade9153a_write_32(ade9153a_t *dev, uint16_t address, uint32_t data)
{
    uint16_t cmd = get_cmd_for(address, false);  // false = write
    
    // byte array 
    // uint8_t data_bytes[6] = { cmd>>8, cmd, data>>24, data>>16, data>>8, data };
    uint8_t data_bytes[6] = {
        (cmd >> 8) & 0xFF,           // Command high byte
        cmd & 0xFF,                  // Command low byte
        (data >> 24) & 0xFF,         // Data byte 3 (MSB)
        (data >> 16) & 0xFF,         // Data byte 2
        (data >> 8) & 0xFF,          // Data byte 1
        data & 0xFF                   // Data byte 0 (LSB)
    };
    
#if ADE9153A_DEBUG
    ESP_LOGI(TAG, "write_32: addr=0x%04X, cmd=0x%04X, data=0x%08lX", address, cmd, data);
    ESP_LOGI(TAG, "  bytes: [0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X]", 
             data_bytes[0], data_bytes[1], data_bytes[2], 
             data_bytes[3], data_bytes[4], data_bytes[5]);
#endif
    
    spi_write(dev, data_bytes, 6);
}

uint16_t ade9153a_read_16(ade9153a_t *dev, uint16_t address)
{
    uint16_t cmd = get_cmd_for(address, true);  // true = read
    uint8_t data_buffer[2] = {0};
    
#if ADE9153A_DEBUG
    ESP_LOGI(TAG, "read_16: addr=0x%04X, cmd=0x%04X", address, cmd);
#endif
    
    spi_read(dev, cmd, data_buffer, 2);
    
    // Reconstruct 16-bit value: data_buffer[0]<<8 | data_buffer[1]
    uint16_t result = (data_buffer[0] << 8) | data_buffer[1];
    
#if ADE9153A_DEBUG
    ESP_LOGI(TAG, "  received bytes: [0x%02X, 0x%02X] -> 0x%04X", 
             data_buffer[0], data_buffer[1], result);
#endif
    
    return result;
}

uint32_t ade9153a_read_32(ade9153a_t *dev, uint16_t address)
{
    uint16_t cmd = get_cmd_for(address, true);  // true = read
    uint8_t data_buffer[4] = {0};
    
#if ADE9153A_DEBUG
    ESP_LOGI(TAG, "read_32: addr=0x%04X, cmd=0x%04X", address, cmd);
#endif
    
    spi_read(dev, cmd, data_buffer, 4);
    
    // Reconstruct 32-bit value 
    // data = (data_buffer[0]<<24) | (data_buffer[1]<<16) | (data_buffer[2]<<8) | data_buffer[3]
    uint32_t result = ((uint32_t)data_buffer[0] << 24) |
                      ((uint32_t)data_buffer[1] << 16) |
                      ((uint32_t)data_buffer[2] << 8)  |
                      ((uint32_t)data_buffer[3]);
    
#if ADE9153A_DEBUG
    ESP_LOGI(TAG, "  received bytes: [0x%02X, 0x%02X, 0x%02X, 0x%02X] -> 0x%08lX", 
             data_buffer[0], data_buffer[1], data_buffer[2], data_buffer[3], result);
#endif
    
    return result;
}

void ade9153a_delay_ms(uint32_t delay_ms)
{
    vTaskDelay(pdMS_TO_TICKS(delay_ms));
}