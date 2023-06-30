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

// system includes
#include <string>
#include <iostream>

// external includes
#include "ilo/memory.h"
#include "mmtisobmff/types.h"
#include "mmtisobmff/logging.h"
#include "mmtisobmff/reader/input.h"
#include "mmtisobmff/reader/reader.h"
#include "mmtisobmff/helper/printhelpertools.h"
#include "mmtisobmff/reader/trackreader.h"

// project includes
#include "iec61937_enc.h"

using namespace mmt::isobmff;

class CProcessor {
 private:
  CIsobmffReader m_reader;
  std::ofstream m_outFile;
  bool m_swapBytes;
  HANDLE_IEC61937_ENCODER m_encoder;

 public:
  CProcessor(std::string& inputFilename, std::string& outputFilename, uint32_t factor,
             bool swapBytes)
      : m_reader(ilo::make_unique<CIsobmffFileInput>(inputFilename)),
        m_outFile(outputFilename, std::ios::out | std::ios::binary),
        m_swapBytes(swapBytes) {
    m_encoder = iec61937_encode_open(factor);
    if (m_encoder == nullptr) {
      throw std::runtime_error("ERROR: IEC61937-13 encoder could not be created!");
    }
    if (!m_outFile) {
      throw std::runtime_error("ERROR: Cannot open output file!");
    }
  }

  ~CProcessor() {
    if (m_encoder != nullptr) {
      iec61937_encode_close(m_encoder);
    }
    if (m_outFile) {
      m_outFile.close();
      if (!m_outFile.good()) {
        std::cout << "Error occurred at writing output file!" << std::endl;
      }
    }
  }

  void process() {
    // Only the first MPEG-H mhm1 track will be processed. Further MPEG-H mhm1 tracks will be
    // skipped!
    bool mhmTrackAlreadyProcessed = false;

    // Getting some information about the available tracks
    std::cout << "Found " << m_reader.trackCount() << " tracks in input file." << std::endl;

    for (const auto& trackInfo : m_reader.trackInfos()) {
      std::cout << "########################################" << std::endl;
      std::cout << "-TrackInfo: " << std::endl;
      std::cout << "-- ID       : " << trackInfo.trackId << std::endl;
      std::cout << "-- Handler  : " << ilo::toString(trackInfo.handler) << std::endl;
      std::cout << "-- Type     : " << tools::trackTypeToString(trackInfo.type) << std::endl;
      std::cout << "-- Codec    : " << ilo::toString(trackInfo.codingName) << std::endl;
      std::cout << "-- Duration : " << trackInfo.duration << std::endl;
      std::cout << "-- Timescale: " << trackInfo.timescale << std::endl;
      std::cout << std::endl;

      if (trackInfo.codec != Codec::mpegh_mhm) {
        std::cout << "Skipping unsupported codec: " << ilo::toString(trackInfo.codingName)
                  << std::endl;
        std::cout << std::endl;
        continue;
      }

      if (mhmTrackAlreadyProcessed) {
        std::cout << "Skipping further mhm1 track!" << std::endl;
        std::cout << std::endl;
        continue;
      }

      std::cout << "Creating reader for track with ID " << trackInfo.trackId << " ... ";

      // Create a generic track reader for track number i
      std::unique_ptr<CGenericTrackReader> trackReader =
          m_reader.trackByIndex<CGenericTrackReader>(trackInfo.trackIndex);

      if (trackReader == nullptr) {
        std::cout << "Error: Track reader could not be created!" << std::endl;
        continue;
      } else {
        std::cout << "Done!" << std::endl;
      }

      std::cout << std::endl;
      std::cout << "Sample Info:" << std::endl;
      std::cout << "########################################" << std::endl;
      std::cout << "Max Sample Size        : " << trackInfo.maxSampleSize << " Bytes" << std::endl;
      std::cout << "Total number of samples: " << trackInfo.sampleCount << std::endl;
      std::cout << std::endl;

      std::cout << "Reading all samples of this track" << std::endl;
      std::cout << "########################################" << std::endl;

      // Preallocate the sample with max sample size to avoid reallocation of memory.
      // Sample can be re-used for each nextSample call.
      CSample sample{trackInfo.maxSampleSize};

      uint64_t sampleCounter = 0;
      ilo::ByteBuffer iecOutputData(MAX_IEC61937_FRAME_SIZE_BYTES);
      uint32_t iecOutputBytes = 0;
      IECENC_RESULT returnValue = IECENC_OK;

      // Get all samples in order. Each call fetches the next sample.
      trackReader->nextSample(sample);
      while (!sample.empty()) {
        // Get as many output frames as possible.
        bool fReadMoreData = false;
        while (!fReadMoreData) {
          iecOutputBytes = MAX_IEC61937_FRAME_SIZE_BYTES;
          // Encode into iec61937-13 format
          returnValue = iec61937_encode_process(
              m_encoder, sample.rawData.data(), sample.rawData.size(), &fReadMoreData,
              sample.duration, iecOutputData.data(), &iecOutputBytes);
          if (returnValue != IECENC_OK) {
            throw std::runtime_error(
                "ERROR: Internal buffer too small or rate factor too small or duration exceeds "
                "maximum.");
          }

          // Write to data file
          if (iecOutputBytes > 0) {
            if (m_swapBytes) {
              // Reorder Bytes
              for (uint32_t i = 0; i < iecOutputBytes; i += 2) {
                std::swap(iecOutputData.at(i), iecOutputData.at(i + 1));
              }
            }

            m_outFile.write(reinterpret_cast<const char*>(iecOutputData.data()),
                            iecOutputBytes * sizeof(uint8_t));
          }
        }

        sampleCounter++;
        std::cout << "Samples processed: " << sampleCounter << "\r" << std::flush;

        trackReader->nextSample(sample);
      }

      mhmTrackAlreadyProcessed = true;

      std::cout << std::endl;
    }

    if (!mhmTrackAlreadyProcessed) {
      throw std::runtime_error("No data to encode found!");
    }
  }
};

