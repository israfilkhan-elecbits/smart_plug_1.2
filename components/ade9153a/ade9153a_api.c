// smart_plug/components/ade9153a/ade9153a_api.c
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "ade9153a_api.h"

static const char *TAG = "ADE9153A_API";

/*===============================================================================
  Setup Function 
  ===============================================================================*/

void ade9153a_setup(ade9153a_t *dev)
{
    if (!dev || !dev->initialized) {
        ESP_LOGE(TAG, "Device not initialized");
        return;
    }
    
    ESP_LOGI(TAG, "Configuring ADE9153A registers...");

    ade9153a_write_16(dev, REG_AI_PGAGAIN, ADE9153A_AI_PGAGAIN);
    ade9153a_write_32(dev, REG_CONFIG0, ADE9153A_CONFIG0);
    ade9153a_write_16(dev, REG_CONFIG1, ADE9153A_CONFIG1);
    ade9153a_write_16(dev, REG_CONFIG2, ADE9153A_CONFIG2);
    ade9153a_write_16(dev, REG_CONFIG3, ADE9153A_CONFIG3);
    ade9153a_write_16(dev, REG_ACCMODE, ADE9153A_ACCMODE);
    ade9153a_write_32(dev, REG_VLEVEL, ADE9153A_VLEVEL);
    ade9153a_write_16(dev, REG_ZX_CFG, ADE9153A_ZX_CFG);
    ade9153a_write_32(dev, REG_MASK, ADE9153A_MASK);
    ade9153a_write_32(dev, REG_ACT_NL_LVL, ADE9153A_ACT_NL_LVL);
    ade9153a_write_32(dev, REG_REACT_NL_LVL, ADE9153A_REACT_NL_LVL);
    ade9153a_write_32(dev, REG_APP_NL_LVL, ADE9153A_APP_NL_LVL);
    ade9153a_write_16(dev, REG_COMPMODE, ADE9153A_COMPMODE);
    ade9153a_write_32(dev, REG_VDIV_RSMALL, ADE9153A_VDIV_RSMALL);
    ade9153a_write_16(dev, REG_EP_CFG, ADE9153A_EP_CFG);
    ade9153a_write_16(dev, REG_EGY_TIME, ADE9153A_EGY_TIME);
    ade9153a_write_16(dev, REG_TEMP_CFG, ADE9153A_TEMP_CFG);
    
    ESP_LOGI(TAG, "ADE9153A configuration complete");
}

/*===============================================================================
  Data Reading Functions 
  ===============================================================================*/

void ade9153a_read_energy(ade9153a_t *dev, energy_regs_t *data)
{
    if (!dev || !data) return;
    
    int32_t temp_reg;
    float temp_value;
    
    // Active Energy 
    temp_reg = (int32_t)ade9153a_read_32(dev, REG_AWATTHR_HI);
    data->ActiveEnergyReg = temp_reg;
    temp_value = (float)temp_reg * CAL_ENERGY_CC_LIB / 1000.0f;
    data->ActiveEnergyValue = temp_value;  // Energy in mWhr
    
    // Fundamental Reactive Energy 
    temp_reg = (int32_t)ade9153a_read_32(dev, REG_AFVARHR_HI);
    data->FundReactiveEnergyReg = temp_reg;
    temp_value = (float)temp_reg * CAL_ENERGY_CC_LIB / 1000.0f;
    data->FundReactiveEnergyValue = temp_value;  // Energy in mVARhr
    
    // Apparent Energy
    temp_reg = (int32_t)ade9153a_read_32(dev, REG_AVAHR_HI);
    data->ApparentEnergyReg = temp_reg;
    temp_value = (float)temp_reg * CAL_ENERGY_CC_LIB / 1000.0f;
    data->ApparentEnergyValue = temp_value;  // Energy in mVAhr
}

