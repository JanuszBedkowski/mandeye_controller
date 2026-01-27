//****************************************************************************************
// @file    SCH1.c
// @brief   SCH1 library functions.
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

#include <stdio.h>

#include "hw.h"
#include "SCH1.h"


/**
 * Internal function prototypes
 */
static uint8_t CRC8(uint64_t SPIframe);
static uint8_t CRC3(uint32_t SPIframe);

/**
 * Internal function definitions
 */


/**
 * @brief Resets SCH1 using EXTRESN pin.
 *
 * @note Resets SCH1 by setting EXTRESN pin low and high.
 *
 * @param None
 *
 * @return None
 */
void SCH1_reset(void)
{
    hw_EXTRESN_Low();
    hw_delay(2);
    hw_EXTRESN_High();
}


/**
 * @brief Resets SCH1 using SPI command.
 *
 * @note Resets SCH1 by setting SOFTRESET_CTRL-bit in CTRL_RESET register.
 *
 * @param None
 *
 * @return None
 */
void SCH1_sendSPIreset(void)
{
    SCH1_sendRequest(REQ_SOFTRESET);
}


/**
 * @brief Checks if the filter value given as parameter is valid.
 *
 * @param Freq - Filter corner frequency [Hz]
 * @note  Value SCH1_FILTER_BYPASS means filter bypass mode.
 *
 * @return true = valid
 *         false = invalid
 */
bool SCH1_isValidFilterFreq(uint32_t Freq)
{
    if (Freq == 13 || Freq == 30 || Freq == 68 || Freq == 235 || Freq == 280 || Freq == 370 || Freq == SCH1_FILTER_BYPASS)
        return true;
    else
        return false;
}


/**
 * @brief Checks if the sensitivity value given as parameter is a valid rate sensitivity.
 *
 * @param Sens - Sensitivity value [LSB/dps]
 *
 * @return true = valid
 *         false = invalid
 */
bool SCH1_isValidRateSens(uint32_t Sens)
{
    if (Sens == 1600 || Sens == 3200 || Sens == 6400)
        return true;
    else
        return false;
}


/**
 * @brief Checks if the sensitivity value given as parameter is a valid accelerometer sensitivity.
 *
 * @param Sens - Sensitivity value [LSB/m/s2]
 *
 * @return true = valid
 *         false = invalid
 */
bool SCH1_isValidAccSens(uint32_t Sens)
{
    if (Sens == 3200 || Sens == 6400 || Sens == 12800 || Sens == 25600)
        return true;
    else
        return false;
}


/**
 * @brief Checks if the decimation value given as parameter is valid.
 *
 * @param Decimation - Decimation ratio
 *
 * @return true = valid
 *         false = invalid
 */
bool SCH1_isValidDecimation(uint32_t Decimation)
{
    if (Decimation == 2 || Decimation == 4 || Decimation == 8 || Decimation == 16 || Decimation == 32)
        return true;
    else
        return false;
}


/**
 * @brief Checks if the sample rate given as parameter is valid.
 *
 * @param Freq - Sample rate [Hz]
 *
 * @return true = valid
 *         false = invalid
 */
bool SCH1_isValidSampleRate(uint32_t Freq)
{
    if ((Freq >= 1) && (Freq <= 10000))
        return true;

    return false;
}


/**
 * @brief Sets filter values for the SCH1.
 *
 * @note Valid filter frequency values for all channels are: 13, 30, 68, 235, 280, 370 [Hz]
 * @note Filter can be set to bypass mode with value SCH1_FILTER_BYPASS
 *
 * @param Freq_Rate12 - Filter for Rate_XYZ1 (interpolated) and Rate_XYZ2 (decimated) outputs.
 * @param Freq_Acc12  - Filter for Acc_XYZ1 (interpolated) and Acc_XYZ2 (decimated) outputs.
 * @param Freq_Acc3   - Filter for Acc_XYZ3 (interpolated) output.
 *
 * @return SCH1_OK = success
 *         SCH1_ERR_* = failure. Please see header file for error definitions.
 */
