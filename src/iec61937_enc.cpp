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

#include "iec61937_enc.h"
#include "iec61937_common.h"

#include <stdlib.h>
#include <string.h>

#define MAX_NUM_MPEGH_FRAMES 5
// Buffer size in bytes to hold one MPEG-H frame (sequence of MHAS packages) + overhead
// for MPEG-H Level 4
#define MAX_MPEGH_FRAME_SIZE 65536
#define MAX_MPEGH_FRAME_DURATION 4096
#define WORKBUFFER_SIZE_BYTES (MAX_MPEGH_FRAME_SIZE) * (MAX_NUM_MPEGH_FRAMES)

struct iec61937_encoder_state {
  uint8_t rateFactor;
  uint8_t audioMode;
  uint32_t burstRepetitionPeriod;
  uint8_t payloadHeaderSize;
  int32_t audioFrameLength;

  int32_t pcmOffset;
  int32_t overallDuration;

  uint8_t workBuffer[WORKBUFFER_SIZE_BYTES];
  uint8_t* pWorkBufferWrite;
  uint8_t* pWorkBufferRead;

  uint32_t framesStoredCount;
  uint32_t frameLength[MAX_NUM_MPEGH_FRAMES];
  uint32_t frameDuration[MAX_NUM_MPEGH_FRAMES];
  bool auPending;
} iec61937_encoder_state;

static void resetBufferState(HANDLE_IEC61937_ENCODER h) {
  h->pWorkBufferRead = h->workBuffer;
  h->pWorkBufferWrite = h->workBuffer;

  h->framesStoredCount = 0;
  for (uint32_t i = 0; i < MAX_NUM_MPEGH_FRAMES; i++) {
    h->frameLength[i] = 0;
    h->frameDuration[i] = 0;
  }
  h->auPending = false;
}

HANDLE_IEC61937_ENCODER iec61937_encode_open(uint8_t rateFactor) {
  HANDLE_IEC61937_ENCODER h;

  h = (HANDLE_IEC61937_ENCODER)calloc(1, sizeof(iec61937_encoder_state));
  if (h == NULL) {
    return NULL;
  }

  h->pcmOffset = 0;
  h->overallDuration = 0;

  resetBufferState(h);

  // set rate factor and corresponding audio mode
  // 0 = MPEG-H 3D Audio
  // 1 = MPEG-H 3D Audio HBR
  switch (rateFactor) {
    case 4:
      h->audioMode = 1;
      h->rateFactor = 1;
      break;
    case 16:
      h->audioMode = 1;
      h->rateFactor = 3;
      break;
    default:
      free(h);
      return NULL;
  }

  // determine the size of a payload header
  h->payloadHeaderSize = (h->audioMode == 0) ? 6 : 8;

  // determine the burst repetition period
  h->audioFrameLength = IEC61937_AUDIOFRAME_LENGTH;
  h->burstRepetitionPeriod = h->audioFrameLength * IEC60958_FRAME_SIZE_BYTES;
  if (h->audioMode == 1) {
    h->burstRepetitionPeriod = h->burstRepetitionPeriod << (h->rateFactor + 1);
  }

  return h;
}

void iec61937_encode_close(HANDLE_IEC61937_ENCODER h) {
  if (h == NULL) {
    return;
  }
  free(h);
}

static uint32_t getNumBuffersToWrite(HANDLE_IEC61937_ENCODER h) {
  uint32_t i = 0;
  uint32_t availableBytes = h->burstRepetitionPeriod;
  availableBytes -= (IEC_HEADER_SIZE_BYTES + IEC_BURST_SPACING_SIZE_BYTES);
  if (!h->auPending) {
    availableBytes -= h->payloadHeaderSize;
  }

  int32_t duration = 0;
  uint32_t writeLength = 0;
  while ((writeLength < availableBytes) && (duration <= h->overallDuration) &&
         (i != h->framesStoredCount)) {
    writeLength += h->frameLength[i] + h->payloadHeaderSize;
    duration += h->frameDuration[i];
    i++;
  }
  return i;
}

