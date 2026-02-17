// smart_plug/components/ade9153a/include/ade9153a.h
#ifndef ADE9153A_H
#define ADE9153A_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===============================================================================
  Register Address Definitions 
  ===============================================================================*/

#define REG_AIGAIN            0x0000    /* Phase A current gain adjust. */
#define REG_APHASECAL         0x0001    /* Phase A phase correction factor. */
#define REG_AVGAIN            0x0002    /* Phase A voltage gain adjust. */
#define REG_AIRMS_OS          0x0003    /* Phase A current rms offset for filter-based AIRMS calculation. */
#define REG_AVRMS_OS          0x0004    /* Phase A voltage rms offset for filter-based AVRMS calculation. */
#define REG_APGAIN            0x0005    /* Phase A power gain adjust for AWATT, AVA, and AFVAR calculations. */
#define REG_AWATT_OS          0x0006    /* Phase A total active power offset correction for AWATT calculation. */
#define REG_AFVAR_OS          0x0007    /* Phase A fundamental reactive power offset correction for AFVAR calculation. */
#define REG_AVRMS_OC_OS       0x0008    /* Phase A voltage rms offset for fast rms, AVRMS_OC calculation. */
#define REG_AIRMS_OC_OS       0x0009    /* Phase A current rms offset for fast rms, AIRMS_OC calculation. */
#define REG_BIGAIN            0x0010    /* Phase B current gain adjust. */
#define REG_BIRMS_OS          0x0013    /* Phase B current rms offset for filter-based BIRMS calculation. */
#define REG_BIRMS_OC_OS       0x0019    /* Phase B current rms offset for fast rms, BIRMS_OC calculation. */
#define REG_CONFIG0           0x0020    /* DSP configuration register. */
#define REG_VNOM              0x0021    /* Nominal phase voltage rms used in the calculation of apparent power. */
#define REG_DICOEFF           0x0022    /* Value used in the digital integrator algorithm. */
#define REG_BI_PGAGAIN        0x0023    /* PGA gain for Current Channel B ADC. */
#define REG_MS_ACAL_CFG       0x0030    /* MSure autocalibration configuration register. */
#define REG_CT_PHASE_DELAY    0x0049    /* Phase delay of the CT used on Current Channel B. */
#define REG_CT_CORNER         0x004A    /* Corner frequency of the CT. */
#define REG_VDIV_RSMALL       0x004C    /* This register holds the resistance value, in Ω, of the small resistor. */
#define REG_AI_WAV            0x0200    /* Instantaneous Current Channel A waveform. */
#define REG_AV_WAV            0x0201    /* Instantaneous Voltage Channel waveform. */
#define REG_AIRMS             0x0202    /* Phase A filter-based current rms value. */
#define REG_AVRMS             0x0203    /* Phase A filter-based voltage rms value. */
#define REG_AWATT             0x0204    /* Phase A low-pass filtered total active power. */
#define REG_AVA               0x0206    /* Phase A total apparent power. */
#define REG_AFVAR             0x0207    /* Phase A fundamental reactive power. */
#define REG_APF               0x0208    /* Phase A power factor. */
#define REG_AIRMS_OC          0x0209    /* Phase A current fast rms calculation. */
#define REG_AVRMS_OC          0x020A    /* Phase A voltage fast rms calculation. */
#define REG_BI_WAV            0x0210    /* Instantaneous Phase B Current Channel waveform. */
#define REG_BIRMS             0x0212    /* Phase B filter-based current rms value. */
#define REG_BIRMS_OC          0x0219    /* Phase B Current fast rms calculation. */
#define REG_MS_ACAL_AICC      0x0220    /* Current Channel A mSure CC estimation from autocalibration. */
#define REG_MS_ACAL_AICERT    0x0221    /* Current Channel A mSure certainty of autocalibration. */
#define REG_MS_ACAL_BICC      0x0222    /* Current Channel B mSure CC estimation from autocalibration. */
#define REG_MS_ACAL_BICERT    0x0223    /* Current Channel B mSure certainty of autocalibration. */
#define REG_MS_ACAL_AVCC      0x0224    /* Voltage Channel mSure CC estimation from autocalibration. */
#define REG_MS_ACAL_AVCERT    0x0225    /* Voltage Channel mSure certainty of autocalibration. */
#define REG_MS_STATUS_CURRENT 0x0240    /* The MS_STATUS_CURRENT register contains bits that reflect the present state. */
#define REG_VERSION_DSP       0x0241    /* This register indicates the version of the ADE9153A DSP. */
#define REG_VERSION_PRODUCT   0x0242    /* This register indicates the version of the product being used. */
#define REG_AWATT_ACC         0x039D    /* Phase A accumulated total active power. */
#define REG_AWATTHR_LO        0x039E    /* Phase A accumulated total active energy, LSBs. */
#define REG_AWATTHR_HI        0x039F    /* Phase A accumulated total active energy, MSBs. */
#define REG_AVA_ACC           0x03B1    /* Phase A accumulated total apparent power. */
#define REG_AVAHR_LO          0x03B2    /* Phase A accumulated total apparent energy, LSBs. */
#define REG_AVAHR_HI          0x03B3    /* Phase A accumulated total apparent energy, MSBs. */
#define REG_AFVAR_ACC         0x03BB    /* Phase A accumulated fundamental reactive power. */
#define REG_AFVARHR_LO        0x03BC    /* Phase A accumulated fundamental reactive energy, LSBs. */
#define REG_AFVARHR_HI        0x03BD    /* Phase A accumulated fundamental reactive energy, MSBs. */
#define REG_PWATT_ACC         0x03EB    /* Accumulated positive total active power. */
#define REG_NWATT_ACC         0x03EF    /* Accumulated negative total active power. */
#define REG_PFVAR_ACC         0x03F3    /* Accumulated positive fundamental reactive power. */
#define REG_NFVAR_ACC         0x03F7    /* Accumulated negative fundamental reactive power. */
#define REG_IPEAK             0x0400    /* Current peak register. */
#define REG_VPEAK             0x0401    /* Voltage peak register. */
#define REG_STATUS            0x0402    /* Tier 1 interrupt status register. */
#define REG_MASK              0x0405    /* Tier 1 interrupt enable register. */
#define REG_OI_LVL            0x0409    /* Overcurrent RMS_OC detection threshold level. */
#define REG_OIA               0x040A    /* Phase A overcurrent RMS_OC value. */
#define REG_OIB               0x040B    /* Phase B overcurrent RMS_OC value. */
#define REG_USER_PERIOD       0x040E    /* User configured line period value. */
#define REG_VLEVEL            0x040F    /* Register used in the algorithm that computes the fundamental reactive power. */
#define REG_DIP_LVL           0x0410    /* Voltage RMS_OC dip detection threshold level. */
#define REG_DIPA              0x0411    /* Phase A voltage RMS_OC value during a dip condition. */
#define REG_SWELL_LVL         0x0414    /* Voltage RMS_OC swell detection threshold level. */
#define REG_SWELLA            0x0415    /* Phase A voltage RMS_OC value during a swell condition. */
#define REG_APERIOD           0x0418    /* Line period on the Phase A voltage. */
#define REG_ACT_NL_LVL        0x041C    /* No load threshold in the total active power datapath. */
#define REG_REACT_NL_LVL      0x041D    /* No load threshold in the fundamental reactive power datapath. */
#define REG_APP_NL_LVL        0x041E    /* No load threshold in the total apparent power datapath. */
#define REG_PHNOLOAD          0x041F    /* Phase no load register. */
#define REG_WTHR              0x0420    /* Sets the maximum output rate from the digital to frequency converter. */
#define REG_VARTHR            0x0421    /* See WTHR. */
#define REG_VATHR             0x0422    /* See WTHR. */
#define REG_LAST_DATA_32      0x0423    /* This register holds the data read or written during the last 32-bit transaction. */
#define REG_CF_LCFG           0x0425    /* CF calibration pulse width configuration register. */
#define REG_TEMP_TRIM         0x0471    /* Temperature sensor gain and offset. */
#define REG_CHIP_ID_HI        0x0472    /* Chip identification, 32 MSBs. */
#define REG_CHIP_ID_LO        0x0473    /* Chip identification, 32 LSBs. */

