# Firmware / package parser library
#
# Parses PS4/PS5 .pkg package containers and PFS filesystem images.
# Does NOT distribute or include any Sony copyrighted material.

file(GLOB package_src CONFIGURE_DEPENDS
    *.cpp
    *.h
)

add_library(package STATIC ${package_src})

target_include_directories(package PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(package PRIVATE common)

# zlib is required for PFSC (compressed PFS) decompression.
find_package(ZLIB)
if(ZLIB_FOUND)
    target_link_libraries(firmware PRIVATE ZLIB::ZLIB)
    target_compile_definitions(firmware PRIVATE KYTY_HAS_ZLIB=1)
endif()