// IEC frame writer. Introduces headers, trailers and includes the payload data.
static uint32_t writeIecFrame(HANDLE_IEC61937_ENCODER h, uint8_t* outputBuffer,
                              uint32_t payloadLength, uint32_t numAvailableBytes,
                              uint32_t numBuffersToWrite) {
  // write frame header
  *outputBuffer++ = SYNC_PREAMBLE_0;             // Pa
  *outputBuffer++ = SYNC_PREAMBLE_1;             // Pa
  *outputBuffer++ = SYNC_PREAMBLE_2;             // Pb
  *outputBuffer++ = SYNC_PREAMBLE_3;             // Pb
  *outputBuffer++ = (h->rateFactor << 3);        // bits 11 - 12 of Pc
  *outputBuffer++ = (h->audioMode << 5) | (25);  // bits  5 -  6 of Pc

  uint32_t numPayloadHeaders = numBuffersToWrite;
  if (h->auPending) {
    numPayloadHeaders--;
  }

  uint32_t dataBurstLengthBytes =
      numAvailableBytes + (numPayloadHeaders + 1) * h->payloadHeaderSize;
  uint32_t payloadDataLength = numAvailableBytes;
  if (payloadLength < numAvailableBytes) {
    dataBurstLengthBytes = payloadLength + (numPayloadHeaders + 1) * h->payloadHeaderSize;
    payloadDataLength = payloadLength;
  }

  uint32_t dataBurstLength = dataBurstLengthBytes;
  if (h->audioMode == 1) {
    // divided by 8
    dataBurstLength = (dataBurstLengthBytes + 7) >> 3;
  }

  *outputBuffer++ = (uint8_t)(dataBurstLength >> 8);  // Pd
  *outputBuffer++ = (uint8_t)dataBurstLength;         // Pd

  // write payload headers
  uint32_t dataOffset = 8;
  dataOffset += (numPayloadHeaders + 1) * h->payloadHeaderSize;

  uint32_t i = 0;
  if (h->auPending) {
    dataOffset += h->frameLength[i];
    i++;
  }

  for (uint32_t j = 0; j < numPayloadHeaders; j++) {
    // write data offset field
    if (h->audioMode == 1) {
      *outputBuffer++ = (uint8_t)(dataOffset >> 16);
    }
    *outputBuffer++ = (uint8_t)(dataOffset >> 8);
    *outputBuffer++ = (uint8_t)dataOffset;
    // write data size field
    if (h->audioMode == 1) {
      *outputBuffer++ = (uint8_t)(h->frameLength[i] >> 16);
    }
    *outputBuffer++ = (uint8_t)((h->frameLength[i]) >> 8);
    *outputBuffer++ = (uint8_t)h->frameLength[i];
    // write PCM offset field
    *outputBuffer++ = (uint8_t)(h->pcmOffset >> 8);
    *outputBuffer++ = (uint8_t)(h->pcmOffset);

    h->pcmOffset += h->frameDuration[i];
    dataOffset += h->frameLength[i];
    i++;
  }

  uint32_t j;
  // write last payload header with zeroes as list terminator
  for (j = 0; j < h->payloadHeaderSize; j++) {
    *outputBuffer++ = 0;
  }

  // write payload data
  for (j = 0; j < payloadDataLength; j++) {
    *outputBuffer++ = *h->pWorkBufferRead++;
  }

  // write padding
  for (j = payloadLength; j < numAvailableBytes; j++) {
    *outputBuffer++ = 0;
  }

  // write burst spacing
  for (j = 0; j < IEC_BURST_SPACING_SIZE_BYTES; j++) {
    *outputBuffer++ = 0;
  }

  return h->burstRepetitionPeriod;
}