int SCH1_setFilters(uint32_t Freq_Rate12, uint32_t Freq_Acc12, uint32_t Freq_Acc3)
{
    uint32_t dataField;
    uint64_t requestFrame_Rate12;
    uint64_t responseFrame_Rate12;
    uint64_t requestFrame_Acc12;
    uint64_t responseFrame_Acc12;
    uint64_t requestFrame_Acc3;
    uint64_t responseFrame_Acc3;
    uint8_t  CRCvalue;

    if (SCH1_isValidFilterFreq(Freq_Rate12) == false) {
        return SCH1_ERR_INVALID_PARAM;
    }
    if (SCH1_isValidFilterFreq(Freq_Acc12) == false) {
        return SCH1_ERR_INVALID_PARAM;
    }
    if (SCH1_isValidFilterFreq(Freq_Acc3) == false) {
        return SCH1_ERR_INVALID_PARAM;
    }

    // Set filters for Rate_XYZ1 (interpolated) and Rate_XYZ2 (decimated) outputs.
    requestFrame_Rate12 = REQ_SET_FILT_RATE;
    dataField = SCH1_convertFilterToBitfield(Freq_Rate12);
    requestFrame_Rate12 |= dataField;
    requestFrame_Rate12 <<= 8;
    CRCvalue = CRC8(requestFrame_Rate12);
    requestFrame_Rate12 |= CRCvalue;
    SCH1_sendRequest(requestFrame_Rate12);

    // Set filters for Acc_XYZ1 (interpolated) and Acc_XYZ2 (decimated) outputs.
    requestFrame_Acc12 = REQ_SET_FILT_ACC12;
    dataField = SCH1_convertFilterToBitfield(Freq_Acc12);
    requestFrame_Acc12 |= dataField;
    requestFrame_Acc12 <<= 8;
    CRCvalue = CRC8(requestFrame_Acc12);
    requestFrame_Acc12 |= CRCvalue;
    SCH1_sendRequest(requestFrame_Acc12);

    // Set filters for Acc_XYZ3 (interpolated) output.
    requestFrame_Acc3 = REQ_SET_FILT_ACC3;
    dataField = SCH1_convertFilterToBitfield(Freq_Acc3);
    requestFrame_Acc3 |= dataField;
    requestFrame_Acc3 <<= 8;
    CRCvalue = CRC8(requestFrame_Acc3);
    requestFrame_Acc3 |= CRCvalue;
    SCH1_sendRequest(requestFrame_Acc3);

    // Read back filter register contents.
    SCH1_sendRequest(REQ_READ_FILT_RATE);
    responseFrame_Rate12 = SCH1_sendRequest(REQ_READ_FILT_ACC12);
    responseFrame_Acc12 = SCH1_sendRequest(REQ_READ_FILT_ACC3);
    responseFrame_Acc3 = SCH1_sendRequest(REQ_READ_FILT_ACC3);

    // Check that return frame is not blank.
    if ((responseFrame_Rate12 == 0xFFFFFFFFFFFF) || (responseFrame_Rate12 == 0x00))
        return SCH1_ERR_OTHER;
    if ((responseFrame_Acc12 == 0xFFFFFFFFFFFF) || (responseFrame_Acc12 == 0x00))
        return SCH1_ERR_OTHER;
    if ((responseFrame_Acc3 == 0xFFFFFFFFFFFF) || (responseFrame_Acc3 == 0x00))
        return SCH1_ERR_OTHER;

    // Check that Source Address matches Target Address.
    if (((requestFrame_Rate12 & TA_FIELD_MASK) >> 38) != ((responseFrame_Rate12 & SA_FIELD_MASK) >> 37))
        return SCH1_ERR_OTHER;
    if (((requestFrame_Acc12 & TA_FIELD_MASK) >> 38) != ((responseFrame_Acc12 & SA_FIELD_MASK) >> 37))
        return SCH1_ERR_OTHER;
    if (((requestFrame_Acc3 & TA_FIELD_MASK) >> 38) != ((responseFrame_Acc3 & SA_FIELD_MASK) >> 37))
        return SCH1_ERR_OTHER;

    // Check that read and written data match.
    if ((requestFrame_Rate12 & DATA_FIELD_MASK) != (responseFrame_Rate12 & DATA_FIELD_MASK))
        return SCH1_ERR_OTHER;
    if ((requestFrame_Acc12 & DATA_FIELD_MASK) != (responseFrame_Acc12 & DATA_FIELD_MASK))
        return SCH1_ERR_OTHER;
    if ((requestFrame_Acc3 & DATA_FIELD_MASK) != (responseFrame_Acc3 & DATA_FIELD_MASK))
        return SCH1_ERR_OTHER;

    return SCH1_OK;
}


/**
 * @brief Sets Rate_XYZ1 & Rate_XYZ2 channel sensitivities and Rate_XYZ2 channel decimation for the SCH1.
 *
 * @note Valid sensitivities for rate channels are: 1600, 3200, 6400 [LSB/dps]
 * @note Valid decimations for Rate_XYZ2 channel are: 2, 4, 8, 16, 32
 *
 * @param Sens_Rate1 - Sensitivity for Rate_XYZ1 (interpolated) output.
 * @param Sens_Rate2 - Sensitivity for Rate_XYZ2 (decimated) output.
 * @param Dec_Rate2  - Decimation for Rate_XYZ2 output (FPRIM / Dec_Rate2).
 *
 * @note Here the same decimation is used for all Rate_XYZ2 axis.
 *
 * @return SCH1_OK = success
 *         SCH1_ERR_* = failure. Please see header file for error definitions.
 */
int SCH1_setRateSensDec(uint16_t Sens_Rate1, uint16_t Sens_Rate2, uint16_t Dec_Rate2)
{
    uint32_t dataField;
    uint32_t bitField;
    uint64_t requestFrame_Rate_Ctrl;
    uint64_t responseFrame_Rate_Ctrl;
    uint8_t  CRCvalue;

    if (SCH1_isValidRateSens(Sens_Rate1) == false) {
        return SCH1_ERR_INVALID_PARAM;
    }
    if (SCH1_isValidRateSens(Sens_Rate2) == false) {
        return SCH1_ERR_INVALID_PARAM;
    }
    if (SCH1_isValidDecimation(Dec_Rate2) == false) {
        return SCH1_ERR_INVALID_PARAM;
    }

    // Set sensitivities for Rate_XYZ1 (interpolated) and Rate_XYZ2 (decimated) outputs.
    // Also set decimation for Rate_XYZ2.
    requestFrame_Rate_Ctrl = REQ_SET_RATE_CTRL;
    dataField = SCH1_convertRateSensToBitfield(Sens_Rate1);
    dataField <<= 3;
    bitField = SCH1_convertRateSensToBitfield(Sens_Rate2);
    dataField |= bitField;
    dataField <<= 3;
    bitField = SCH1_convertDecimationToBitfield(Dec_Rate2);
    dataField |= bitField;
    dataField <<= 3;
    dataField |= bitField;
    dataField <<= 3;
    dataField |= bitField;

    requestFrame_Rate_Ctrl |= dataField;
    requestFrame_Rate_Ctrl <<= 8;
    CRCvalue = CRC8(requestFrame_Rate_Ctrl);
    requestFrame_Rate_Ctrl |= CRCvalue;
    SCH1_sendRequest(requestFrame_Rate_Ctrl);

    // Read back rate control register contents.
    SCH1_sendRequest(REQ_READ_RATE_CTRL);
    responseFrame_Rate_Ctrl = SCH1_sendRequest(REQ_READ_RATE_CTRL);

    // Check that return frame is not blank.
    if ((responseFrame_Rate_Ctrl == 0xFFFFFFFFFFFF) || (responseFrame_Rate_Ctrl == 0x00))
        return SCH1_ERR_OTHER;

    // Check that Source Address matches Target Address.
    if (((requestFrame_Rate_Ctrl & TA_FIELD_MASK) >> 38) != ((responseFrame_Rate_Ctrl & SA_FIELD_MASK) >> 37))
        return SCH1_ERR_OTHER;

    // Check that read and written data match.
    if ((requestFrame_Rate_Ctrl & DATA_FIELD_MASK) != (responseFrame_Rate_Ctrl & DATA_FIELD_MASK))
        return SCH1_ERR_OTHER;

    return SCH1_OK;
}


