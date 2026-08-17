# Firmware module - PUP parser and LLE module loader
#
# Parses official Sony PS5 firmware update files (.pup) for Low-Level
# Emulation. Does NOT distribute or include any Sony copyrighted material.

file(GLOB firmware_src CONFIGURE_DEPENDS
    *.cpp
    *.h
)

add_library(firmware STATIC ${firmware_src})

target_include_directories(firmware PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(firmware PRIVATE common)

# zlib is required for PFSC (compressed PFS) decompression.
find_package(ZLIB)
if(ZLIB_FOUND)
    target_link_libraries(firmware PRIVATE ZLIB::ZLIB)
    target_compile_definitions(firmware PRIVATE KYTY_HAS_ZLIB=1)
endif()