IECENC_RESULT iec61937_encode_process(HANDLE_IEC61937_ENCODER h, const uint8_t* inputBuffer,
                                      uint32_t inputBufferLength, bool* fInputBufferProcessed,
                                      uint32_t duration, uint8_t* outputBuffer,
                                      uint32_t* pOutputBufferLength) {
  if (h == NULL || inputBuffer == NULL || fInputBufferProcessed == NULL || outputBuffer == NULL ||
      pOutputBufferLength == NULL) {
    return IECENC_NULLPTR_ERROR;
  }
  if (*pOutputBufferLength < h->burstRepetitionPeriod) {
    return IECENC_BUFFER_ERROR;
  }
  if (duration > MAX_MPEGH_FRAME_DURATION) {
    return IECENC_DURATION_ERROR;
  }
  *pOutputBufferLength = 0;

  // Process accumulated data first
  *fInputBufferProcessed = false;
  if (h->overallDuration >= h->audioFrameLength) {
    inputBufferLength = 0;
  }

  uint32_t numBuffersToWrite = 0;
  bool numBuffersToWriteObtained = false;

  // Accumulate new data
  if (inputBufferLength != 0) {
    if (h->framesStoredCount + 1 >= MAX_NUM_MPEGH_FRAMES) {
      return IECENC_BUFFER_ERROR;
    }
    if (h->pWorkBufferWrite + inputBufferLength > h->workBuffer + sizeof(h->workBuffer)) {
      return IECENC_BUFFER_ERROR;
    }

    *fInputBufferProcessed = true;
    h->overallDuration += duration;

    memcpy(h->pWorkBufferWrite, inputBuffer, inputBufferLength);
    h->pWorkBufferWrite += inputBufferLength;
    h->frameLength[h->framesStoredCount] = inputBufferLength;
    h->frameDuration[h->framesStoredCount] = duration;
    h->framesStoredCount++;

    // determine how many stored frames can be written to the IEC frame
    numBuffersToWrite = getNumBuffersToWrite(h);
    numBuffersToWriteObtained = true;

    // check if there is enough data available to be written to the IEC frame
    if ((h->overallDuration < h->audioFrameLength) || numBuffersToWrite == 0) {
      return IECENC_OK;
    }
  }

  if (!numBuffersToWriteObtained) {
    // determine how many stored frames can be written to the IEC frame
    numBuffersToWrite = getNumBuffersToWrite(h);
  }

  // calculate the number of bytes available in the work buffer
  uint32_t currentWorkbufferBytes = 0;
  for (uint32_t i = 0; i < h->framesStoredCount; i++) {
    currentWorkbufferBytes += h->frameLength[i];
  }

  // calculate the number of bytes available for the payload data in the IEC frame to be written
  uint32_t numAvailableBytes = h->burstRepetitionPeriod;
  numAvailableBytes -= (IEC_HEADER_SIZE_BYTES + IEC_BURST_SPACING_SIZE_BYTES);
  if (!h->auPending) {
    numAvailableBytes -= h->payloadHeaderSize;
  }

  uint32_t payloadDataLength = 0;
  for (uint32_t i = 0; i < numBuffersToWrite; i++) {
    payloadDataLength += h->frameLength[i];
    numAvailableBytes -= h->payloadHeaderSize;
  }

  // write an IEC61937-13 frame
  uint32_t lengthWritten =
      writeIecFrame(h, outputBuffer, payloadDataLength, numAvailableBytes, numBuffersToWrite);
  *pOutputBufferLength += lengthWritten;
  outputBuffer += lengthWritten;
  h->overallDuration -= h->audioFrameLength;
  h->pcmOffset -= h->audioFrameLength;

  // adjust the written data
  uint32_t buffersToDelete = 0;
  for (uint32_t i = 0; i < numBuffersToWrite; i++) {
    if (i == numBuffersToWrite - 1 && payloadDataLength > numAvailableBytes) {
      h->auPending = true;
      h->frameLength[i] = payloadDataLength - numAvailableBytes;
      h->frameDuration[i] = 0;
    } else {
      h->auPending = false;
      h->frameLength[i] = 0;
      h->frameDuration[i] = 0;
      buffersToDelete++;
    }
  }

  // remove/adjust processed frame info
  if (buffersToDelete > 0) {
    h->framesStoredCount -= buffersToDelete;
    memmove(&h->frameLength[0], &h->frameLength[buffersToDelete],
            h->framesStoredCount * sizeof(uint32_t));
    memmove(&h->frameDuration[0], &h->frameDuration[buffersToDelete],
            h->framesStoredCount * sizeof(uint32_t));
  }
  uint32_t payloadDataToDelete = payloadDataLength;
  if (payloadDataLength > numAvailableBytes) {
    payloadDataToDelete = numAvailableBytes;
  }
  uint32_t payloadDataToKeep = currentWorkbufferBytes - payloadDataToDelete;
  if (payloadDataToKeep > 0) {
    memmove(&h->workBuffer[0], h->pWorkBufferRead, payloadDataToKeep * sizeof(uint8_t));
  }
  h->pWorkBufferRead = h->workBuffer;
  h->pWorkBufferWrite = &h->workBuffer[payloadDataToKeep];

  return IECENC_OK;
}
