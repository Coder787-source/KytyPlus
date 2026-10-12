# Exercise the actual executable without a GPU or a game dump. An invalid PKG
# makes successful argument parsing exit through the package error path.
if(NOT DEFINED EMULATOR OR NOT DEFINED TEST_WORK_DIR)
  message(FATAL_ERROR "EMULATOR and TEST_WORK_DIR are required")
endif()
file(MAKE_DIRECTORY "${TEST_WORK_DIR}")
set(fake_pkg "${TEST_WORK_DIR}/invalid-cli-test.pkg")
file(WRITE "${fake_pkg}" "not a package")

function(expect_package_path name)
  execute_process(COMMAND "${EMULATOR}" ${ARGN} --install-pkg "${fake_pkg}"
    WORKING_DIRECTORY "${TEST_WORK_DIR}"
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 15)
  if(NOT result EQUAL 1 OR NOT output MATCHES "PKG parse failed:")
    message(FATAL_ERROR "${name}: expected package error after parsing; result=${result}\n${output}\n${error}")
  endif()
  message(STATUS "PASS: ${name}")
endfunction()

function(expect_argument_error name expected)
  execute_process(COMMAND "${EMULATOR}" ${ARGN} --install-pkg "${fake_pkg}"
    WORKING_DIRECTORY "${TEST_WORK_DIR}"
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 15)
  if(result EQUAL 0 OR NOT output MATCHES "${expected}" OR output MATCHES "PKG parse failed:")
    message(FATAL_ERROR "${name}: expected argument rejection; result=${result}\n${output}\n${error}")
  endif()
  message(STATUS "PASS: ${name}")
endfunction()

expect_package_path("FSR defaults")
expect_package_path("FSR Off with inherited output dimensions" --upscaler-method Off)
expect_package_path("FSR Off with explicit window dimensions"
  --upscaler-method Off --fsr-output-width 0 --fsr-output-height 0)
expect_package_path("FSR explicit 4K" --upscaler-method Fsr1
  --fsr-output-width 3840 --fsr-output-height 2160)
# The essential option combination copied from discussion #15, including --game.
expect_package_path("discussion 15 legacy launcher arguments"
  --screen-width 1920 --screen-height 1080 --vblank-frequency 60 --console-language 1
  --vulkan-validation true --shader-validation true --shader-optimization-type Size
  --shader-log-direction Silent --shader-log-folder _Shaders --command-buffer-dump false
  --command-buffer-dump-folder _Buffers --printf-direction Silent --printf-output-file _kyty.txt
  --profiler-direction None --upscaler-method Off --upscaler-quality UltraQuality
  --upscaler-sharpness 0.30 --igpu-optimization Auto --texture-lod-bias 0
  --present-mode Fifo --present-filter Linear --aspect-ratio Stretch
  --spirv-debug-printf false --game "${TEST_WORK_DIR}")
expect_argument_error("explicit 4K plus Off remains invalid" "dimensions require"
  --upscaler-method Off --fsr-output-width 3840 --fsr-output-height 2160)
expect_argument_error("dimension pair validation" "supplied as a pair"
  --fsr-output-width 0)
expect_argument_error("invalid render dimension" "invalid dimension"
  --fsr-output-width -1)
expect_package_path("Off then Fsr1 follows final method"
  --upscaler-method Off --upscaler-method Fsr1)
expect_package_path("Fsr1 then Off follows final method"
  --upscaler-method Fsr1 --upscaler-method Off)
