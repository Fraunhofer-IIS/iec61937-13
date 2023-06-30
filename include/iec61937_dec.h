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

#if !defined(IEC61937_DEC_H)
#define IEC61937_DEC_H

/**
 * @file   iec61937_dec.h
 * @brief  IEC61937-13 decoder library interface header file.
 */

#ifdef __cplusplus
extern "C" {
#endif

// Buffer size in bytes to hold one MPEG-H frame (sequence of MHAS packages) + overhead
// for MPEG-H Level 4
#define MAX_MPEGH_FRAME_SIZE 65536

#define MAX_AUDIOFRAME_LENGTH 4096
#define IEC61937_MAX_SAMPLERATE_FACTOR 16
#define IEC60958_FRAME_SIZE_BYTES 4

#define MAX_IEC61937_FRAME_SIZE_BYTES \
  (MAX_AUDIOFRAME_LENGTH) * (IEC61937_MAX_SAMPLERATE_FACTOR) * (IEC60958_FRAME_SIZE_BYTES)

#define WORKBUFFER_SIZE_BYTES (MAX_IEC61937_FRAME_SIZE_BYTES) * 3

typedef enum IECDEC_RESULT {
  IECDEC_OK = 0,            /*!< Ok, no error */
  IECDEC_FEED_MORE_DATA,    /*!< Ok, but more input data needs to be fed */
  IECDEC_PENDINGDATA_ERROR, /*!< The pending data could not be completed (e.g. data offset mismatch)
                                 or the available data exceeds the pending data limit */
  IECDEC_BUFFER_ERROR,      /*!< Working buffer full or output buffer size too small */
  IECDEC_NULLPTR_ERROR,     /*!< A nullptr was used */
} IECDEC_RESULT;

/* IEC61937-13 decoder state structure */
typedef struct iec61937_decoder_state* HANDLE_IEC61937_DECODER;

/**
 * @brief Open an IEC61937-13 decoder instance.
 * @return HANDLE_IEC61937_DECODER on success or NULL in case of error
 */
HANDLE_IEC61937_DECODER iec61937_decode_open(void);

/**
 * @brief Close an IEC61937-13 decoder instance.
 * @param[in] h decoder handle to be closed.
 */
void iec61937_decode_close(HANDLE_IEC61937_DECODER h);

/**
 * @brief Feed IEC frames/data chunks to the IEC61937-13 decoder.
 * @param[in] h decoder handle
 * @param[in] inputBuffer pointer to a data buffer to read the input data from
 * @param[in] inputBufferLength length in bytes of the provided input data
 * @returns IECDEC_OK in case of success, IECDEC_BUFFER_ERROR if the size of the input data is too
 * big to fit into the internal working buffer and IECDEC_NULLPTR_ERROR if a nullptr was used as an
 * input argument
 */
IECDEC_RESULT iec61937_decode_feed(HANDLE_IEC61937_DECODER h, const uint8_t* inputBuffer,
                                   uint32_t inputBufferLength);

/**
 * @brief Decode the IEC61937-13 frame and obtain one MPEG-H frame.
 * @param[in] h decoder handle
 * @param[out] outputBuffer pointer to an output data buffer into which the MPEG-H frame is written
 * @param[in,out] pOutputBufferLength pointer to the capacity of the outputBuffer on input and the
 * number of bytes written into outputBuffer on output
 * @param[out] pPcmOffset pointer to where the PCM offset of the MPEG-H frame written to
 * outputBuffer is stored into; can be used to recreate the PTS of the obtained MPEG-H frame
 * @param[out] pIecFrameLength pointer where the frame length of the current IEC frame is stored
 * into; can be used to recreate the PTS of the obtained MPEG-H frame
 * @param[out] pIecFrameProcessed pointer where the info about having completed the processing of
 * the IEC frame is stored into; can be used to recreate the PTS of the obtained MPEG-H frame
 * @return IECDEC_OK on success, IECDEC_FEED_MORE_DATA if new data needs to fed into the decoder,
 * IECDEC_BUFFER_ERROR if the provided output buffer has not enough space to hold the output MPEG-H
 * frame and IECDEC_NULLPTR_ERROR if a nullptr was used as an input argument
 */
IECDEC_RESULT iec61937_decode_process(HANDLE_IEC61937_DECODER h, uint8_t* outputBuffer,
                                      uint32_t* pOutputBufferLength, int32_t* pPcmOffset,
                                      uint32_t* pIecFrameLength, bool* pIecFrameProcessed);

#ifdef __cplusplus
}
#endif

#endif /* !defined(IEC61937_DEC_H) */