/* 16-bit registers */
#define REG_RUN               0x0480    /* Write this register to 1 to start the measurements. */
#define REG_CONFIG1           0x0481    /* Configuration Register 1. */
#define REG_ANGL_AV_AI        0x0485    /* Time between positive to negative zero crossings on Phase A voltage and current. */
#define REG_ANGL_AI_BI        0x0488    /* Time between positive to negative zero crossings on Phase A and Phase B currents. */
#define REG_DIP_CYC           0x048B    /* Voltage RMS_OC dip detection cycle configuration. */
#define REG_SWELL_CYC         0x048C    /* Voltage RMS_OC swell detection cycle configuration. */
#define REG_CFMODE            0x0490    /* CFx configuration register. */
#define REG_COMPMODE          0x0491    /* Computation mode register. Set this register to 0x0005. */
#define REG_ACCMODE           0x0492    /* Accumulation mode register. */
#define REG_CONFIG3           0x0493    /* Configuration Register 3 for configuration of power quality settings. */
#define REG_CF1DEN            0x0494    /* CF1 denominator register. */
#define REG_CF2DEN            0x0495    /* CF2 denominator register. */
#define REG_ZXTOUT            0x0498    /* Zero-crossing timeout configuration register. */
#define REG_ZXTHRSH           0x0499    /* Voltage channel zero-crossing threshold register. */
#define REG_ZX_CFG            0x049A    /* Zero-crossing detection configuration register. */
#define REG_PHSIGN            0x049D    /* Power sign register. */
#define REG_CRC_RSLT          0x04A8    /* This register holds the CRC of the configuration registers. */
#define REG_CRC_SPI           0x04A9    /* The register holds the 16-bit CRC of the data sent out. */
#define REG_LAST_DATA_16      0x04AC    /* This register holds the data read or written during the last 16-bit transaction. */
#define REG_LAST_CMD          0x04AE    /* This register holds the address and the read/write operation request. */
#define REG_CONFIG2           0x04AF    /* Configuration Register 2. */
#define REG_EP_CFG            0x04B0    /* Energy and power accumulation configuration. */
#define REG_PWR_TIME          0x04B1    /* Power update time configuration. */
#define REG_EGY_TIME          0x04B2    /* Energy accumulation update time configuration. */
#define REG_CRC_FORCE         0x04B4    /* This register forces an update of the CRC of configuration registers. */
#define REG_TEMP_CFG          0x04B6    /* Temperature sensor configuration register. */
#define REG_TEMP_RSLT         0x04B7    /* Temperature measurement result. */
#define REG_AI_PGAGAIN        0x04B9    /* This register configures the PGA gain for Current Channel A. */
#define REG_WR_LOCK           0x04BF    /* This register enables the configuration lock feature. */
#define REG_MS_STATUS_IRQ     0x04C0    /* Tier 2 status register for the autocalibration. */
#define REG_EVENT_STATUS      0x04C1    /* Tier 2 status register for power quality event related interrupts. */
#define REG_CHIP_STATUS       0x04C2    /* Tier 2 status register for chip error related interrupts. */
#define REG_UART_BAUD_SWITCH  0x04DC    /* This register switches the UART Baud rate. */
#define REG_VERSION           0x04FE    /* Version of the ADE9153 IC. */
#define REG_AI_WAV_1          0x0600    /* SPI burst read accessible registers organized functionally. */
#define REG_AV_WAV_1          0x0601    /* SPI burst read accessible registers organized functionally. */
#define REG_BI_WAV_1          0x0602    /* SPI burst read accessible registers organized functionally. */
#define REG_AIRMS_1           0x0604    /* SPI burst read accessible registers organized functionally. */
#define REG_BIRMS_1           0x0605    /* SPI burst read accessible registers organized functionally. */
#define REG_AVRMS_1           0x0606    /* SPI burst read accessible registers organized functionally. */
#define REG_AWATT_1           0x0608    /* SPI burst read accessible registers organized functionally. */
#define REG_AFVAR_1           0x060A    /* SPI burst read accessible registers organized functionally. */
#define REG_AVA_1             0x060C    /* SPI burst read accessible registers organized functionally. */
#define REG_APF_1             0x060E    /* SPI burst read accessible registers organized functionally. */
#define REG_AI_WAV_2          0x0610    /* SPI burst read accessible registers organized by phase. */
#define REG_AV_WAV_2          0x0611    /* SPI burst read accessible registers organized by phase. */
#define REG_AIRMS_2           0x0612    /* SPI burst read accessible registers organized by phase. */
#define REG_AVRMS_2           0x0613    /* SPI burst read accessible registers organized by phase. */
#define REG_AWATT_2           0x0614    /* SPI burst read accessible registers organized by phase. */
#define REG_AVA_2             0x0615    /* SPI burst read accessible registers organized by phase. */
#define REG_AFVAR_2           0x0616    /* SPI burst read accessible registers organized by phase. */
#define REG_APF_2             0x0617    /* SPI burst read accessible registers organized by phase. */
#define REG_BI_WAV_2          0x0618    /* SPI burst read accessible registers organized by phase. */
#define REG_BIRMS_2           0x061A    /* SPI burst read accessible registers organized by phase. */

#ifdef __cplusplus
}
#endif

#endif /* ADE9153A_H */