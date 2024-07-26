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

#include "iec61937_dec.h"
#include "iec61937_common.h"

#include <stdlib.h>
#include <string.h>

struct iec61937_decoder_state {
  uint8_t workBuffer[WORKBUFFER_SIZE_BYTES];
  uint32_t workBufferBytesAvailable;

  // Pending data state
  uint8_t frameBufferPending[MAX_MPEGH_FRAME_SIZE];
  uint32_t frameBytesPending;
  uint32_t frameBytesMissing;
  int32_t pcmOffsetPending; /* PCM offset of pending audio frame */

  // Sync state
  bool syncFound;
  bool syncCandidateFound;
  uint32_t syncCandidateIndex;

  // Parser state
  uint16_t dataType;
  uint16_t audioMode;
  uint16_t rateFactor;
  uint32_t frameLength;
  uint32_t payloadLength;
  uint32_t burstRepetitionPeriod;
  uint32_t payloadHeaderSize;
  uint32_t numPayloadHeaders;
  uint32_t payloadHeaderIndex;
} iec61937_decoder_state;

static void resetSyncState(HANDLE_IEC61937_DECODER h) {
  h->syncCandidateFound = false;
  h->syncCandidateIndex = 0;
  h->syncFound = false;
}

static void resetParsingState(HANDLE_IEC61937_DECODER h) {
  h->dataType = 0;
  h->audioMode = 0;
  h->rateFactor = 0;
  h->frameLength = 0;
  h->payloadLength = 0;
  h->burstRepetitionPeriod = 0;
  h->payloadHeaderSize = 0;
  h->numPayloadHeaders = 0;
  h->payloadHeaderIndex = 0;
}

static void resetPendingState(HANDLE_IEC61937_DECODER h) {
  h->frameBytesPending = 0;
  h->frameBytesMissing = 0;
  h->pcmOffsetPending = 0;
}

static int32_t parseIecFrameData(HANDLE_IEC61937_DECODER h) {
  // Parse Pc, Pd
  uint16_t dataType = h->workBuffer[h->syncCandidateIndex + 5] & 0x1f;
  uint16_t audioMode = (h->workBuffer[h->syncCandidateIndex + 5] >> 5) & 0x3;
  uint16_t frameLengthCode = h->workBuffer[h->syncCandidateIndex + 4] & 0x7;
  uint16_t rateFactor = (h->workBuffer[h->syncCandidateIndex + 4] >> 3) & 0x3;
  uint32_t payloadLength = (uint16_t)(h->workBuffer[h->syncCandidateIndex + 6] << 8) +
                           (uint16_t)h->workBuffer[h->syncCandidateIndex + 7];

  // check data type for MPEG-H 3D Audio
  if (dataType != 25) {
    return 1;
  }

  // check for supported audio mode
  // 0 = MPEG-H 3D Audio
  // 1 = MPEG-H 3D Audio HBR
  if (audioMode > 1) {
    return 1;
  }

  // check the data frame length
  uint32_t frameLength = 0;
  switch (frameLengthCode) {
    case 0:
      frameLength = 1024;
      break;
    case 1:
      frameLength = 2048;
      break;
    case 2:
      frameLength = 4096;
      break;
    case 3:
      frameLength = 768;
      break;
    case 4:
      frameLength = 1536;
      break;
    case 5:
      frameLength = 3072;
      break;
    default:
      break;
  }
  if (frameLength == 0) {
    return 1;
  }

  // determine the burst repetition period
  uint32_t burstRepetitionPeriod = frameLength * IEC60958_FRAME_SIZE_BYTES;
  if (audioMode == 1) {
    burstRepetitionPeriod = burstRepetitionPeriod << (rateFactor + 1);
  }

  // adjust payload length to be in number of bytes
  if (audioMode == 1) {
    payloadLength *= 8;
  }

  // check payload length
  if (payloadLength >
      burstRepetitionPeriod - IEC_HEADER_SIZE_BYTES - IEC_BURST_SPACING_SIZE_BYTES) {
    return 1;
  }

  // determine the size of a payload header
  uint32_t payloadHeaderSize = (audioMode == 0) ? 6 : 8;

  // store the parsed IEC frame header data
  h->dataType = dataType;
  h->audioMode = audioMode;
  h->rateFactor = rateFactor;
  h->frameLength = frameLength;
  h->payloadLength = payloadLength;
  h->burstRepetitionPeriod = burstRepetitionPeriod;
  h->payloadHeaderSize = payloadHeaderSize;

  return 0;
}