void ade9153a_read_power(ade9153a_t *dev, power_regs_t *data)
{
    if (!dev || !data) return;
    
    int32_t temp_reg;
    float temp_value;
    
    // Active Power 
    temp_reg = (int32_t)ade9153a_read_32(dev, REG_AWATT);
    data->ActivePowerReg = temp_reg;
    temp_value = (float)temp_reg * CAL_POWER_CC_LIB / 1000.0f;
    data->ActivePowerValue = temp_value;  // Power in mW
    
    // Fundamental Reactive Power 
    temp_reg = (int32_t)ade9153a_read_32(dev, REG_AFVAR);
    data->FundReactivePowerReg = temp_reg;
    temp_value = (float)temp_reg * CAL_POWER_CC_LIB / 1000.0f;
    data->FundReactivePowerValue = temp_value;  // Power in mVAR
    
    // Apparent Power 
    temp_reg = (int32_t)ade9153a_read_32(dev, REG_AVA);
    data->ApparentPowerReg = temp_reg;
    temp_value = (float)temp_reg * CAL_POWER_CC_LIB / 1000.0f;
    data->ApparentPowerValue = temp_value;  // Power in mVA
}

void ade9153a_read_rms(ade9153a_t *dev, rms_regs_t *data)
{
    if (!dev || !data) return;
    
    uint32_t temp_reg;
    float temp_value;
    
    // Current RMS 
    temp_reg = ade9153a_read_32(dev, REG_AIRMS);
    data->CurrentRMSReg = (int32_t)temp_reg;
    temp_value = (float)temp_reg * CAL_IRMS_CC_LIB / 1000.0f;  // RMS in mA
    data->CurrentRMSValue = temp_value;
    
    // Voltage RMS
    temp_reg = ade9153a_read_32(dev, REG_AVRMS);
    data->VoltageRMSReg = (int32_t)temp_reg;
    temp_value = (float)temp_reg * CAL_VRMS_CC_LIB / 1000.0f;  // RMS in mV
    data->VoltageRMSValue = temp_value;
}

void ade9153a_read_half_rms(ade9153a_t *dev, half_rms_regs_t *data)
{
    if (!dev || !data) return;
    
    uint32_t temp_reg;
    float temp_value;
    
    // Half-cycle Current RMS 
    temp_reg = ade9153a_read_32(dev, REG_AIRMS_OC);
    data->HalfCurrentRMSReg = (int32_t)temp_reg;
    temp_value = (float)temp_reg * CAL_IRMS_CC_LIB / 1000.0f;  // Half-RMS in mA
    data->HalfCurrentRMSValue = temp_value;
    
    // Half-cycle Voltage RMS
    temp_reg = ade9153a_read_32(dev, REG_AVRMS_OC);
    data->HalfVoltageRMSReg = (int32_t)temp_reg;
    temp_value = (float)temp_reg * CAL_VRMS_CC_LIB / 1000.0f;  // Half-RMS in mV
    data->HalfVoltageRMSValue = temp_value;
}

void ade9153a_read_pq(ade9153a_t *dev, pq_regs_t *data)
{
    if (!dev || !data) return;
    
    int32_t temp_reg;
    uint16_t temp;
    float mul_constant;
    float temp_value;
    
    // Power Factor 
    temp_reg = (int32_t)ade9153a_read_32(dev, REG_APF);
    data->PowerFactorReg = temp_reg;
    temp_value = (float)temp_reg / 134217728.0f;
    data->PowerFactorValue = temp_value;
    
    // Period/Frequency 
    temp_reg = (int32_t)ade9153a_read_32(dev, REG_APERIOD);
    data->PeriodReg = temp_reg;
    temp_value = (float)(4000 * 65536) / (float)(temp_reg + 1);
    data->FrequencyValue = temp_value;
    
    // Angle
    temp = ade9153a_read_16(dev, REG_ACCMODE);
    if ((temp & 0x0010) > 0) {
        mul_constant = 0.02109375f;   // multiplier for 60Hz system
    } else {
        mul_constant = 0.017578125f;  // multiplier for 50Hz system
    }
    
    temp_reg = (int16_t)ade9153a_read_16(dev, REG_ANGL_AV_AI);
    data->AngleReg_AV_AI = temp_reg;
    temp_value = temp_reg * mul_constant;
    data->AngleValue_AV_AI = temp_value;
}

