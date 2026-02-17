// smart_plug/components/ade9153a/include/ade9153a_api.h
#ifndef ADE9153A_API_H
#define ADE9153A_API_H

#include <stdint.h>
#include <stdbool.h>
#include "driver/spi_master.h"  
#include "ade9153a.h"

#ifdef __cplusplus
extern "C" {
#endif

/*===============================================================================
  Configuration Register Defaults 
  ===============================================================================*/

#define ADE9153A_AI_PGAGAIN         0x000A      /* Signal on IAN, current channel gain=16x */
#define ADE9153A_CONFIG0             0x00000000  /* Datapath settings at default */
#define ADE9153A_CONFIG1             0x0300      /* Chip settings at default */
#define ADE9153A_CONFIG2             0x0C00      /* High-pass filter corner, fc=0.625Hz */
#define ADE9153A_CONFIG3             0x0000      /* Peak and overcurrent settings */
#define ADE9153A_ACCMODE             0x0010      /* Energy accumulation modes, Bit 4, 0 for 50Hz, 1 for 60Hz */
#define ADE9153A_VLEVEL              0x002C11E8  /* Assuming Vnom=1/2 of fullscale */
#define ADE9153A_ZX_CFG              0x0000      /* ZX low-pass filter select */
#define ADE9153A_MASK                0x00000100  /* Enable EGYRDY interrupt */
#define ADE9153A_ACT_NL_LVL          0x000033C8
#define ADE9153A_REACT_NL_LVL        0x000033C8
#define ADE9153A_APP_NL_LVL          0x000033C8
#define ADE9153A_RUN_ON              0x0001      /* DSP On */
#define ADE9153A_COMPMODE            0x0005      /* Initialize for proper operation */
#define ADE9153A_VDIV_RSMALL         0x03E8      /* Small resistor on board is 1kOhm=0x3E8 */
#define ADE9153A_EP_CFG              0x0009      /* Energy accumulation configuration - Note: 0x0009 from their code */
#define ADE9153A_EGY_TIME            0x0F9F      /* Accumulate energy for 4000 samples */
#define ADE9153A_TEMP_CFG            0x000C      /* Temperature sensor configuration */

/*===============================================================================
  Calibration Constants
  ===============================================================================*/
#define CAL_IRMS_CC_LIB       0.838190f   /* (uA/code) - Library default */
#define CAL_VRMS_CC_LIB       13.41105f   /* (uV/code) - Library default */
#define CAL_POWER_CC_LIB      1508.743f   /* (uW/code) - Library default */
#define CAL_ENERGY_CC_LIB     0.858307f   /* (uWhr/xTHR_HI code) - Library default */

/*===============================================================================
  Data Structures 
  ===============================================================================*/

typedef struct {
    int32_t ActiveEnergyReg;
    int32_t FundReactiveEnergyReg;
    int32_t ApparentEnergyReg;
    float ActiveEnergyValue;
    float FundReactiveEnergyValue;
    float ApparentEnergyValue;
} energy_regs_t;

typedef struct {
    int32_t ActivePowerReg;
    float ActivePowerValue;
    int32_t FundReactivePowerReg;
    float FundReactivePowerValue;
    int32_t ApparentPowerReg;
    float ApparentPowerValue;
} power_regs_t;

typedef struct {
    int32_t CurrentRMSReg;
    float CurrentRMSValue;
    int32_t VoltageRMSReg;
    float VoltageRMSValue;
} rms_regs_t;

typedef struct {
    int32_t HalfCurrentRMSReg;
    float HalfCurrentRMSValue;
    int32_t HalfVoltageRMSReg;
    float HalfVoltageRMSValue;
} half_rms_regs_t;

typedef struct {
    int32_t PowerFactorReg;
    float PowerFactorValue;
    int32_t PeriodReg;
    float FrequencyValue;
    int32_t AngleReg_AV_AI;
    float AngleValue_AV_AI;
} pq_regs_t;

typedef struct {
    int32_t AcalAICCReg;
    float AICC;
    int32_t AcalAICERTReg;
    int32_t AcalAVCCReg;
    float AVCC;
    int32_t AcalAVCERTReg;
} acal_regs_t;

typedef struct {
    uint16_t TemperatureReg;
    float TemperatureVal;
} temperature_t;

/*===============================================================================
  Driver Structure
  ===============================================================================*/

typedef struct {
    spi_device_handle_t spi_handle;
    int cs_pin;
    bool initialized;
} ade9153a_t;

/*===============================================================================
  Public API Functions
  ===============================================================================*/

/**
 * @brief Initialize ADE9153A with SPI
 */
bool ade9153a_init(ade9153a_t *dev, uint32_t spi_speed, int cs_pin, 
                   int sck_pin, int mosi_pin, int miso_pin);

/**
 * @brief Setup ADE9153A with default configuration
 */
void ade9153a_setup(ade9153a_t *dev);

/**
 * @brief Write 16-bit data to 16-bit register
 */
void ade9153a_write_16(ade9153a_t *dev, uint16_t address, uint16_t data);

/**
 * @brief Write 32-bit data to 32-bit register
 */
void ade9153a_write_32(ade9153a_t *dev, uint16_t address, uint32_t data);

/**
 * @brief Read 16-bit data from register
 */
uint16_t ade9153a_read_16(ade9153a_t *dev, uint16_t address);

/**
 * @brief Read 32-bit data from register
 */
uint32_t ade9153a_read_32(ade9153a_t *dev, uint16_t address);

/**
 * @brief Read energy registers
 */
void ade9153a_read_energy(ade9153a_t *dev, energy_regs_t *data);

/**
 * @brief Read power registers
 */
void ade9153a_read_power(ade9153a_t *dev, power_regs_t *data);

/**
 * @brief Read RMS registers
 */
void ade9153a_read_rms(ade9153a_t *dev, rms_regs_t *data);

/**
 * @brief Read half-cycle RMS registers
 */
void ade9153a_read_half_rms(ade9153a_t *dev, half_rms_regs_t *data);

/**
 * @brief Read power quality registers (PF, frequency, angle)
 */
void ade9153a_read_pq(ade9153a_t *dev, pq_regs_t *data);

/**
 * @brief Read autocalibration registers
 */
void ade9153a_read_acal(ade9153a_t *dev, acal_regs_t *data);

/**
 * @brief Start current channel autocalibration (normal mode)
 */
bool ade9153a_start_acal_ai_normal(ade9153a_t *dev);

/**
 * @brief Start current channel autocalibration (turbo mode)
 */
bool ade9153a_start_acal_ai_turbo(ade9153a_t *dev);

/**
 * @brief Start voltage channel autocalibration
 */
bool ade9153a_start_acal_av(ade9153a_t *dev);

/**
 * @brief Stop autocalibration
 */
void ade9153a_stop_acal(ade9153a_t *dev);

/**
 * @brief Apply autocalibration gains
 */
bool ade9153a_apply_acal(ade9153a_t *dev, float aicc, float avcc);

/**
 * @brief Read temperature
 */
void ade9153a_read_temperature(ade9153a_t *dev, temperature_t *data);

/**
 * @brief Delay function matching their ade9153a_spi_delay_ms
 */
void ade9153a_delay_ms(uint32_t delay_ms);

#ifdef __cplusplus
}
#endif

#endif /* ADE9153A_API_H */