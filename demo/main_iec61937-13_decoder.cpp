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
#include <fstream>
#include <memory>
#include <string>

// External includes
#include "ilo/memory.h"
#include "mmtisobmff/types.h"
#include "mmtisobmff/logging.h"
#include "mmtisobmff/writer/writer.h"
#include "mmtisobmff/writer/trackwriter.h"

// project includes
#include "iec61937_dec.h"

using namespace mmt::isobmff;

/**
 * @brief This function reads an integer value using a varying number of bits from the bitstream
 *        q.v. ISO/IEC FDIS 23003-3  Table 16
 *
 * @param parser    ilo::CBitParser
 * @param numBits1  number of bits to read for a small integer value or escape value
 * @param numBits2  number of bits to read for a medium sized integer value or escape value
 * @param numBits3  number of bits to read for a large integer value
 * @return integer value read from bitstream
 */
static uint64_t escapedValue(ilo::CBitParser& parser, uint32_t numBits1, uint32_t numBits2,
                             uint32_t numBits3) {
  uint64_t value = parser.read<uint64_t>(numBits1);
  uint64_t tmp = (1 << numBits1) - 1;
  if (value == tmp) {
    uint64_t valueAdd = parser.read<uint64_t>(numBits2);
    value += valueAdd;
    tmp = (1 << numBits2) - 1;
    if (valueAdd == tmp) {
      valueAdd = parser.read<uint64_t>(numBits3);
      value += valueAdd;
    }
  }
  return value;
}

/**
 * @brief This function analyzes a MPEG-H frame to identify whether the processed MPEG-H frame is a
 * RAP (random access point).
 *
 * This information will be used in MP4 file format to mark a MP4 sample entry (containing the
 * processed MPEG-H frame) with sync sample flag.
 *
 * @warning Please note that the exemplary implementation to identify whether the processed MEPG-H
 * frame is a RAP or not is not the full solution. In this example a reduced 'isRAP' identification
 * is implemented since the complete solution will broaden the scope of this example. A complete
 * solution will require additional information from 3DAFrame and 3DAConfig packets.
 */
bool isSyncSample(const ilo::ByteBuffer& buffer) {
  ilo::CBitParser parser(buffer);
  uint64_t packType = 0;
  uint64_t packLength = 0;
  bool isRAP = false;

  while (!parser.eof()) {
    packType = escapedValue(parser, 3, 8, 8);
    if (packType == 1) {
      isRAP = true;
      break;
    }
    /*packLabel = */ escapedValue(parser, 2, 8, 32);
    packLength = escapedValue(parser, 11, 24, 24);

    parser.seek(packLength * 8, ilo::EPosType::cur);
  }
  return isRAP;
}

static constexpr uint32_t inputChunkSize = 1024 * 2 * 2 * 4;  // for swapping bytes this should
                                                              // be an even number!

class CProcessor {
 private:
  std::ifstream m_inFile;
  bool m_swapBytes;
  HANDLE_IEC61937_DECODER m_decoder;
  std::unique_ptr<CIsobmffFileWriter> m_writer;

 public:
  CProcessor(const std::string& inputFilename, const std::string& outputFilename, bool swapBytes)
      : m_inFile(inputFilename, std::ios::in | std::ios::binary), m_swapBytes(swapBytes) {
    m_decoder = iec61937_decode_open();
    if (m_decoder == nullptr) {
      throw std::runtime_error("ERROR: IEC61937-13 decoder could not be created!");
    }
    if (!m_inFile) {
      throw std::runtime_error("ERROR: Cannot open input file!");
    }

    // Configure the output
    CIsobmffFileWriter::SOutputConfig outputConfig;
    outputConfig.outputUri = outputFilename;
    // Optional: Path to tmp file. If not set, a unique tmp file
    //           will be generated in system specific tmp dir.
    outputConfig.tmpUri = "";

    SMovieConfig movieConfig;
    movieConfig.majorBrand = ilo::toFcc("mp42");

    // Create a non-fragmented (plain) MP4 file writer
    m_writer = ilo::make_unique<CIsobmffFileWriter>(outputConfig, movieConfig);
  }

  ~CProcessor() {
    if (m_decoder != nullptr) {
      iec61937_decode_close(m_decoder);
    }
    // Finish the file, delete temp files, close the file library
    try {
      m_writer->close();
    } catch (...) {
      std::cout << "Error closing the MP4 file writer!" << std::endl;
    }
  }