static void parsePayloadHeader(HANDLE_IEC61937_DECODER h, uint8_t* data, uint32_t* dataOffset,
                               uint32_t* dataLength, int32_t* pcmOffset) {
  if (h->audioMode == 0) {
    *dataOffset = (data[0] << 8) | data[1];
    *dataLength = (data[2] << 8) | data[3];
    *pcmOffset = (data[4] << 8) | data[5];
  } else {
    *dataOffset = (data[0] << 16) | (data[1] << 8) | data[2];
    *dataLength = (data[3] << 16) | (data[4] << 8) | data[5];
    *pcmOffset = (data[6] << 8) | data[7];
  }
}

static bool checkPayloadHeaders(HANDLE_IEC61937_DECODER h, uint32_t* numPayloadHeaders) {
  // get the number of payload headers and check the offsets
  uint32_t payloadHeadersLength = 0;
  uint32_t payloadStartIndex = h->syncCandidateIndex + IEC_HEADER_SIZE_BYTES;
  uint8_t* headerPointer = &h->workBuffer[payloadStartIndex];
  uint32_t firstPayloadOffset = 0;
  uint32_t previousPayloadOffset = 0;
  while (true) {
    uint32_t dataOffset = 0;
    uint32_t dataLength = 0;
    int32_t pcmOffset = 0;

    // Parse audio burst payload header
    parsePayloadHeader(h, headerPointer, &dataOffset, &dataLength, &pcmOffset);

    if (dataLength > 0) {
      if (*numPayloadHeaders == 0) {
        firstPayloadOffset = dataOffset;
      } else {
        if (dataOffset <= previousPayloadOffset) {
          return false;
        }
      }
      previousPayloadOffset = dataOffset;

      if (dataOffset > h->payloadLength) {
        return false;
      }
    }
    payloadHeadersLength += h->payloadHeaderSize;
    headerPointer += h->payloadHeaderSize;
    if (dataLength == 0) {
      break;
    }
    (*numPayloadHeaders)++;
  }
  if (*numPayloadHeaders > 0) {
    if (firstPayloadOffset < payloadHeadersLength + IEC_HEADER_SIZE_BYTES + h->frameBytesMissing) {
      return false;
    }
  }
  return true;
}

static bool checkBurstSpacing(HANDLE_IEC61937_DECODER h) {
  for (uint32_t k = h->syncCandidateIndex + h->burstRepetitionPeriod - IEC_BURST_SPACING_SIZE_BYTES;
       k < h->syncCandidateIndex + h->burstRepetitionPeriod; k++) {
    if (h->workBuffer[k] != 0) {
      return false;
    }
  }
  return true;
}

HANDLE_IEC61937_DECODER iec61937_decode_open(void) {
  HANDLE_IEC61937_DECODER h;

  h = (HANDLE_IEC61937_DECODER)calloc(1, sizeof(iec61937_decoder_state));
  resetSyncState(h);
  resetParsingState(h);
  resetPendingState(h);

  return h;
}

void iec61937_decode_close(HANDLE_IEC61937_DECODER h) {
  free(h);
}

IECDEC_RESULT iec61937_decode_feed(HANDLE_IEC61937_DECODER h, const uint8_t* inputBuffer,
                                   uint32_t inputBufferLength) {
  if (h == NULL || inputBuffer == NULL) {
    return IECDEC_NULLPTR_ERROR;
  }
  // check if the input data fits into the work buffer
  if (h->workBufferBytesAvailable > UINT32_MAX - inputBufferLength ||
      h->workBufferBytesAvailable + inputBufferLength > WORKBUFFER_SIZE_BYTES) {
    return IECDEC_BUFFER_ERROR;
  }

  // copy the input data to the work buffer
  memcpy(h->workBuffer + h->workBufferBytesAvailable, inputBuffer, inputBufferLength);
  h->workBufferBytesAvailable += inputBufferLength;
  return IECDEC_OK;
}

