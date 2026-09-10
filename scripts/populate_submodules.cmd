@echo off
setlocal
set "REPO=C:\Users\kesha\KytyPlus-fix"
cd /d "%REPO%"

call :clone LibAtrac9 https://github.com/shadps4-emu/ext-LibAtrac9.git ec8899dadf393f655f2871a94e0fe4b3d6220c9a
call :clone SPIRV-Headers https://github.com/KhronosGroup/SPIRV-Headers.git 01e0577914a75a2569c846778c2f93aa8e6feddd
call :clone SPIRV-Tools https://github.com/KhronosGroup/SPIRV-Tools.git 7f2d9ee926f98fc77a3ed1e1e0f113b8c9c49458
call :clone Vulkan-Headers https://github.com/KhronosGroup/Vulkan-Headers.git 2fa203425eb4af9dfc6b03f97ef72b0b5bcb8350
call :clone VulkanMemoryAllocator https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git a1d434708c217b2a6c7b365f1fe41fa03a562e59
call :clone ffmpeg-core https://github.com/shadps4-emu/ext-ffmpeg-core.git 94dde08c8a9e4271a93a2a7e4159e9fb05d30c0a
call :clone fmt https://github.com/fmtlib/fmt.git 11ddbcb7898d2d3445d431a54814367b21dee6ad
call :clone imgui https://github.com/ocornut/imgui.git f1cc2ae15e53a861a874c3034aae6798fde194ab
call :clone magic_enum https://github.com/Neargye/magic_enum.git 1384769c66bd16ec9bb1353f45fe8ec8ccc12dbd
call :clone nlohmann_json https://github.com/nlohmann/json.git 272411c5e6ea45919af7673524e74e60c62116df
call :clone spdlog https://github.com/gabime/spdlog.git 8671ca4d492c8ee1cdfd3dd88afb9f88dd268178
call :clone tracy https://github.com/wolfpld/tracy.git 05cceee0df3b8d7c6fa87e9638af311dbabc63cb
call :clone xxHash https://github.com/Cyan4973/xxHash.git e573d4d2aaeaba0f3e5a0a9a54144a1f2b4b56e7

echo === ALL DONE ===
exit /b 0

:clone
set "NAME=%~1"
set "URL=%~2"
set "COMMIT=%~3"
echo === CLONE %NAME% ===
if exist "3rdparty\%NAME%" rmdir /s /q "3rdparty\%NAME%"
git clone --no-checkout "%URL%" "3rdparty\%NAME%" 2>&1
if errorlevel 1 (
  echo CLONE FAILED %NAME%
  exit /b 1
)
git -C "3rdparty\%NAME%" checkout "%COMMIT%" 2>&1
if errorlevel 1 (
  echo CHECKOUT FAILED %NAME%
  exit /b 1
)
echo OK %NAME%
exit /b 0
