//****************************************************************************************
// @file    hw.h
// @brief   Header file for hardware related functions.
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

#ifndef HW_H
#define HW_H

#include <stdint.h>
#include "config.h"

#ifdef __cplusplus
extern "C" {
#endif

// IO-pin definitions
#define EXTRESN_PORT    GPIOA
#define EXTRESN_PIN     GPIO_PIN_8
#define TA9_PORT        GPIOB
#define TA9_PIN         GPIO_PIN_10
#define TA8_PORT        GPIOC
#define TA8_PIN         GPIO_PIN_7
#define DRY_SYNC_PORT   GPIOA
#define DRY_SYNC_PIN    GPIO_PIN_9

#define SPI1_CS_PORT    GPIOB
#define SPI1_CS_PIN     GPIO_PIN_6
#define SPI1_PORT       GPIOA
#define SPI1_SCK_PIN    GPIO_PIN_5
#define SPI1_MISO_PIN   GPIO_PIN_6
#define SPI1_MOSI_PIN   GPIO_PIN_7

// Function prototypes
void        hw_init(void);
void        hw_EXTRESN_High(void);
void        hw_EXTRESN_Low(void);
void        hw_CS_High(void);
void        hw_CS_Low(void);
void        hw_delay(uint32_t ms);
uint64_t    hw_SPI48_Send_Request(uint64_t Request);
void        hw_timer_setFreq(uint32_t freq);
void        hw_timer_startIT(void);
void        hw_timer_stopIT(void);

#ifdef __cplusplus
}
#endif

#endif
