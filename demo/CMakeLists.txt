FetchContent_MakeAvailable(ilo mmtisobmff)

add_executable(iec61937-13_encoder
  ${PROJECT_SOURCE_DIR}/demo/main_iec61937-13_encoder.cpp
)
target_link_libraries(iec61937-13_encoder
  iec61937-13_enc
  mmtisobmff
  ilo
)

add_executable(iec61937-13_decoder
  ${PROJECT_SOURCE_DIR}/demo/main_iec61937-13_decoder.cpp
)
target_link_libraries(iec61937-13_decoder
  iec61937-13_dec
  mmtisobmff
  ilo
)
