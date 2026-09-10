@echo off
setlocal
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" amd64 >nul 2>&1
if errorlevel 1 (
  echo VCVARS FAILED
  exit /b 1
)
set "NINJA=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe"
set "BUILD=C:\Users\kesha\KytyPlus-fix\build"
echo === NINJA BUILD (all test targets) ===
"%NINJA%" -C "%BUILD%" shader_cfg_tests scalar_provenance_tests page_manager_tests memory_tracker_tests shader_vertex_metadata_tests pipeline_cache_data_tests graphics_audio_semantics_tests pm4_ngs2_fuzz_tests package_parser_tests soft_ignore_evidence_tests shader_stage_runtime_tests resource_tracking_tests resource_mutex_tests event_queue_lifetime_tests image_page_table_tests shader_recompiler_compute_tests virtual_memory_allocation_tests
exit /b %errorlevel%