  void process() {
    // Pre-Allocate the sample with max sample size to avoid re-allocation of memory.
    CSample sample{MAX_MPEGH_FRAME_SIZE};

    // Adjust MPEG-H configuration
    SMpeghMhm1TrackConfig mpeghConfig;
    mpeghConfig.mediaTimescale = 48000;
    mpeghConfig.sampleRate = 48000;

    // Create MPEG-H track writer
    std::unique_ptr<CMpeghTrackWriter> mpeghTrackWriter =
        m_writer->trackWriter<CMpeghTrackWriter>(mpeghConfig);

    // Init structure and assign buffer
    bool inputDataAvailable = false;
    ilo::ByteBuffer inputBuffer(inputChunkSize);

    // Get all MPEG-H samples in order.
    // Each call fetches the next sample and writes it immediately to file.
    uint64_t sampleCounter = 0;
    int64_t currentRef = 0;
    int64_t lastPts = 0;
    CSample lastSample;
    uint32_t lastIecFrameLength = 0;
    IECDEC_RESULT err = IECDEC_OK;
    while (m_inFile) {
      m_inFile.read(reinterpret_cast<char*>(inputBuffer.data()),
                    inputBuffer.size() * sizeof(uint8_t));
      uint64_t inputDataRead = m_inFile.gcount();
      inputBuffer.resize(inputDataRead);

      if (m_swapBytes) {
        // Reorder Bytes
        for (uint64_t i = 0; i < inputDataRead; i += 2) {
          std::swap(inputBuffer.at(i), inputBuffer.at(i + 1));
        }
      }
      err = iec61937_decode_feed(m_decoder, inputBuffer.data(), inputDataRead);
      if (err != IECDEC_OK) {
        throw std::runtime_error("ERROR: Unable to feed data to the IEC decoder!");
      }

      inputDataAvailable = true;

      while (inputDataAvailable) {
        uint32_t outputDataLength = MAX_MPEGH_FRAME_SIZE;
        int32_t pcmOffset = 0;
        uint32_t iecFrameLength = 0;
        bool iecFrameProcessed = false;
        sample.clear();
        sample.rawData.resize(MAX_MPEGH_FRAME_SIZE);

        err = iec61937_decode_process(m_decoder, sample.rawData.data(), &outputDataLength,
                                      &pcmOffset, &iecFrameLength, &iecFrameProcessed);
        switch (err) {
          case IECDEC_BUFFER_ERROR:
            throw std::runtime_error("ERROR: Not enough space in provided output buffer!");
            break;
          case IECDEC_NULLPTR_ERROR:
            throw std::runtime_error("ERROR: A nullptr was provided!");
            break;
          case IECDEC_PENDINGDATA_ERROR:
            throw std::runtime_error(
                "ERROR: Something went wrong while trying to complete a splitted frame!");
            break;
          case IECDEC_FEED_MORE_DATA:
            // Feed more data to the decoder.
            inputDataAvailable = false;
            break;
          default:
            break;
        }

        if (outputDataLength > 0) {
          // Because of MPEG-H frame duration calculation required for sample entry duration
          // indication in ISO BMFF we delay writing to the output file in order to calculate the
          // MPEG-H frame duration from IEC61937-13 PTS information. If the retrieved MPEG-H frame
          // will be e.g. decoded instead, the following steps can be omitted and the retrieved
          // MPEG-H frame, together with the corresponding PTS, can be processed directly.
          sample.rawData.resize(outputDataLength);
          sample.isSyncSample = isSyncSample(sample.rawData);
          if (sample.isSyncSample) {
            std::cout << "Sample " << sampleCounter
                      << " can be marked as RAP (random access point)!" << std::endl;
          }
          sample.duration = 1;  // intermediate value; will be set later
          int64_t currentPts = currentRef + pcmOffset;

          if (lastSample.duration != 0) {  // we have previously processed an MPEG-H frame
            lastSample.duration = currentPts - lastPts;
            mpeghTrackWriter->addSample(lastSample);
            sampleCounter++;
          }

          lastPts = currentPts;
          lastSample = CSample{sample};

          std::cout << "Samples processed: " << sampleCounter << "\r" << std::flush;
        }
        if (iecFrameProcessed) {  // update IEC time line
          currentRef += iecFrameLength;
          lastIecFrameLength = iecFrameLength;
        }
      }
    }

    if (lastSample.duration != 0) {
      // For simplification it is assumed that the last samples' duration is equal to the IEC frame
      // length!
      lastSample.duration = lastIecFrameLength;
      mpeghTrackWriter->addSample(lastSample);
      sampleCounter++;
      std::cout << "Samples processed: " << sampleCounter << "\r" << std::flush;
    }
    std::cout << std::endl;
  }
};

static bool parseCmdlInteger(const char* arg, int32_t& result) {
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

  if (argc != 4) {
    std::cout << "Usage: IEC61937-13_decoder_example <inputFile-URI> <outputFile-URI> <swap byte "
                 "order flag>"
              << std::endl;
    std::cout << "  swap byte order flag : 1 to swap pairwise, 0 to keep the byte order"
              << std::endl;
    std::cout << "    NOTE: the default byte order is Big-Endian" << std::endl;
    return 0;
  }

  std::string inputFileUri = std::string(argv[1]);
  std::string outputFileUri = std::string(argv[2]);

  // parse and check swap bytes flag
  int32_t swapBytes = 0;
  if (!parseCmdlInteger(argv[3], swapBytes)) {
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
    CProcessor processor(inputFileUri, outputFileUri, swapBytes == 1);
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