IECDEC_RESULT iec61937_decode_process(HANDLE_IEC61937_DECODER h, uint8_t* outputBuffer,
                                      uint32_t* pOutputBufferLength, int32_t* pPcmOffset,
                                      uint32_t* pIecFrameLength, bool* pIecFrameProcessed) {
  if (h == NULL || outputBuffer == NULL || pOutputBufferLength == NULL || pPcmOffset == NULL ||
      pIecFrameLength == NULL || pIecFrameProcessed == NULL) {
    return IECDEC_NULLPTR_ERROR;
  }
  uint32_t outputBufferLength = *pOutputBufferLength;
  *pOutputBufferLength = 0;
  *pPcmOffset = 0;
  *pIecFrameLength = 0;
  *pIecFrameProcessed = false;

  while (!h->syncFound && h->workBufferBytesAvailable > IEC_HEADER_SIZE_BYTES) {
    while (!h->syncCandidateFound && h->workBufferBytesAvailable > IEC_HEADER_SIZE_BYTES) {
      for (uint32_t i = 0; i < h->workBufferBytesAvailable - IEC_HEADER_SIZE_BYTES; i++) {
        // search for sync preamble
        if (h->workBuffer[i + 0] == SYNC_PREAMBLE_0 && h->workBuffer[i + 1] == SYNC_PREAMBLE_1 &&
            h->workBuffer[i + 2] == SYNC_PREAMBLE_2 && h->workBuffer[i + 3] == SYNC_PREAMBLE_3) {
          // store the workbuffer index of the sync candidate
          h->syncCandidateIndex = i;

          // parse and process IEC frame data (Pc, Pd)
          int32_t err = parseIecFrameData(h);
          if (err > 0) {
            // something went wrong when parsing the frame data
            continue;
          }

          // signal that a possible sync candidate has been found
          h->syncCandidateFound = true;
          break;
        }  // if preamble
      }    // for loop

      // adjust the workBuffer
      if (h->syncCandidateFound) {
        // remove everything before the syncCandidateIndex
        memmove(h->workBuffer, h->workBuffer + h->syncCandidateIndex,
                h->workBufferBytesAvailable - h->syncCandidateIndex);
        h->workBufferBytesAvailable -= h->syncCandidateIndex;
      } else {
        // no sync found -> only keep the last IEC_HEADER_SIZE_BYTES bytes
        memmove(h->workBuffer, h->workBuffer + h->workBufferBytesAvailable - IEC_HEADER_SIZE_BYTES,
                IEC_HEADER_SIZE_BYTES);
        h->workBufferBytesAvailable = IEC_HEADER_SIZE_BYTES;
      }
      h->syncCandidateIndex = 0;
    }  // while (!h->syncCandidateFound && h->workBufferBytesAvailable - IEC_HEADER_SIZE_BYTES > 0)

    if (h->syncCandidateFound) {
      if (h->workBufferBytesAvailable >= h->syncCandidateIndex + h->burstRepetitionPeriod) {
        if (checkBurstSpacing(h)) {
          // we found an IEC frame
          uint32_t numPayloadHeaders = 0;
          if (checkPayloadHeaders(h, &numPayloadHeaders)) {
            // the found frame is okay
            h->syncFound = true;
            h->numPayloadHeaders = numPayloadHeaders;
            h->payloadHeaderIndex = 0;
          } else {
            // there is some offset missmatch
            // remove everything before the syncCandidateIndex and restart syncing, reset all states
            memmove(h->workBuffer, h->workBuffer + h->syncCandidateIndex + IEC_HEADER_SIZE_BYTES,
                    h->workBufferBytesAvailable - h->syncCandidateIndex - IEC_HEADER_SIZE_BYTES);
            h->workBufferBytesAvailable -= h->syncCandidateIndex;
            resetSyncState(h);
            resetParsingState(h);
            resetPendingState(h);
          }
        } else {
          // no correct IEC frame because burst spacing is wrong
          // remove everything before the syncCandidateIndex and restart syncing
          memmove(h->workBuffer, h->workBuffer + h->syncCandidateIndex + IEC_HEADER_SIZE_BYTES,
                  h->workBufferBytesAvailable - h->syncCandidateIndex - IEC_HEADER_SIZE_BYTES);
          h->workBufferBytesAvailable -= h->syncCandidateIndex;
          resetSyncState(h);
        }
      } else {
        // break the while (!h->syncFound && h->workBufferBytesAvailable - IEC_HEADER_SIZE_BYTES >
        // 0)
        break;
      }
    }  // if (h->syncCandidateFound)
  }    // while (!h->syncFound && h->workBufferBytesAvailable - IEC_HEADER_SIZE_BYTES > 0)

  if (!h->syncFound) {
    // we were unable to find the sync on the current work buffer data
    return IECDEC_FEED_MORE_DATA;
  }

  *pIecFrameLength = h->frameLength;

  // Handle pending data
  if (h->frameBytesMissing > 0) {
    if (h->numPayloadHeaders == 0) {
      // check if current payload bytes complete the pending data
      uint32_t payloadBytesAvailable = h->payloadLength - h->payloadHeaderSize;
      uint32_t dataIndex = h->syncCandidateIndex + IEC_HEADER_SIZE_BYTES + h->payloadHeaderSize;

      if (h->frameBytesMissing > payloadBytesAvailable) {
        // the pending data cannot be completed -> copy complete payload data to pending buffer
        memcpy(h->frameBufferPending + h->frameBytesPending, h->workBuffer + dataIndex,
               payloadBytesAvailable);
        h->frameBytesPending += payloadBytesAvailable;
        h->frameBytesMissing -= payloadBytesAvailable;
        h->pcmOffsetPending -= h->frameLength;
      } else {
        // the pending data can be completed
        if (h->frameBytesMissing < payloadBytesAvailable) {
          // The pending data could be completed, but too much payload data is still available
          return IECDEC_PENDINGDATA_ERROR;
        }
        // check if there is enough space in the output buffer
        if (h->frameBytesPending + h->frameBytesMissing > outputBufferLength) {
          return IECDEC_BUFFER_ERROR;
        }
        // copy previous data
        memcpy(outputBuffer, h->frameBufferPending, h->frameBytesPending);
        // copy current data
        memcpy(outputBuffer + h->frameBytesPending, h->workBuffer + dataIndex,
               h->frameBytesMissing);
        *pOutputBufferLength = h->frameBytesPending + h->frameBytesMissing;
        *pPcmOffset = h->pcmOffsetPending;
        resetPendingState(h);
        return IECDEC_OK;
      }
    } else {
      // pending data can be completed

      // check if there is enough space in the output buffer
      if (h->frameBytesPending + h->frameBytesMissing > outputBufferLength) {
        return IECDEC_BUFFER_ERROR;
      }

      // get first payload header offset
      uint32_t headerIndex = h->syncCandidateIndex + IEC_HEADER_SIZE_BYTES;
      uint8_t* headerPointer = &h->workBuffer[headerIndex];
      uint32_t dataOffset = 0;
      uint32_t dataLength = 0;
      int32_t pcmOffset = 0;
      // Parse audio burst payload header
      parsePayloadHeader(h, headerPointer, &dataOffset, &dataLength, &pcmOffset);

      if (h->syncCandidateIndex + dataOffset < h->frameBytesMissing) {
        resetSyncState(h);
        resetParsingState(h);
        resetPendingState(h);
        return IECDEC_PENDINGDATA_ERROR;
      }
      uint32_t dataIndex = h->syncCandidateIndex + dataOffset - h->frameBytesMissing;

      // copy previous data
      memcpy(outputBuffer, h->frameBufferPending, h->frameBytesPending);
      // copy current data
      memcpy(outputBuffer + h->frameBytesPending, h->workBuffer + dataIndex, h->frameBytesMissing);
      *pOutputBufferLength = h->frameBytesPending + h->frameBytesMissing;
      *pPcmOffset = h->pcmOffsetPending;
      resetPendingState(h);
      return IECDEC_OK;
    }
  }

  // perform parsing of payload and write MPEG-H AU to output
  if (h->payloadHeaderIndex < h->numPayloadHeaders) {
    uint32_t headerIndex = h->syncCandidateIndex + IEC_HEADER_SIZE_BYTES +
                           h->payloadHeaderIndex * h->payloadHeaderSize;
    uint8_t* headerPointer = &h->workBuffer[headerIndex];
    uint32_t dataOffset = 0;
    uint32_t dataLength = 0;
    int32_t pcmOffset = 0;

    // Parse audio burst payload header
    parsePayloadHeader(h, headerPointer, &dataOffset, &dataLength, &pcmOffset);

    // check if there is enough space in the output buffer
    if (dataLength > outputBufferLength) {
      return IECDEC_BUFFER_ERROR;
    }

    // Check if frame is split across IEC frames
    if (dataOffset + dataLength > IEC_HEADER_SIZE_BYTES + h->payloadLength) {
      uint32_t numAuBytesMissing =
          dataLength - (h->payloadLength - dataOffset + IEC_HEADER_SIZE_BYTES);
      uint32_t numAuBytesAvailable = dataLength - numAuBytesMissing;
      h->frameBytesPending = numAuBytesAvailable;
      h->frameBytesMissing = numAuBytesMissing;

      // Write partial data to pending buffer.
      memcpy(h->frameBufferPending, h->workBuffer + h->syncCandidateIndex + dataOffset,
             h->frameBytesPending);
      h->pcmOffsetPending = pcmOffset - (int32_t)h->frameLength;
    } else {
      // Store length and PCM offset of complete AU to be written.
      *pOutputBufferLength = dataLength;
      *pPcmOffset = pcmOffset;
      memcpy(outputBuffer, h->workBuffer + h->syncCandidateIndex + dataOffset, dataLength);
    }

    h->payloadHeaderIndex++;
  }

  if (h->payloadHeaderIndex == h->numPayloadHeaders) {
    // the complete IEC frame has been processed
    // remove the found frame
    memmove(h->workBuffer, h->workBuffer + h->syncCandidateIndex + h->burstRepetitionPeriod,
            h->workBufferBytesAvailable - h->burstRepetitionPeriod);
    h->workBufferBytesAvailable -= h->burstRepetitionPeriod;

    // signal that the complete frame was processed
    *pIecFrameProcessed = true;

    // reset the sync state
    resetSyncState(h);

    // reset the parser state
    resetParsingState(h);
  }
  return IECDEC_OK;
}