static bool parseCmdlInteger(const char* arg, uint32_t& result) {
  std::istringstream ss(arg);
  if (!(ss >> result)) {
    std::cout << "Invalid number: " << arg << std::endl;
    return false;
  } else if (!ss.eof()) {
    std::cout << "Trailing characters after number: " << arg << std::endl;
    return false;
  }
  return true;
}

int main(int argc, char** argv) {
  // Configure mmtisobmff logging to your liking (logging to file, system, console or disable)
  disableLogging();

  if (argc != 5) {
    std::cout << "Usage: IEC61937-13_encoder_example <inputFile-URI> <outputFile-URI> <samplerate "
                 "factor> <swap byte order flag>"
              << std::endl;
    std::cout << "  samplerate factor    : 4 or 16" << std::endl;
    std::cout << "  swap byte order flag : 1 to swap pairwise, 0 to keep the byte order"
              << std::endl;
    std::cout << "    NOTE: the default byte order is Big-Endian" << std::endl;
    return 0;
  }

  std::string inputFileUri = std::string(argv[1]);
  std::string outputFileUri = std::string(argv[2]);

  // parse and check samplerate factor
  uint32_t factor = 0;
  if (!parseCmdlInteger(argv[3], factor)) {
    return 1;
  }
  if (factor != 4 && factor != 16) {
    std::cout << "Unsupported samplerate factor: " << factor << std::endl;
    return 1;
  }

  // parse and check swap bytes flag
  uint32_t swapBytes = 0;
  if (!parseCmdlInteger(argv[4], swapBytes)) {
    return 1;
  }
  if (swapBytes != 0 && swapBytes != 1) {
    std::cout << "Unsupported swap byte order value: " << swapBytes << std::endl;
    return 1;
  }

  std::cout << "Reading from input file: " << inputFileUri << std::endl;
  std::cout << "Writing to output file: " << outputFileUri << std::endl;
  std::cout << std::endl;

  try {
    CProcessor processor(inputFileUri, outputFileUri, factor, swapBytes > 0);
    processor.process();
  } catch (const std::exception& e) {
    std::cout << std::endl << "Exception caught: " << e.what() << std::endl;
    return 1;
  } catch (...) {
    std::cout << std::endl
              << "Error: An unknown error happened. The program will exit now." << std::endl;
    return 1;
  }

  return 0;
}
