//****************************************************************************************
// @file    SCH1.h
// @brief   Header file for the SCH1 library functions.
//
// @attention
//
// This software is released under the BSD license as follows.
// Copyright (c) 2024, Murata Electronics Oy.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following
// conditions are met:
//    1. Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//    2. Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials
//       provided with the distribution.
//    3. Neither the name of Murata Electronics Oy nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
// FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
// COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
// INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
// HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
// STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
// IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//****************************************************************************************

#ifndef SCH1_H
#define SCH1_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif


/**
 * API return codes
 */
#define SCH1_OK                      0
#define SCH1_ERR_NULL_POINTER       -1
#define SCH1_ERR_INVALID_PARAM      -2
#define SCH1_ERR_SENSOR_INIT        -3
#define SCH1_ERR_OTHER              -4


/**
 * SCH1 Standard requests
 */
// Rate and acceleration
#define REQ_READ_RATE_X1            0x0048000000AC
#define REQ_READ_RATE_Y1            0x00880000009A
#define REQ_READ_RATE_Z1            0x00C80000006D
#define REQ_READ_ACC_X1             0x0108000000F6
#define REQ_READ_ACC_Y1             0x014800000001
#define REQ_READ_ACC_Z1             0x018800000037
#define REQ_READ_ACC_X3             0x01C8000000C0
#define REQ_READ_ACC_Y3             0x02080000002E
#define REQ_READ_ACC_Z3             0x0248000000D9
#define REQ_READ_RATE_X2            0x0288000000EF
#define REQ_READ_RATE_Y2            0x02C800000018
#define REQ_READ_RATE_Z2            0x030800000083
#define REQ_READ_ACC_X2             0x034800000074
#define REQ_READ_ACC_Y2             0x038800000042
#define REQ_READ_ACC_Z2             0x03C8000000B5

// Status
#define REQ_READ_STAT_SUM           0x05080000001C
#define REQ_READ_STAT_SUM_SAT       0x0548000000EB
#define REQ_READ_STAT_COM           0x0588000000DD
#define REQ_READ_STAT_RATE_COM      0x05C80000002A
#define REQ_READ_STAT_RATE_X        0x0608000000C4
#define REQ_READ_STAT_RATE_Y        0x064800000033
#define REQ_READ_STAT_RATE_Z        0x068800000005
#define REQ_READ_STAT_ACC_X         0x06C8000000F2
#define REQ_READ_STAT_ACC_Y         0x070800000069
#define REQ_READ_STAT_ACC_Z         0x07480000009E

// Temperature and traceability
#define REQ_READ_TEMP               0x0408000000B1
#define REQ_READ_SN_ID1             0x0F4800000065
#define REQ_READ_SN_ID2             0x0F8800000053
#define REQ_READ_SN_ID3             0x0FC8000000A4
#define REQ_READ_COMP_ID            0x0F0800000092

// Filters
#define REQ_READ_FILT_RATE          0x0948000000FA
#define REQ_READ_FILT_ACC12         0x0988000000CC
#define REQ_READ_FILT_ACC3          0x09C80000003B
#define REQ_READ_RATE_CTRL          0x0A08000000D5
#define REQ_READ_ACC12_CTRL         0x0A4800000022
#define REQ_READ_ACC3_CTRL          0x0A8800000014
#define REQ_READ_MODE_CTRL          0x0D4800000010
#define REQ_SET_FILT_RATE           0x0968000000    // For building Rate_XYZ1/2 filter setting frame.
#define REQ_SET_FILT_ACC12          0x09A8000000    // For building Acc_XYZ1/2 filter setting frame.
#define REQ_SET_FILT_ACC3           0x09E8000000    // For building Acc_XYZ3 filter setting frame.

// Sensitivity and decimation
#define REQ_SET_RATE_CTRL           0x0A28000000    // For building Rate_XYZ1/2 sensitivity and
                                                    //     Rate_XYZ2 decimation setting frame.
#define REQ_SET_ACC12_CTRL          0x0A68000000    // For building Acc_XYZ1/2 sensitivity and
                                                    //     Acc_XYZ2 decimation setting frame.
#define REQ_SET_ACC3_CTRL           0x0AA8000000    // For building Acc_XYZ3 sensitivity setting frame.
#define REQ_SET_MODE_CTRL           0x0D68000000    // For building MODE-register setting frame.

// DRY/SYNC configuration
#define REQ_READ_USER_IF_CTRL       0x0CC80000007C
#define REQ_SET_USER_IF_CTRL        0x0CE8000000    // For building USER_IF_CTRL -register setting frame.

// Other
#define REQ_SOFTRESET               0x0DA800000AC3  // SPI soft reset command.

/**
 * Frame field masks
 */
#define TA_FIELD_MASK               0xFFC000000000
#define SA_FIELD_MASK               0x7FE000000000
#define DATA_FIELD_MASK             0x00000FFFFF00
#define CRC_FIELD_MASK              0x0000000000FF
#define ERROR_FIELD_MASK            0x001E00000000

/**
 * Macros
 */