/**
 * @brief Gets Rate_XYZ1 & Rate_XYZ2 channel sensitivities and Rate_XYZ2 channel decimation for the SCH1.
 *
 * @param Sens_Rate1 - Sensitivity for Rate_XYZ1 (interpolated) output.
 * @param Sens_Rate2 - Sensitivity for Rate_XYZ2 (decimated) output.
 * @param Dec_Rate2  - Decimation for Rate_XYZ2 output (FPRIM / Dec_Rate2).
 *
 * @note It is assumed that the same decimation is used for all Rate_XYZ2 axis.
 *
 * @return SCH1_OK = success
 *         SCH1_ERR_* = failure. Please see header file for error definitions.
 */
int SCH1_getRateSensDec(uint16_t *Sens_Rate1, uint16_t *Sens_Rate2, uint16_t *Dec_Rate2)
{
    uint32_t dataField;
    uint64_t responseFrame_Rate_Ctrl;

    // Read Rate control register contents.
    SCH1_sendRequest(REQ_READ_RATE_CTRL);
    responseFrame_Rate_Ctrl = SCH1_sendRequest(REQ_READ_RATE_CTRL);

    // Check that return frame is not blank.
    if ((responseFrame_Rate_Ctrl == 0xFFFFFFFFFFFF) || (responseFrame_Rate_Ctrl == 0x00))
        return SCH1_ERR_OTHER;

    // Check that Source Address matches Target Address.
    if (((REQ_READ_RATE_CTRL & TA_FIELD_MASK) >> 38) != ((responseFrame_Rate_Ctrl & SA_FIELD_MASK) >> 37))
        return SCH1_ERR_OTHER;


    // Get sensitivities for Rate_XYZ1 (interpolated) and Rate_XYZ2 (decimated) outputs.
    // Also get decimation for Rate_XYZ2.

    // First get the Rate_XYZ2 decimation
    dataField = (uint16_t)(responseFrame_Rate_Ctrl >> 8) & 0x07;
    *Dec_Rate2 = (uint16_t)SCH1_convertBitfieldToDecimation(dataField);

    // Rate_XYZ2 sensitivity
    dataField = (uint16_t)(responseFrame_Rate_Ctrl >> 17) & 0x07;
    *Sens_Rate2 = (uint16_t)SCH1_convertBitfieldToRateSens(dataField);

    // Rate_XYZ1 sensitivity
    dataField = (uint16_t)(responseFrame_Rate_Ctrl >> 20) & 0x07;
    *Sens_Rate1 = (uint16_t)SCH1_convertBitfieldToRateSens(dataField);

    return SCH1_OK;
}


/**
 * @brief Sets Acc_XYZ1/2/3 channel sensitivities and Acc_XYZ2 channel decimation for the SCH1.
 *
 * @note Valid sensitivities for all Acc channels are: 3200, 6400, 12800, 25600 [LSB/m/s2]
 * @note Valid decimations for Acc_XYZ3 channel are: 2, 4, 8, 16, 32
 *
 * @param Sens_Acc1 - Sensitivity for Acc_XYZ1 (interpolated) output.
 * @param Sens_Acc2 - Sensitivity for Acc_XYZ2 (decimated) output.
 * @param Sens_Acc3 - Sensitivity for Acc_XYZ3 (interpolated) output.
 * @param Dec_Acc2  - Decimation for Acc_XYZ2 output (FPRIM / Dec_Acc2).
 *
 * @note Here the same decimation is used for all Acc_XYZ2 axis.
 *
 * @return SCH1_OK = success
 *         SCH1_ERR_* = failure. Please see header file for error definitions.
 */
