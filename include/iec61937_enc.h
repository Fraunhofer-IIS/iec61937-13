/*-----------------------------------------------------------------------------
Software License for The Fraunhofer FDK MPEG-H Software

Copyright (c) 2018 - 2023 Fraunhofer-Gesellschaft zur Förderung der angewandten
Forschung e.V. and Contributors
All rights reserved.

1. INTRODUCTION

The "Fraunhofer FDK MPEG-H Software" is software that implements the ISO/MPEG
MPEG-H 3D Audio standard for digital audio or related system features. Patent
licenses for necessary patent claims for the Fraunhofer FDK MPEG-H Software
(including those of Fraunhofer), for the use in commercial products and
services, may be obtained from the respective patent owners individually and/or
from Via LA (www.via-la.com).

Fraunhofer supports the development of MPEG-H products and services by offering
additional software, documentation, and technical advice. In addition, it
operates the MPEG-H Trademark Program to ease interoperability testing of end-
products. Please visit www.mpegh.com for more information.

2. COPYRIGHT LICENSE

Redistribution and use in source and binary forms, with or without modification,
are permitted without payment of copyright license fees provided that you
satisfy the following conditions:

* You must retain the complete text of this software license in redistributions
of the Fraunhofer FDK MPEG-H Software or your modifications thereto in source
code form.

* You must retain the complete text of this software license in the
documentation and/or other materials provided with redistributions of
the Fraunhofer FDK MPEG-H Software or your modifications thereto in binary form.
You must make available free of charge copies of the complete source code of
the Fraunhofer FDK MPEG-H Software and your modifications thereto to recipients
of copies in binary form.

* The name of Fraunhofer may not be used to endorse or promote products derived
from the Fraunhofer FDK MPEG-H Software without prior written permission.

* You may not charge copyright license fees for anyone to use, copy or
distribute the Fraunhofer FDK MPEG-H Software or your modifications thereto.

* Your modified versions of the Fraunhofer FDK MPEG-H Software must carry
prominent notices stating that you changed the software and the date of any
change. For modified versions of the Fraunhofer FDK MPEG-H Software, the term
"Fraunhofer FDK MPEG-H Software" must be replaced by the term "Third-Party
Modified Version of the Fraunhofer FDK MPEG-H Software".

3. No PATENT LICENSE

NO EXPRESS OR IMPLIED LICENSES TO ANY PATENT CLAIMS, including without
limitation the patents of Fraunhofer, ARE GRANTED BY THIS SOFTWARE LICENSE.
Fraunhofer provides no warranty of patent non-infringement with respect to this
software. You may use this Fraunhofer FDK MPEG-H Software or modifications
thereto only for purposes that are authorized by appropriate patent licenses.

4. DISCLAIMER

This Fraunhofer FDK MPEG-H Software is provided by Fraunhofer on behalf of the
copyright holders and contributors "AS IS" and WITHOUT ANY EXPRESS OR IMPLIED
WARRANTIES, including but not limited to the implied warranties of
merchantability and fitness for a particular purpose. IN NO EVENT SHALL THE
COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE for any direct, indirect,
incidental, special, exemplary, or consequential damages, including but not
limited to procurement of substitute goods or services; loss of use, data, or
profits, or business interruption, however caused and on any theory of
liability, whether in contract, strict liability, or tort (including
negligence), arising in any way out of the use of this software, even if
advised of the possibility of such damage.

5. CONTACT INFORMATION

Fraunhofer Institute for Integrated Circuits IIS
Attention: Division Audio and Media Technologies - MPEG-H FDK
Am Wolfsmantel 33
91058 Erlangen, Germany
www.iis.fraunhofer.de/amm
amm-info@iis.fraunhofer.de
-----------------------------------------------------------------------------*/

#include <stdint.h>
#include <stdbool.h>

#if !defined(IEC61937_ENC_H)
#define IEC61937_ENC_H

/**
 * @file   iec61937_enc.h
 * @brief  IEC61937-13 encoder library interface header file.
 */

#ifdef __cplusplus
extern "C" {
#endif

#define IEC61937_AUDIOFRAME_LENGTH 1024
#define IEC61937_MAX_SAMPLERATE_FACTOR 16
#define IEC60958_FRAME_SIZE_BYTES 4

#define MAX_IEC61937_FRAME_SIZE_BYTES \
  (IEC61937_AUDIOFRAME_LENGTH) * (IEC61937_MAX_SAMPLERATE_FACTOR) * (IEC60958_FRAME_SIZE_BYTES)

typedef enum IECENC_RESULT {
  IECENC_OK = 0,         /*!< Ok, no error */
  IECENC_BUFFER_ERROR,   /*!< Working buffer full or output buffer size too small */
  IECENC_NULLPTR_ERROR,  /*!< A nullptr was used */
  IECENC_DURATION_ERROR, /*!< The provided frame duration exceeds the maximum allowed duration */
} IECENC_RESULT;

/* IEC61937-13 encoder state structure */
typedef struct iec61937_encoder_state* HANDLE_IEC61937_ENCODER;

/**
 * @brief Encode one IEC61937-13 MPEG-H frame.
 * @param[in] h encoder handle
 * @param[in] inputBuffer pointer to data buffer where one MPEG-H frame is read from
 * @param[in] inputBufferLength size in bytes of the data in inputBuffer
 * @param[out] fInputBufferProcessed flag set to true if data from inputBuffer was read or false if
 * it had to be postponed, i.e. the inputBuffer needs to be passed in again
 * @param[in] duration the amount of audio samples according to PTS difference of consecutive MPEG-H
 * frames
 * @param[out] outputBuffer pointer to an output data buffer into which one resulting IEC61937-13
 * frame will be written
 * @param[in,out] pOutputBufferLength pointer to the capacity of the outputBuffer on input and the
 * number of bytes written into outputBuffer on output
 * @returns IECENC_OK in case of success, IECDEC_BUFFER_ERROR in case the internal buffer is full or
 * the output buffer size is too small and IECENC_NULLPTR_ERROR if a nullptr was used as an input
 * argument.
 */
IECENC_RESULT iec61937_encode_process(HANDLE_IEC61937_ENCODER h, const uint8_t* inputBuffer,
                                      uint32_t inputBufferLength, bool* fInputBufferProcessed,
                                      uint32_t duration, uint8_t* outputBuffer,
                                      uint32_t* pOutputBufferLength);

/**
 * @brief Create a IEC61937-13 encoder instance.
 * @param[in] rateFactor bit rate factor for IEC frame rate. The rate factors are defined in
 * specification IEC 61937-13 subclause 5.3.2. Supported rate factors are 4 and 16.
 * @return HANDLE_IEC61937_ENCODER in case of success, NULL in case of error.
 */
HANDLE_IEC61937_ENCODER iec61937_encode_open(uint8_t rateFactor);

/**
 * @brief Close a IEC61937-13 encoder instance.
 * @param[in] h encoder handle to be closed
 */
void iec61937_encode_close(HANDLE_IEC61937_ENCODER h);

#ifdef __cplusplus
}
#endif

#endif /* !defined(IEC61937_ENC_H) */
