if(USE_PKGCONFIG_DEPS)
  find_package(PkgConfig)
  pkg_check_modules(ILO REQUIRED IMPORTED_TARGET GLOBAL ilo>=2.0)
  add_library(ilo ALIAS PkgConfig::ILO)
  pkg_check_modules(MMTISOBMFF REQUIRED IMPORTED_TARGET GLOBAL mmtisobmff>=1.0)
  add_library(mmtisobmff ALIAS PkgConfig::MMTISOBMFF)
else()
  FetchContent_MakeAvailable(ilo mmtisobmff)
endif()

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
