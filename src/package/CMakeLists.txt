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
# Vendored static build from 3rdparty/zlib (always available, no system dependency).
target_link_libraries(pkg_parser PRIVATE ZLIB::ZLIBSTATIC)
target_compile_definitions(pkg_parser PRIVATE KYTY_HAS_ZLIB=1)

# Optional, isolated host-side NAPS decoder. It does not link managed or
# third-party decoder code into the emulator; .NET 9 is needed at runtime.
find_program(KYTY_DOTNET_EXECUTABLE dotnet)
if(KYTY_DOTNET_EXECUTABLE)
    add_custom_target(naps_extractor
        COMMAND ${KYTY_DOTNET_EXECUTABLE} publish
            "${CMAKE_CURRENT_SOURCE_DIR}/naps/NapsExtractor.csproj"
            --configuration Release --no-self-contained
            --output "${CMAKE_BINARY_DIR}/src/launcher/naps"
        VERBATIM
    )
    install(DIRECTORY "${CMAKE_BINARY_DIR}/src/launcher/naps/" DESTINATION naps)
endif()