#define SPI48_DATA_INT32(a)         (((int32_t)(((a) << 4)  & 0xfffff000UL)) >> 12)
#define SPI48_DATA_UINT32(a)        ((uint32_t)(((a) >> 8)  & 0x000fffffUL))
#define SPI48_DATA_UINT16(a)        ((uint16_t)(((a) >> 8)  & 0x0000ffffUL))
#define GET_TEMPERATURE(a)          ((a) / 100.0f)


/**
 * Filter bypass mode marker
 */
#define SCH1_FILTER_BYPASS   0


/**
 * Status Summary Register bit definitions
 */
#define S_SUM_CMN               0x0080
#define S_SUM_RATE_X            0x0040
#define S_SUM_RATE_Y            0x0020
#define S_SUM_RATE_Z            0x0010
#define S_SUM_ACC_X             0x0008
#define S_SUM_ACC_Y             0x0004
#define S_SUM_ACC_Z             0x0002
#define S_SUM_INIT_RDY          0x0001

/**
 * Saturation Status Summary Register bit definitions
 */
#define S_SUM_SAT_RATE_X1       0x4000
#define S_SUM_SAT_RATE_Y1       0x2000
#define S_SUM_SAT_RATE_Z1       0x1000
#define S_SUM_SAT_ACC_X1        0x0800
#define S_SUM_SAT_ACC_Y1        0x0400
#define S_SUM_SAT_ACC_Z1        0x0200
#define S_SUM_SAT_ACC_X3        0x0100
#define S_SUM_SAT_ACC_Y3        0x0080
#define S_SUM_SAT_ACC_Z3        0x0040
#define S_SUM_SAT_RATE_X2       0x0020
#define S_SUM_SAT_RATE_Y2       0x0010
#define S_SUM_SAT_RATE_Z2       0x0008
#define S_SUM_SAT_ACC_X2        0x0004
#define S_SUM_SAT_ACC_Y2        0x0002
#define S_SUM_SAT_ACC_Z2        0x0001

/**
 * Common Status Register bit definitions
 */
#define S_COM_MCLK              0x0400
#define S_COM_DUAL_CLOCK        0x0200
#define S_COM_DSP               0x0100
#define S_COM_SVM               0x0080
#define S_COM_HV_CP             0x0040
#define S_COM_SUPPLY            0x0020
#define S_COM_TEMP              0x0010
#define S_COM_NMODE             0x0008
#define S_COM_NVM_STS           0x0004
#define S_COM_CMN_STS           0x0002
#define S_COM_CMN_STS_RDY       0x0001

/**
 * Rate Common Status Register bit definitions
 */
#define S_RATE_COM_PRI_AGC      0x0080
#define S_RATE_COM_PRI          0x0040
#define S_RATE_COM_PRI_START    0x0020
#define S_RATE_COM_HV           0x0010
#define S_RATE_COM_SD_STS       0x0004
#define S_RATE_COM_BOND_STS     0x0002
#define S_RATE_COM_STS_RDY      0x0001

/**
 * Rate Status X Register bit definitions
 */
#define S_RATE_X_DEC_SAT        0x0200
#define S_RATE_X_INTP_SAT       0x0100
#define S_RATE_X_STC_DIG        0x0040
#define S_RATE_X_STC_ANA        0x0020
#define S_RATE_X_QC             0x0010

/**
 * Rate Status Y Register bit definitions
 */
#define S_RATE_Y_DEC_SAT        0x0200
#define S_RATE_Y_INTP_SAT       0x0100
#define S_RATE_Y_STC_DIG        0x0040
#define S_RATE_Y_STC_ANA        0x0020
#define S_RATE_Y_QC             0x0010

/**
 * Rate Status Z Register bit definitions
 */
#define S_RATE_Z_DEC_SAT        0x0200
#define S_RATE_Z_INTP_SAT       0x0100
#define S_RATE_Z_STC_DIG        0x0040
#define S_RATE_Z_STC_ANA        0x0020
#define S_RATE_Z_QC             0x0010

/**
 * ACC Status X Register bit definitions
 */
#define S_ACC_X_SAT             0x0400
#define S_ACC_X_DEC_SAT         0x0200
#define S_ACC_X_INTP_SAT        0x0100
#define S_ACC_X_STC_DIG         0x0080
#define S_ACC_X_STC_TCAP        0x0040
#define S_ACC_X_STC_SDD         0x0020
#define S_ACC_X_STC_N           0x0010
#define S_ACC_X_SD_STS          0x0004
#define S_ACC_X_STS             0x0002
#define S_ACC_X_STS_RDY         0x0001

/**
 * ACC Status Y Register bit definitions
 */
#define S_ACC_Y_SAT             0x0400
#define S_ACC_Y_DEC_SAT         0x0200
#define S_ACC_Y_INTP_SAT        0x0100
#define S_ACC_Y_STC_DIG         0x0080
#define S_ACC_Y_STC_TCAP        0x0040
#define S_ACC_Y_STC_SDD         0x0020
#define S_ACC_Y_STC_N           0x0010
#define S_ACC_Y_SD_STS          0x0004
#define S_ACC_Y_STS             0x0002
#define S_ACC_Y_STS_RDY         0x0001

