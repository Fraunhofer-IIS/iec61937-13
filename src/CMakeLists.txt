add_library(iec61937-13_enc STATIC)
target_sources(iec61937-13_enc
  PRIVATE
    ${PROJECT_SOURCE_DIR}/src/iec61937_enc.cpp
    ${PROJECT_SOURCE_DIR}/src/iec61937_common.h
)
target_include_directories(iec61937-13_enc
  PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)

add_library(iec61937-13_dec STATIC)
target_sources(iec61937-13_dec
  PRIVATE
    ${PROJECT_SOURCE_DIR}/src/iec61937_dec.cpp
    ${PROJECT_SOURCE_DIR}/src/iec61937_common.h
)
target_include_directories(iec61937-13_dec
  PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)