int SCH1_setAccSensDec(uint16_t Sens_Acc1, uint16_t Sens_Acc2, uint16_t Sens_Acc3, uint16_t Dec_Acc2)
{
    uint32_t dataField;
    uint32_t bitField;
    uint64_t requestFrame_Acc12_Ctrl;
    uint64_t responseFrame_Acc12_Ctrl;
    uint64_t requestFrame_Acc3_Ctrl;
    uint64_t responseFrame_Acc3_Ctrl;
    uint8_t  CRCvalue;

    if (SCH1_isValidAccSens(Sens_Acc1) == false) {
        return SCH1_ERR_INVALID_PARAM;
    }
    if (SCH1_isValidAccSens(Sens_Acc2) == false) {
        return SCH1_ERR_INVALID_PARAM;
    }
    if (SCH1_isValidAccSens(Sens_Acc3) == false) {
        return SCH1_ERR_INVALID_PARAM;
    }
    if (SCH1_isValidDecimation(Dec_Acc2) == false) {
        return SCH1_ERR_INVALID_PARAM;
    }

    // Set sensitivities for Acc_XYZ1 (interpolated) and Acc_XYZ2 (decimated) outputs.
    // Also set decimation for Acc_XYZ2.
    requestFrame_Acc12_Ctrl = REQ_SET_ACC12_CTRL;
    dataField = SCH1_convertAccSensToBitfield(Sens_Acc1);
    dataField <<= 3;
    bitField = SCH1_convertAccSensToBitfield(Sens_Acc2);
    dataField |= bitField;
    dataField <<= 3;
    bitField = SCH1_convertDecimationToBitfield(Dec_Acc2);
    dataField |= bitField;
    dataField <<= 3;
    dataField |= bitField;
    dataField <<= 3;
    dataField |= bitField;

    requestFrame_Acc12_Ctrl |= dataField;
    requestFrame_Acc12_Ctrl <<= 8;
    CRCvalue = CRC8(requestFrame_Acc12_Ctrl);
    requestFrame_Acc12_Ctrl |= CRCvalue;
    SCH1_sendRequest(requestFrame_Acc12_Ctrl);

    // Set sensitivity for Acc_XYZ3 (interpolated) output.
    requestFrame_Acc3_Ctrl = REQ_SET_ACC3_CTRL;
    dataField = SCH1_convertAccSensToBitfield(Sens_Acc3);
    requestFrame_Acc3_Ctrl |= dataField;
    requestFrame_Acc3_Ctrl <<= 8;
    CRCvalue = CRC8(requestFrame_Acc3_Ctrl);
    requestFrame_Acc3_Ctrl |= CRCvalue;
    SCH1_sendRequest(requestFrame_Acc3_Ctrl);

    // Read back sensitivity control register contents.
    SCH1_sendRequest(REQ_READ_ACC12_CTRL);
    responseFrame_Acc12_Ctrl = SCH1_sendRequest(REQ_READ_ACC3_CTRL);
    responseFrame_Acc3_Ctrl = SCH1_sendRequest(REQ_READ_ACC3_CTRL);

    // Check that return frame is not blank.
    if ((responseFrame_Acc12_Ctrl == 0xFFFFFFFFFFFF) || (responseFrame_Acc12_Ctrl == 0x00))
        return SCH1_ERR_OTHER;
    if ((responseFrame_Acc3_Ctrl == 0xFFFFFFFFFFFF) || (responseFrame_Acc3_Ctrl == 0x00))
        return SCH1_ERR_OTHER;

    // Check that Source Address matches Target Address.
    if (((requestFrame_Acc12_Ctrl & TA_FIELD_MASK) >> 38) != ((responseFrame_Acc12_Ctrl & SA_FIELD_MASK) >> 37))
        return SCH1_ERR_OTHER;
    if (((requestFrame_Acc3_Ctrl & TA_FIELD_MASK) >> 38) != ((responseFrame_Acc3_Ctrl & SA_FIELD_MASK) >> 37))
        return SCH1_ERR_OTHER;

    // Check that read and written data match.
    if ((requestFrame_Acc12_Ctrl & DATA_FIELD_MASK) != (responseFrame_Acc12_Ctrl & DATA_FIELD_MASK))
        return SCH1_ERR_OTHER;
    if ((requestFrame_Acc3_Ctrl & DATA_FIELD_MASK) != (responseFrame_Acc3_Ctrl & DATA_FIELD_MASK))
        return SCH1_ERR_OTHER;

    return SCH1_OK;
}


/**
 * @brief Gets Acc_XYZ1/2/3 channel sensitivities and Acc_XYZ2 channel decimation for the SCH1.
 *
 * @param Sens_Acc1 - Sensitivity for Acc_XYZ1 (interpolated) output.
 * @param Sens_Acc2 - Sensitivity for Acc_XYZ2 (decimated) output.
 * @param Sens_Acc3 - Sensitivity for Acc_XYZ3 (interpolated) output.
 * @param Dec_Acc2  - Decimation for Acc_XYZ2 output (FPRIM / Dec_Acc2).
 *
 * @note It is assumed that the same decimation is used for all Acc_XYZ2 axis.
 *
 * @return SCH1_OK = success
 *         SCH1_ERR_* = failure. Please see header file for error definitions.
 */
int SCH1_getAccSensDec(uint16_t *Sens_Acc1, uint16_t *Sens_Acc2, uint16_t *Sens_Acc3, uint16_t *Dec_Acc2)
{
    uint32_t dataField;
    uint64_t responseFrame_Acc12_Ctrl;
    uint64_t responseFrame_Acc3_Ctrl;

    // Read Acc12 and Acc3 control register contents.
    SCH1_sendRequest(REQ_READ_ACC12_CTRL);
    responseFrame_Acc12_Ctrl = SCH1_sendRequest(REQ_READ_ACC3_CTRL);
    responseFrame_Acc3_Ctrl = SCH1_sendRequest(REQ_READ_ACC3_CTRL);


    // Check that return frame is not blank.
    if ((responseFrame_Acc12_Ctrl == 0xFFFFFFFFFFFF) || (responseFrame_Acc12_Ctrl == 0x00))
        return SCH1_ERR_OTHER;
    if ((responseFrame_Acc3_Ctrl == 0xFFFFFFFFFFFF) || (responseFrame_Acc3_Ctrl == 0x00))
        return SCH1_ERR_OTHER;

    // Check that Source Address matches Target Address.
    if (((REQ_READ_ACC12_CTRL & TA_FIELD_MASK) >> 38) != ((responseFrame_Acc12_Ctrl & SA_FIELD_MASK) >> 37))
        return SCH1_ERR_OTHER;
    if (((REQ_READ_ACC3_CTRL & TA_FIELD_MASK) >> 38) != ((responseFrame_Acc3_Ctrl & SA_FIELD_MASK) >> 37))
        return SCH1_ERR_OTHER;


    // Get sensitivities for Acc_XYZ1 (interpolated) and Acc_XYZ2 (decimated) outputs.
    // Also get decimation for Acc_XYZ2.

    // Firat get the Acc_XYZ2 decimation
    dataField = (uint16_t)(responseFrame_Acc12_Ctrl >> 8) & 0x07;
    *Dec_Acc2 = (uint16_t)SCH1_convertBitfieldToDecimation(dataField);

    // Acc_XYZ2 sensitivity
    dataField = (uint16_t)(responseFrame_Acc12_Ctrl >> 17) & 0x07;
    *Sens_Acc2 = (uint16_t)SCH1_convertBitfieldToAccSens(dataField);

    // Acc_XYZ1 sensitivity
    dataField = (uint16_t)(responseFrame_Acc12_Ctrl >> 20) & 0x07;
    *Sens_Acc1 = (uint16_t)SCH1_convertBitfieldToAccSens(dataField);

    // Acc_XYZ3 sensitivity
    dataField = (uint16_t)(responseFrame_Acc3_Ctrl >> 8) & 0x07;
    *Sens_Acc3 = (uint16_t)SCH1_convertBitfieldToAccSens(dataField);

    return SCH1_OK;
}