/**
 * ACC Status Z Register bit definitions
 */
#define S_ACC_Z_SAT             0x0400
#define S_ACC_Z_DEC_SAT         0x0200
#define S_ACC_Z_INTP_SAT        0x0100
#define S_ACC_Z_STC_DIG         0x0080
#define S_ACC_Z_STC_TCAP        0x0040
#define S_ACC_Z_STC_SDD         0x0020
#define S_ACC_Z_STC_N           0x0010
#define S_ACC_Z_SD_STS          0x0004
#define S_ACC_Z_STS             0x0002
#define S_ACC_Z_STS_RDY         0x0001


// SCH1 raw unprocessed data values
typedef struct _SCH1_raw_data {
    int32_t Rate1_raw[3];
    int32_t Rate2_raw[3];
    int32_t Acc1_raw[3];
    int32_t Acc2_raw[3];
    int32_t Acc3_raw[3];
    int32_t Temp_raw;
    bool frame_error;
} SCH1_raw_data;


// SCH1 scaled measurement results
typedef struct _SCH1_result {
    float Rate1[3];
    float Rate2[3];
    float Acc1[3];
    float Acc2[3];
    float Acc3[3];
    float Temp;
} SCH1_result;


// SCH1 status data
typedef struct _SCH1_status {
    uint16_t Summary;
    uint16_t Summary_Sat;
    uint16_t Common;
    uint16_t Rate_Common;
    uint16_t Rate_X;
    uint16_t Rate_Y;
    uint16_t Rate_Z;
    uint16_t Acc_X;
    uint16_t Acc_Y;
    uint16_t Acc_Z;
} SCH1_status;


// SCH1 filters
typedef struct _SCH1_filter {
    uint16_t Rate12;
    uint16_t Acc12;
    uint16_t Acc3;
} SCH1_filter;


// SCH1 sensitivities
typedef struct _SCH1_sensitivity {
    uint16_t Rate1;
    uint16_t Rate2;
    uint16_t Acc1;
    uint16_t Acc2;
    uint16_t Acc3;
} SCH1_sensitivity;


// SCH1 decimation
typedef struct _SCH1_decimation {
    uint16_t Rate2;
    uint16_t Acc2;
} SCH1_decimation;


// Measurement axes
enum
{
    AXIS_X,
    AXIS_Y,
    AXIS_Z
};


/**
 * Function prototypes
 */
int         SCH1_init(SCH1_filter sFilter, SCH1_sensitivity sSensitivity, SCH1_decimation sDecimation, bool enableDRY);
void        SCH1_getData(SCH1_raw_data *data);
int         SCH1_getStatus(SCH1_status *Status);
void        SCH1_convert_data(SCH1_raw_data *data_in, SCH1_result *data_out);
bool        SCH1_check_48bit_frame_error(uint64_t *data, int size);
uint64_t    SCH1_sendRequest(uint64_t Request);

int         SCH1_setFilters(uint32_t Freq_Rate12, uint32_t Freq_Acc12, uint32_t Freq_Acc3);
int         SCH1_setRateSensDec(uint16_t Sens_Rate1, uint16_t Sens_Rate2, uint16_t Dec_Rate2);
int         SCH1_getRateSensDec(uint16_t *Sens_Rate1, uint16_t *Sens_Rate2, uint16_t *Dec_Rate2);
int         SCH1_setAccSensDec(uint16_t Sens_Acc1, uint16_t Sens_Acc2, uint16_t Sens_Acc3, uint16_t Dec_Acc2);
int         SCH1_getAccSensDec(uint16_t *Sens_Acc1, uint16_t *Sens_Acc2, uint16_t *Sens_Acc3, uint16_t *Dec_Acc2);

uint32_t    SCH1_convertFilterToBitfield(uint32_t Freq);
uint32_t    SCH1_convertRateSensToBitfield(uint32_t Sens);
uint32_t    SCH1_convertBitfieldToRateSens(uint32_t bitfield);
uint32_t    SCH1_convertAccSensToBitfield(uint32_t Sens);
uint32_t    SCH1_convertBitfieldToAccSens(uint32_t Sens);
uint32_t    SCH1_convertDecimationToBitfield(uint32_t Decimation);
uint32_t    SCH1_convertBitfieldToDecimation(uint32_t bitfield);

bool        SCH1_isValidFilterFreq(uint32_t Freq);
bool        SCH1_isValidSampleRate(uint32_t Freq);
bool        SCH1_isValidRateSens(uint32_t Sens);
bool        SCH1_isValidAccSens(uint32_t Sens);
bool        SCH1_isValidDecimation(uint32_t Decimation);

int         SCH1_enableMeas(bool enableSensor, bool setEOI);
int         SCH1_setDRY(int8_t polarity, bool enable);
void        SCH1_sendSPIreset(void);
bool        SCH1_checkCRC8(uint64_t SPIframe);
bool        SCH1_checkCRC3(uint32_t SPIframe);
bool        SCH1_verifyStatus(SCH1_status *Status);

char        *SCH1_getSnbr(void);
void        SCH1_reset(void);

#ifdef __cplusplus
}
#endif

#endif /* SCH1_H */