void ade9153a_read_acal(ade9153a_t *dev, acal_regs_t *data)
{
    if (!dev || !data) return;
    
    uint32_t temp_reg;
    float temp_value;
    
    // AICC 
    temp_reg = ade9153a_read_32(dev, REG_MS_ACAL_AICC);
    data->AcalAICCReg = (int32_t)temp_reg;
    temp_value = (float)temp_reg / 2048.0f;
    data->AICC = temp_value;
    
    // AICERT
    temp_reg = ade9153a_read_32(dev, REG_MS_ACAL_AICERT);
    data->AcalAICERTReg = (int32_t)temp_reg;
    
    // AVCC 
    temp_reg = ade9153a_read_32(dev, REG_MS_ACAL_AVCC);
    data->AcalAVCCReg = (int32_t)temp_reg;
    temp_value = (float)temp_reg / 2048.0f;
    data->AVCC = temp_value;
    
    // AVCERT 
    temp_reg = ade9153a_read_32(dev, REG_MS_ACAL_AVCERT);
    data->AcalAVCERTReg = (int32_t)temp_reg;
}

/*===============================================================================
  Autocalibration Functions 
  ===============================================================================*/

bool ade9153a_start_acal_ai_normal(ade9153a_t *dev)
{
    uint32_t ready = 0;
    int wait_time = 0;
    
    // Wait for system ready 
    while ((ready & 0x00000001) == 0) {
        if (wait_time > 11) {
            return false;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
        wait_time++;
        ready = ade9153a_read_32(dev, REG_MS_STATUS_CURRENT);
    }
    
    ade9153a_write_32(dev, REG_MS_ACAL_CFG, 0x00000013);
    return true;
}

bool ade9153a_start_acal_ai_turbo(ade9153a_t *dev)
{
    uint32_t ready = 0;
    int wait_time = 0;
    
    // Wait for system ready 
    while ((ready & 0x00000001) == 0) {
        if (wait_time > 15) {
            return false;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
        wait_time++;
        ready = ade9153a_read_32(dev, REG_MS_STATUS_CURRENT);
    }
    
    ade9153a_write_32(dev, REG_MS_ACAL_CFG, 0x00000017);
    return true;
}

bool ade9153a_start_acal_av(ade9153a_t *dev)
{
    uint32_t ready = 0;
    int wait_time = 0;
    
    // Wait for system ready 
    while ((ready & 0x00000001) == 0) {
        if (wait_time > 15) {
            return false;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
        wait_time++;
        ready = ade9153a_read_32(dev, REG_MS_STATUS_CURRENT);
    }
    
    ade9153a_write_32(dev, REG_MS_ACAL_CFG, 0x00000043);
    return true;
}

void ade9153a_stop_acal(ade9153a_t *dev)
{
    ade9153a_write_32(dev, REG_MS_ACAL_CFG, 0x00000000);
}

bool ade9153a_apply_acal(ade9153a_t *dev, float aicc, float avcc)
{
    int32_t aigain;
    int32_t avgain;
    
    aigain = (int32_t)((-(aicc / (CAL_IRMS_CC_LIB * 1000.0f)) - 1.0f) * 134217728.0f);
    avgain = (int32_t)((avcc / (CAL_VRMS_CC_LIB * 1000.0f) - 1.0f) * 134217728.0f);
    
    ade9153a_write_32(dev, REG_AIGAIN, (uint32_t)aigain);
    ade9153a_write_32(dev, REG_AVGAIN, (uint32_t)avgain);
    
    return true;
}

/*===============================================================================
  Temperature Reading
  ===============================================================================*/

void ade9153a_read_temperature(ade9153a_t *dev, temperature_t *data)
{
    if (!dev || !data) return;
    
    uint32_t trim;
    uint16_t gain;
    uint16_t offset;
    uint16_t temp_reg;
    float temp_value;

    // Start temperature acquisition 
    ade9153a_write_16(dev, REG_TEMP_CFG, ADE9153A_TEMP_CFG);
    vTaskDelay(pdMS_TO_TICKS(10));  // 10ms delay
    
    // Read trim values
    trim = ade9153a_read_32(dev, REG_TEMP_TRIM);
    gain = trim & 0xFFFF;           // Extract 16 LSB
    offset = (trim >> 16) & 0xFFFF; // Extract 16 MSB
    
    // Read temperature result
    temp_reg = ade9153a_read_16(dev, REG_TEMP_RSLT);
    
    // formula: ((float)offset / 32.00) - ((float)temp_reg * (float)gain/(float)131072)
    temp_value = ((float)offset / 32.0f) - ((float)temp_reg * (float)gain / 131072.0f);
    
    data->TemperatureReg = temp_reg;
    data->TemperatureVal = temp_value;
}