/**
 * @brief Activates/deactivates SCH1 measurement mode and sets the EOI (End Of Initialization) bit if needed.
 *
 * @param enableSensor - Enables/disables the sensor.
 * @param setEOI - Sets EOI-bit. Locks all R/W registers, except soft reset. Can only be set when no errors in common status.
 *
 * @return SCH1_OK = success
 *         SCH1_ERR_* = failure. Please see header file for error definitions.
 */
int SCH1_enableMeas(bool enableSensor, bool setEOI)
{
    uint64_t requestFrame_Mode_Ctrl;
    uint64_t responseFrame_Mode_Ctrl;
    uint8_t  CRCvalue;

    requestFrame_Mode_Ctrl = REQ_SET_MODE_CTRL;

    // Handle EN_SENSOR -bit
    if (enableSensor)
        requestFrame_Mode_Ctrl |= 0x01;

    // Handle EOI_CTRL -bit
    if (setEOI)
        requestFrame_Mode_Ctrl |= 0x02;

    requestFrame_Mode_Ctrl <<= 8;
    CRCvalue = CRC8(requestFrame_Mode_Ctrl);
    requestFrame_Mode_Ctrl |= CRCvalue;
    SCH1_sendRequest(requestFrame_Mode_Ctrl);

    // Read back sensitivity control register contents.
    SCH1_sendRequest(REQ_READ_MODE_CTRL);
    responseFrame_Mode_Ctrl = SCH1_sendRequest(REQ_READ_MODE_CTRL);

    // Check that return frame is not blank.
    if ((responseFrame_Mode_Ctrl == 0xFFFFFFFFFFFF) || (responseFrame_Mode_Ctrl == 0x00))
        return SCH1_ERR_OTHER;

    // Check that Source Address matches Target Address.
    if (((requestFrame_Mode_Ctrl & TA_FIELD_MASK) >> 38) != ((responseFrame_Mode_Ctrl & SA_FIELD_MASK) >> 37))
        return SCH1_ERR_OTHER;

    return SCH1_OK;
}


/**
 * @brief Configures Data Ready (DRY) -pin
 *
 * @param polarity - Data Ready polarity control: 0 - high active (default), 1 - low active. -1 = don't care
 * @param enable   - Data Ready -pin disable/enable
 *
 * @return SCH1_OK = success
 *         SCH1_ERR_* = failure. Please see header file for error definitions.
 */
int SCH1_setDRY(int8_t polarity, bool enable)
{
    uint64_t requestFrame_User_If_Ctrl;
    uint64_t responseFrame_User_If_Ctrl;
    uint64_t dataContent;
    uint8_t  CRCvalue;

    if ((polarity < -1) || (polarity > 1))
        return SCH1_ERR_INVALID_PARAM;

    // Read USER_IF_CTRL -register content
    SCH1_sendRequest(REQ_READ_USER_IF_CTRL);
    responseFrame_User_If_Ctrl = SCH1_sendRequest(REQ_READ_USER_IF_CTRL);
    dataContent = (responseFrame_User_If_Ctrl & DATA_FIELD_MASK) >> 8;

    if (polarity == 0)
        dataContent &= (uint16_t)~0x40;   // Set DRY active high (0b01000000)
    else if (polarity == 1)
        dataContent |= 0x40;              // Set DRY active low

    if (enable)
        dataContent |= 0x20;              // Set DRY enabled (0b00100000)
    else
        dataContent &= (uint16_t)~0x20;   // Set DRY disabled

    requestFrame_User_If_Ctrl = REQ_SET_USER_IF_CTRL;
    requestFrame_User_If_Ctrl |= dataContent;
    requestFrame_User_If_Ctrl <<= 8;
    CRCvalue = CRC8(requestFrame_User_If_Ctrl);
    requestFrame_User_If_Ctrl |= CRCvalue;
    SCH1_sendRequest(requestFrame_User_If_Ctrl);

    // Read back sensitivity control register contents.
    SCH1_sendRequest(REQ_READ_USER_IF_CTRL);
    responseFrame_User_If_Ctrl = SCH1_sendRequest(REQ_READ_USER_IF_CTRL);

    // Check that return frame is not blank.
    if ((responseFrame_User_If_Ctrl == 0xFFFFFFFFFFFF) || (responseFrame_User_If_Ctrl == 0x00))
        return SCH1_ERR_OTHER;

    // Check that Source Address matches Target Address.
    if (((requestFrame_User_If_Ctrl & TA_FIELD_MASK) >> 38) != ((responseFrame_User_If_Ctrl & SA_FIELD_MASK) >> 37))
        return SCH1_ERR_OTHER;

    // Check that read and written data match.
    if ((requestFrame_User_If_Ctrl & DATA_FIELD_MASK) != (responseFrame_User_If_Ctrl & DATA_FIELD_MASK))
        return SCH1_ERR_OTHER;

    return SCH1_OK;
}


/**
 * @brief Returns bitfield for setting output channel filters to desired frequency value.
 *
 * @note Valid filter frequency values for all channels are: 13, 30, 68, 235, 280, 370 [Hz]
 * @note SCH1_FILTER_BYPASS is used for filter bypass mode.
 * @note Here all XYZ-axis filters are set to same value.
 *
 * @param Freq - Filter for an output channel (Rate_XYZ1, Freq_Acc12, Freq_Acc3).
 *
 * @return Equivalent bit field for building the filter setting SPI-frame.
 */
