# PKG / PFS package parser library
#
# Parses PS4/PS5 .pkg package containers and PFS filesystem images.
# Does NOT distribute or include any Sony copyrighted material.

file(GLOB pkg_parser_src CONFIGURE_DEPENDS
    *.cpp
    *.h
)

add_library(pkg_parser STATIC ${pkg_parser_src})

target_include_directories(pkg_parser PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(pkg_parser PRIVATE common)

# zlib is required for PFSC (compressed PFS) decompression.
find_package(ZLIB)
if(ZLIB_FOUND)
    target_link_libraries(pkg_parser PRIVATE ZLIB::ZLIB)
    target_compile_definitions(pkg_parser PRIVATE KYTY_HAS_ZLIB=1)
endif()