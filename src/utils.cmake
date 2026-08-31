# This file is included both from the repository-root CMakeLists.txt and when
# CI configures src/ directly. In the latter mode the root has not initialized
# KYTY_SOURCE_DIR, so derive it from this helper file before any function uses it.
if(NOT DEFINED KYTY_SOURCE_DIR OR KYTY_SOURCE_DIR STREQUAL "")
	set(KYTY_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}")
endif()

# Keep the existing Linux IWYU warnings non-fatal.
set(KYTY_IWYU_COMMON "-Xiwyu;--cxx17ns;-Qunused-arguments")
if (LINUX)
	set(KYTY_IWYU_STRICT "")
else()
	set(KYTY_IWYU_STRICT ";-Werror")
endif()

function(include_what_you_use target dirs)
  if (CLANG AND ("${target}" IN_LIST KYTY_IWYU))
    find_program (CLANG_IWYU_EXE NAMES "include-what-you-use")
    if (CLANG_IWYU_EXE)
		set_target_properties(${target} PROPERTIES CXX_INCLUDE_WHAT_YOU_USE "${CLANG_IWYU_EXE};${KYTY_IWYU_COMMON}${KYTY_IWYU_STRICT}")
    endif()
  endif()
endfunction()

function(include_what_you_use_with_mappings target dirs mappings)
  if (CLANG AND ("${target}" IN_LIST KYTY_IWYU))
    find_program (CLANG_IWYU_EXE NAMES "include-what-you-use")
    if (CLANG_IWYU_EXE)
		foreach(map ${mappings})
			list(APPEND mapdirs ";-Xiwyu;--mapping_file=${map}")
		endforeach()
		set_target_properties(${target} PROPERTIES CXX_INCLUDE_WHAT_YOU_USE "${CLANG_IWYU_EXE};${mapdirs};${KYTY_IWYU_COMMON}${KYTY_IWYU_STRICT}")
    endif()
  endif()
endfunction()

function(clang_tidy_check target config headers dirs)
  if (KYTY_ENABLE_CLANG_TIDY AND CLANG AND ("${target}" IN_LIST KYTY_CLANG_TIDY) AND NOT KYTY_CLANG_CL)
    find_program (CLANG_TIDY_EXE NAMES "clang-tidy")
    if (CLANG_TIDY_EXE)
		set(std_arg "-extra-arg=-std=c++${CMAKE_CXX_STANDARD}")
		foreach(dir ${dirs})
			list(APPEND incdirs "-extra-arg=-I${dir}")
		endforeach()
		foreach(header ${headers})
			list(APPEND filter "(${header}.*)")
		endforeach()
		string(REPLACE ";" "|" filter "${filter}")
		if ("${config}" STREQUAL "")
			set_target_properties(${target} PROPERTIES CXX_CLANG_TIDY "${CLANG_TIDY_EXE};-warnings-as-errors=*;-header-filter=${filter};${std_arg};${incdirs}")	
		else()		
			set_target_properties(${target} PROPERTIES CXX_CLANG_TIDY "${CLANG_TIDY_EXE};-config=${config};-warnings-as-errors=*;-header-filter=${filter};${std_arg};${incdirs}")	
		endif()
    endif()
  endif()
endfunction()

function(clang_tidy_fix target config headers dirs)
  if (KYTY_ENABLE_CLANG_TIDY AND CLANG AND ("${target}" IN_LIST KYTY_CLANG_TIDY))
    find_program (CLANG_TIDY_EXE NAMES "clang-tidy")
    if (CLANG_TIDY_EXE)
		set(std_arg "-extra-arg=-std=c++${CMAKE_CXX_STANDARD}")
		foreach(dir ${dirs})
			list(APPEND incdirs "-extra-arg=-I${dir}")
		endforeach()
		foreach(header ${headers})
			list(APPEND filter "(${header}.*)")
		endforeach()
		string(REPLACE ";" "|" filter "${filter}")
		if ("${config}" STREQUAL "")
			set_target_properties(${target} PROPERTIES CXX_CLANG_TIDY "${CLANG_TIDY_EXE};-fix;-format-style=file;-header-filter=${filter};${std_arg};${incdirs}")	
		else()	
			set_target_properties(${target} PROPERTIES CXX_CLANG_TIDY "${CLANG_TIDY_EXE};-fix;-format-style=file;-config=${config};-header-filter=${filter};${std_arg};${incdirs}")	
		endif()
    endif()
  endif()