uint32_t SCH1_convertFilterToBitfield(uint32_t Freq)
{
    switch (Freq)
    {
        case 13:
            return 0x092;   // 010 010 010
        case 30:
            return 0x049;   // 001 001 001
        case 68:
            return 0x000;   // 000 000 000
        case 235:
            return 0x16D;   // 101 101 101
        case 280:
            return 0x0DB;   // 011 011 011
        case 370:
            return 0x124;   // 100 100 100
        case SCH1_FILTER_BYPASS:
            return 0x1FF;   // 111 111 111, filter bypass mode
        default:
            return 0x000;
    }
}


/**
 * @brief Returns bitfield for setting output channel rate sensitivities to desired value.
 *
 * @note Valid sensitivity values for all rate channels are: 1600, 3200, 6400 [LSB/dps]
 *
 * @param Sens - Sensitivity for a rate output channel (Rate_XYZ1 & Rate_XYZ2).
 *
 * @return Equivalent bit field for building the rate sensitivity setting SPI-frame.
 */
uint32_t SCH1_convertRateSensToBitfield(uint32_t Sens)
{
    switch (Sens)
    {
        case 1600:
            return 0x02;   // 010
        case 3200:
            return 0x03;   // 011
        case 6400:
            return 0x04;   // 100
        default:
            return 0x01;
    }
}


/**
 * @brief Returns rate sensitivity that equals to RATE_CTRL-register bitfield.
 *
 * @param bitfield - RATE_CTRL-register bitfield for a rate output channel (Rate_XYZ1 & Rate_XYZ2).
 *
 * @return Equivalent sensitivity for Rate channels.
 */
uint32_t SCH1_convertBitfieldToRateSens(uint32_t bitfield)
{
    switch (bitfield)
    {
        case 0x02:          // 010
            return 1600;
        case 0x03:          // 011
            return 3200;
        case 0x04:          // 100
            return 6400;
        default:
            return 0x00;
    }
}


/**
 * @brief Returns bitfield for setting output channel acc sensitivities to desired value.
 *
 * @note Valid sensitivity values for all acc channels are: 3200, 6400, 12800, 25600 [LSB/m/s2]
 *
 * @param Sens - Sensitivity for a rate output channel (Rate_XYZ1 & Rate_XYZ2).
 *
 * @return Equivalent bit field for building the rate sensitivity setting SPI-frame.
 */
uint32_t SCH1_convertAccSensToBitfield(uint32_t Sens)
{
    switch (Sens)
    {
        case 3200:
            return 0x01;   // 001
        case 6400:
            return 0x02;   // 010
        case 12800:
            return 0x03;   // 011
        case 25600:
            return 0x04;   // 100
        default:
            return 0x00;
    }
}


/**
 * @brief Return acc sensitivity that equals to ACC12/3_CTRL-register bitfield.
 *
 * @param bitfield - ACC12/3_CTRL-register bitfield for an acc output channel (Acc_XYZ1/2 & Acc_XYZ3).
 *
 * @return Equivalent sensitivity for Acc channels.
 */
uint32_t SCH1_convertBitfieldToAccSens(uint32_t bitfield)
{
    switch (bitfield)
    {
        case 0x01:          // 001
            return 3200;
        case 0x02:          // 010
            return 6400;
        case 0x03:          // 011
            return 12800;
        case 0x04:          // 100
            return 25600;
        default:
            return 0x00;
    }
}


/**
 * @brief Returns bitfield for setting output channel rate decimation to desired value.
 *
 * @note Valid decimation values for all rate channels are: 2, 4, 8, 16, 32
 *
 * @param Decimation - Decimation for output channel.
 *
 * @return Equivalent bit field for building the decimation setting SPI-frame.
 */
uint32_t SCH1_convertDecimationToBitfield(uint32_t Decimation)
{
    switch (Decimation)
    {
        case 2:
            return 0x00;   // 001
        case 4:
            return 0x01;   // 010
        case 8:
            return 0x02;   // 011
        case 16:
            return 0x03;   // 100
        case 32:
            return 0x04;   // 100
        default:
            return 0x00;
    }
}


/**
 * @brief Return decimation that equals to RATE_CTRL-register bitfield.
 *
 * @param bitfield - RATE_CTRL-register bitfield for decimation.
 *
 * @return Equivalent decimation value
 */
uint32_t SCH1_convertBitfieldToDecimation(uint32_t bitfield)
{
    switch (bitfield)
    {
        case 0x00:      // 001
            return 2;
        case 0x01:      // 010
            return 4;
        case 0x02:      // 011
            return 8;
        case 0x03:      // 100
            return 16;
        case 0x04:      // 100
            return 32;
        default:
            return 0x00;
    }
}


/**
 * @brief Read status register values.
 *
.* @param Status - reference to SCH1 status register structure.
 *
 * @return SCH1_MT_OK = success, SCH1_MT_ERR_* = failure. Please see header file for error definitions.
 */
int SCH1_getStatus(SCH1_status *Status)
{
    if (Status == NULL) {
        return SCH1_ERR_NULL_POINTER;
    }

    SCH1_sendRequest(REQ_READ_STAT_SUM);
    Status->Summary     = SPI48_DATA_UINT16(SCH1_sendRequest(REQ_READ_STAT_SUM_SAT));
    Status->Summary_Sat = SPI48_DATA_UINT16(SCH1_sendRequest(REQ_READ_STAT_COM));
    Status->Common      = SPI48_DATA_UINT16(SCH1_sendRequest(REQ_READ_STAT_RATE_COM));
    Status->Rate_Common = SPI48_DATA_UINT16(SCH1_sendRequest(REQ_READ_STAT_RATE_X));
    Status->Rate_X      = SPI48_DATA_UINT16(SCH1_sendRequest(REQ_READ_STAT_RATE_Y));
    Status->Rate_Y      = SPI48_DATA_UINT16(SCH1_sendRequest(REQ_READ_STAT_RATE_Z));
    Status->Rate_Z      = SPI48_DATA_UINT16(SCH1_sendRequest(REQ_READ_STAT_ACC_X));
    Status->Acc_X       = SPI48_DATA_UINT16(SCH1_sendRequest(REQ_READ_STAT_ACC_Y));
    Status->Acc_Y       = SPI48_DATA_UINT16(SCH1_sendRequest(REQ_READ_STAT_ACC_Z));
    Status->Acc_Z       = SPI48_DATA_UINT16(SCH1_sendRequest(REQ_READ_STAT_ACC_Z));

    return SCH1_OK;
}


/**
 * @brief Verify if all status registers show OK condition.
 *
.* @param Status - reference to SCH1 status register structure.
 *
 * @return true = no status failures
 *         false = at least one status failure.
 */
bool SCH1_verifyStatus(SCH1_status *Status)
{
    if (Status == NULL) {
        return SCH1_ERR_NULL_POINTER;
    }

    if (Status->Summary != 0xffff)
        return false;
    if (Status->Summary_Sat != 0xffff)
        return false;
    if (Status->Common != 0xffff)
        return false;
    if (Status->Rate_Common != 0xffff)
        return false;
    if (Status->Rate_X != 0xffff)
        return false;
    if (Status->Rate_Y != 0xffff)
        return false;
    if (Status->Rate_Z != 0xffff)
        return false;
    if (Status->Acc_X != 0xffff)
        return false;
    if (Status->Acc_Y != 0xffff)
        return false;
    if (Status->Acc_Z != 0xffff)
        return false;

    return true;
}


/**
 * @brief Read serial number of the SCH1.
 *
 * @param None
 *
 * @return Serial number string
 */
char* SCH1_getSnbr(void)
{
    uint16_t sn_id1;
    uint16_t sn_id2;
    uint16_t sn_id3;
    static char strBuffer[15];

    SCH1_sendRequest(REQ_READ_SN_ID1);
    sn_id1 = SPI48_DATA_UINT16(SCH1_sendRequest(REQ_READ_SN_ID2));
    sn_id2 = SPI48_DATA_UINT16(SCH1_sendRequest(REQ_READ_SN_ID3));
    sn_id3 = SPI48_DATA_UINT16(SCH1_sendRequest(REQ_READ_SN_ID3));

    // Build serial number string
    snprintf(strBuffer, 14, "%05d%01X%04X", sn_id2, sn_id1 & 0x000F, sn_id3);

    return strBuffer;
}


/**
 * @brief Send SPI request to SCH1.
 *
 * @param  Request - 48-bit MOSI data
 *
 * @return 48-bit received MISO line data.
 */
uint64_t SCH1_sendRequest(uint64_t Request)
{
    return hw_SPI48_Send_Request(Request);
}


/**
 * @brief Calculate CRC8 for the 48-bit SPI-frame.
 *
 * @param SPIframe - 48-bit SPI-frame for which the CRC is calculated.
 *
 * @return CRC for the given SPI-frame.
 */
uint8_t CRC8(uint64_t SPIframe)
{
    uint64_t data = SPIframe & 0xFFFFFFFFFF00LL;
    uint8_t crc = 0xFF;

    for (int i = 47; i >= 0; i--)
    {
        uint8_t data_bit = (data >> i) & 0x01;
        crc = crc & 0x80 ? (uint8_t)((crc << 1) ^ 0x2F) ^ data_bit : (uint8_t)(crc << 1) | data_bit;
    }

    return crc;
}


/**
 * @brief Check if the CRC8 is correct for the given SPI frame.
 *
 * @param SPIframe - 48 bit SPI frame
 *
 * @return true = ok
 *         false = error
 */
bool SCH1_checkCRC8(uint64_t SPIframe)
{
    if((uint8_t)(SPIframe & 0xff) == CRC8(SPIframe))
        return true;
    else
        return false;
}


/**
 * @brief Calculate CRC3 for the 32-bit SPI-frame.
 *
 * @param SPIframe - 32-bit SPI-frame for which the CRC is calculated.
 *
 * @return CRC for the given SPI-frame.
 */
uint8_t CRC3(uint32_t SPIframe)
{
    uint32_t data = SPIframe & 0xFFFFFFF8;
    uint8_t crc = 0x05;

    for (int i = 31; i >= 0; i--)
    {
        uint8_t data_bit = (data >> i) & 0x01;
        crc = crc & 0x4 ? (uint8_t)((crc << 1) ^ 0x3) ^ data_bit : (uint8_t)(crc << 1) | data_bit;
        crc &= 0x07;
    }

    return crc;
}


/**
 * @brief Check if the CRC3 is correct for the given SPI frame.
 *
 * @param SPIframe - 32 bit SPI frame
 *
 * @return true = ok
 *         false = error
 */
bool SCH1_checkCRC3(uint32_t SPIframe)
{
    if((uint8_t)(SPIframe & 0x07) == CRC3(SPIframe))
        return true;
    else
        return false;
}


/**
 * @brief Initialize the SCH1 sensor.
 *
 * @param sFilter - structure containing filter settings for all channels.
 *        sSensitivity - structure containing sensitivity settings for all channels.
 *        sDecimation - structure containing decimation settings for decimated channels.
 *        enableDRY - true = enable DRY interrupt, false = disable DRY interrupt.
 *
 * @return SCH1_OK = success
 *         SCH1_ERR_* = failure. Please see header file for error definitions.
 */