endfunction()

macro(config_compiler_and_linker)

set(KYTY_WARNINGS_ARE_ERRORS OFF)

set(KYTY_C_FLAGS "")
set(KYTY_CPP_FLAGS "")

# Apple's /usr/bin/ar does not understand @response-file syntax, and macOS has a
# large ARG_MAX so response files aren't needed. Note: the Ninja generator forces a
# response file whenever CMAKE_NINJA_FORCE_RESPONSE_FILE is *defined* (it tests
# definedness, not the value), so on Apple it must be fully unset, not set to 0.
if(APPLE)
	unset(CMAKE_NINJA_FORCE_RESPONSE_FILE CACHE)
else()
	SET(CMAKE_NINJA_FORCE_RESPONSE_FILE 1 CACHE INTERNAL "")
endif()

if(KYTY_CLANG_CL)
	if(CMAKE_CXX_FLAGS MATCHES "/W[0-4]")
		string(REGEX REPLACE "/W[0-4]" "/W3" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
	else()
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W3")
	endif()
	
	string(REGEX REPLACE "/MD" "/MT" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
	string(REGEX REPLACE "/MD" "/MT" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
	string(REGEX REPLACE "/MD" "/MT" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
	string(REGEX REPLACE "/MD" "/MT" CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}")
	string(REGEX REPLACE "/MD" "/MT" CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE}")
	string(REGEX REPLACE "/MD" "/MT" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
			
	set(KYTY_CPP_FLAGS "${KYTY_CPP_FLAGS} /utf-8 /Oy- /wd4244 /wd4305 /wd4800 /wd4345")
  
	if(KYTY_WARNINGS_ARE_ERRORS)
		#set(KYTY_CPP_FLAGS "${KYTY_CPP_FLAGS} /WX")
	    add_compile_options(/WX)
	endif()

	add_compile_options("$<$<CONFIG:Release>:/O2>")
	add_compile_options("$<$<CONFIG:Release>:/DNDEBUG>")
	add_compile_options("$<$<CONFIG:RelWithDebInfo>:/O2>")
	add_compile_options("$<$<CONFIG:RelWithDebInfo>:/DNDEBUG>")

	set(KYTY_C_FLAGS "${KYTY_CPP_FLAGS}")
	
elseif(CLANG OR GCC)

	if (CLANG)
	    set(KYTY_CPP_FLAGS "${KYTY_CPP_FLAGS} -fno-rtti -fno-exceptions -fcolor-diagnostics -finput-charset=UTF-8 -fexec-charset=UTF-8 -g -fno-strict-aliasing -fno-omit-frame-pointer -Wall -fmessage-length=0")
	    if (WIN32)
	    	set(KYTY_CPP_FLAGS "${KYTY_CPP_FLAGS} -static")
	    endif()
	else()
		set(KYTY_CPP_FLAGS "${KYTY_CPP_FLAGS} -fno-exceptions -fdiagnostics-color=always -finput-charset=UTF-8 -fexec-charset=UTF-8 -static-libgcc -static-libstdc++ -g -fno-strict-aliasing -fno-omit-frame-pointer -Wall -Wno-unused-value -fmessage-length=0")
	endif()
	
    if(KYTY_WARNINGS_ARE_ERRORS)
        #set(KYTY_CPP_FLAGS "${KYTY_CPP_FLAGS} -Werror")
	add_compile_options(-Werror)
    endif()	

	add_link_options("-g")
	
	unset(CMAKE_CXX_STANDARD_LIBRARIES CACHE)
	unset(CMAKE_C_STANDARD_LIBRARIES CACHE)
	
	set(KYTY_C_FLAGS "${KYTY_CPP_FLAGS}")
	
endif()

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${KYTY_C_FLAGS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${KYTY_CPP_FLAGS}")

endmacro()

# Build a minimal test executable that links the same way the emulator does.
# Lifted out of CMakeLists.txt so all subdirectories see one canonical definition
# (the merged tree had it declared in both top-level and src/CMakeLists.txt).
function(add_kyty_full_emulator_test target source)
	add_executable(${target} EXCLUDE_FROM_ALL ${source} ${kyty_emulator_src})
	# Match kyty_emulator's own link set: component libraries that live outside
	# the kyty_emulator_src glob (gcn_decoder, pkg_parser) must also be linked
	# here or full-emulator test executables fail at link time with undefined
	# symbols from those TUs.
	target_link_libraries(${target} ${kyty_emulator_link_libraries} gcn_decoder pkg_parser)
	# Propagate the emulator's compile definitions (e.g. KYTY_HAS_ZLIB,
	# KYTY_HAS_MBEDTLS) so test TUs see the same feature set as kyty_emulator.
	target_compile_definitions(${target} PRIVATE ${kyty_emulator_compile_definitions})
	target_include_directories(${target} PRIVATE ${inc_headers})
	if (WIN32)
		target_link_libraries(${target} iphlpapi)
		# crypt32: TLS system-trust-store export (mbedtls/libSsl HTTPS)
		target_link_libraries(${target} crypt32)
	endif()
	if (KYTY_CLANG_CL)
		target_link_libraries(${target} onecore)
		add_custom_command(TARGET ${target} POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "${KYTY_THIRD_PARTY_DIR}/winpthread/bin/libwinpthread-1.dll" $<TARGET_FILE_DIR:${target}>/libwinpthread-1.dll)
	endif()
	# The macOS x86_64 guest address space needs its .zerofill segments anchored by
	# linker flags, or the kernel kills the binary on load (posix_spawn EIO).
	if (COMMAND configure_macos_guest_address_space)
		configure_macos_guest_address_space(${target})
	endif()
endfunction()

# Anchor the .zerofill segments of an x86_64 macOS-guest emulator binary so the
# loader does not abort with posix_spawn EIO. Lifted to utils.cmake so that
# test executables (which use add_kyty_full_emulator_test) and the main emulator
# can share the same configuration.
function(configure_macos_guest_address_space target)
	if(APPLE AND (CMAKE_OSX_ARCHITECTURES STREQUAL "x86_64" OR
			(NOT CMAKE_OSX_ARCHITECTURES AND CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64)$")))
		set(kyty_macos_guest_address_space_source
			"${KYTY_SOURCE_DIR}/kernel/macosGuestAddressSpace.cpp")
		if(NOT EXISTS "${kyty_macos_guest_address_space_source}")
			message(FATAL_ERROR
				"macOS guest address-space source not found: ${kyty_macos_guest_address_space_source}")
		endif()
		target_sources(${target} PRIVATE "${kyty_macos_guest_address_space_source}")
		target_compile_definitions(${target} PRIVATE KYTY_LINKED_GUEST_ADDRESS_SPACE=1)
		target_link_options(${target} PRIVATE
			-Wl,-ld_classic,-no_pie,-no_fixup_chains,-no_huge,-pagezero_size,0x40000,-segaddr,SYSTEM_MANAGED,0x40000,-segaddr,SYSTEM_RESERVED,0x7ffffc000,-segaddr,USER_AREA,0x7000000000,-image_base,0xfc0000000)
	endif()
endfunction()