int SCH1_init(SCH1_filter sFilter, SCH1_sensitivity sSensitivity, SCH1_decimation sDecimation, bool enableDRY)
{

    int ret = SCH1_OK;
    uint8_t startup_attempt = 0;
    bool SCH1status = false;
    SCH1_status SCH1statusAll;

    // SCH1 startup sequence specified in section "5 Component Operation,
    // Reset and Power Up" in the data sheet.

    SCH1_reset(); // Reset sensor

    for (startup_attempt = 0; startup_attempt < 2; startup_attempt++) {

        // Wait 32 ms for the non-volatile memory (NVM) Read
        hw_delay(32);

        // Set user controls
        SCH1_setFilters(sFilter.Rate12, sFilter.Acc12, sFilter.Acc3);
        SCH1_setRateSensDec(sSensitivity.Rate1, sSensitivity.Rate2, sDecimation.Rate2);
        SCH1_setAccSensDec(sSensitivity.Acc1, sSensitivity.Acc2, sSensitivity.Acc3, sDecimation.Acc2);
        if (enableDRY)
            SCH1_setDRY(0, true);   // 0 = DRY active high
        else
            SCH1_setDRY(0, false);

        // Write EN_SENSOR = 1
        SCH1_enableMeas(true, false);

        // Wait 215 ms
        hw_delay(215);

        // Read all status registers once. No critization
        SCH1_getStatus(&SCH1statusAll);

        // Write EOI = 1 (End of Initialization command)
        SCH1_enableMeas(true, true);

        // Wait 3 ms
        hw_delay(3);

        // Read all status registers twice.
        SCH1_getStatus(&SCH1statusAll);
        SCH1_getStatus(&SCH1statusAll);

        // Read all user control registers and verify content - Add verification here if needed for FuSa.

        // Check that all status registers have OK status.
        if (!SCH1_verifyStatus(&SCH1statusAll)) {
            SCH1status = false;
            SCH1_reset();    // Sensor failed, reset and retry.
        }
        else {
            SCH1status = true;
            break;
        }

    } // for (startup_attempt = 0; startup_attempt < 2; startup_attempt++)

    if (SCH1status != true)
        ret = SCH1_ERR_SENSOR_INIT;

    return ret;
}


/**
 * @brief Read rate, acceleration and temperature data from sensor. Called by sampling_callback()
 *
 * @param data - pointer to "raw" data from sensor
 *
 * @return None
 */
void SCH1_getData(SCH1_raw_data *data)
{
    SCH1_sendRequest(REQ_READ_RATE_X1);
    uint64_t rate_x_raw = SCH1_sendRequest(REQ_READ_RATE_Y1);
    uint64_t rate_y_raw = SCH1_sendRequest(REQ_READ_RATE_Z1);
    uint64_t rate_z_raw = SCH1_sendRequest(REQ_READ_ACC_X1);
    uint64_t acc_x_raw  = SCH1_sendRequest(REQ_READ_ACC_Y1);
    uint64_t acc_y_raw  = SCH1_sendRequest(REQ_READ_ACC_Z1);
    uint64_t acc_z_raw  = SCH1_sendRequest(REQ_READ_TEMP);
    uint64_t temp_raw   = SCH1_sendRequest(REQ_READ_TEMP);

    // Get possible frame errors
    uint64_t miso_words[] = {rate_x_raw, rate_y_raw, rate_z_raw, acc_x_raw, acc_y_raw, acc_z_raw, temp_raw};
    data->frame_error = SCH1_check_48bit_frame_error(miso_words, (sizeof(miso_words) / sizeof(uint64_t)));

    // Parse MISO data to structure
    data->Rate1_raw[AXIS_X] = SPI48_DATA_INT32(rate_x_raw);
    data->Rate1_raw[AXIS_Y] = SPI48_DATA_INT32(rate_y_raw);
    data->Rate1_raw[AXIS_Z] = SPI48_DATA_INT32(rate_z_raw);
    data->Acc1_raw[AXIS_X]  = SPI48_DATA_INT32(acc_x_raw);
    data->Acc1_raw[AXIS_Y]  = SPI48_DATA_INT32(acc_y_raw);
    data->Acc1_raw[AXIS_Z]  = SPI48_DATA_INT32(acc_z_raw);

    // Temperature data is always 16 bits wide. Drop 4 LSBs as they are not used.
    data->Temp_raw = SPI48_DATA_INT32(temp_raw) >> 4;

}


/**
 * @brief Convert summed raw data from sensor to real values. Also calculate averages values.
 *
 * @param data_in - pointer to summed raw data from sensor
 *        data_out - pointer to converted values
 *
 * @return None
 */
void SCH1_convert_data(SCH1_raw_data *data_in, SCH1_result *data_out)
{
    // Convert from raw counts to sensitivity and calculate averages here for faster execution
    data_out->Rate1[AXIS_X] = (float)data_in->Rate1_raw[AXIS_X] / (SENSITIVITY_RATE1 * (float)AVG_FACTOR);
    data_out->Rate1[AXIS_Y] = (float)data_in->Rate1_raw[AXIS_Y] / (SENSITIVITY_RATE1 * (float)AVG_FACTOR);
    data_out->Rate1[AXIS_Z] = (float)data_in->Rate1_raw[AXIS_Z] / (SENSITIVITY_RATE1 * (float)AVG_FACTOR);
    data_out->Acc1[AXIS_X]  = (float)data_in->Acc1_raw[AXIS_X] / (SENSITIVITY_ACC1 * (float)AVG_FACTOR);
    data_out->Acc1[AXIS_Y]  = (float)data_in->Acc1_raw[AXIS_Y] / (SENSITIVITY_ACC1 * (float)AVG_FACTOR);
    data_out->Acc1[AXIS_Z]  = (float)data_in->Acc1_raw[AXIS_Z] / (SENSITIVITY_ACC1 * (float)AVG_FACTOR);

    // Convert temperature and calculate average
    data_out->Temp = GET_TEMPERATURE((float)data_in->Temp_raw / (float)AVG_FACTOR);

}


/**
 * @brief Check if 48-bit MISO frames have any error bits set. Return true on the first error encountered.
 *
 * @param data - pointer to 48-bit MISO frames from sensor
 *        size - number of frames to check
 *
 * @return true = any error bit set
 *         false = no error
 */
bool SCH1_check_48bit_frame_error(uint64_t *data, int size)
{
    for (int i = 0; i < size; i++)
    {
        uint64_t value = data[i];
        if (value & ERROR_FIELD_MASK)
            return true;
    }

    return false;
}
