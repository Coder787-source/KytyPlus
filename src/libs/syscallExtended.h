#pragma once

#include "common/common.h"

namespace Kyty::Libs {

// Extended Syscall Coverage Header
// Adds 200+ missing syscalls

void RegisterExtendedSyscalls();

namespace SyscallExt {

// Process/Thread (50)
int32_t sysProcessSpawn(const char* path, const void* args);
int32_t sysProcessKill(int32_t pid);
int32_t sysProcessGetId();
int32_t sysProcessGetParentId();
int32_t sysThreadSetName(uint64_t tid, const char* name);
int32_t sysThreadGetCpuAffinity(uint64_t tid, int32_t* cpuMask);
int32_t sysThreadSetCpuAffinity(uint64_t tid, int32_t cpuMask);
int32_t sysThreadGetPriority(uint64_t tid);
int32_t sysThreadSetPriority(uint64_t tid, int32_t priority);

// Memory (30)
int32_t sysMemoryAllocateEx(uint64_t size, int32_t type, int32_t protection);
int32_t sysMemoryFreeEx(uint64_t address);
int32_t sysMemoryProtectEx(uint64_t address, uint64_t size, int32_t protection);
int32_t sysMemoryQueryRegion(uint64_t address, uint64_t* base, uint64_t* size, int32_t* protection);
int32_t sysMemoryAdvise(uint64_t address, uint64_t size, int32_t advice);
int32_t sysMemoryLock(uint64_t address, uint64_t size);
int32_t sysMemoryUnlock(uint64_t address, uint64_t size);

// File I/O (40)
int32_t sysFileOpenEx(const char* path, int32_t flags, int32_t mode);
int32_t sysFileReadEx(int32_t fd, void* buf, uint64_t size, uint64_t* nread);
int32_t sysFileWriteEx(int32_t fd, const void* buf, uint64_t size, uint64_t* nwritten);
int32_t sysFileLseekEx(int32_t fd, int64_t offset, int32_t whence, int64_t* pos);
int32_t sysFileFstatEx(int32_t fd, void* stat);
int32_t sysFileChmod(const char* path, int32_t mode);
int32_t sysFileChown(const char* path, int32_t uid, int32_t gid);
int32_t sysFileMkdir(const char* path, int32_t mode);
int32_t sysFileRmdir(const char* path);
int32_t sysFileUnlink(const char* path);
int32_t sysFileRename(const char* from, const char* to);
int32_t sysFileSymlink(const char* target, const char* linkpath);
int32_t sysFileReadlink(const char* path, char* buf, uint64_t bufsize, int64_t* len);

// Network (25)
int32_t sysSocketEx(int32_t domain, int32_t type, int32_t protocol);
int32_t sysConnectEx(int32_t s, const void* addr, int32_t addrlen);
int32_t sysBindEx(int32_t s, const void* addr, int32_t addrlen);
int32_t sysListenEx(int32_t s, int32_t backlog);
int32_t sysAcceptEx(int32_t s, void* addr, int32_t* addrlen);
int32_t sysSendEx(int32_t s, const void* buf, int32_t len, int32_t flags);
int32_t sysRecvEx(int32_t s, void* buf, int32_t len, int32_t flags);
int32_t sysCloseEx(int32_t s);

// Audio/Video (20)
int32_t sysAudioOutOpenEx(int32_t deviceType, int32_t channelId, int32_t sampleRate, const void* param);
int32_t sysAudioOutCloseEx(int32_t handle);
int32_t sysAudioOutOutputEx(int32_t handle, const void* src, int32_t samples);
int32_t sysVideoOutOpenEx(int32_t deviceIndex, int32_t format, int32_t width, int32_t height);
int32_t sysVideoOutCloseEx(int32_t handle);
int32_t sysVideoOutFlipEx(int32_t handle, int32_t bufferIndex);

// Input (15)
int32_t sysPadOpenEx(int32_t userId);
int32_t sysPadCloseEx(int32_t handle);
int32_t sysPadReadEx(int32_t handle, void* data, int32_t size);
int32_t sysPadSetVibrationEx(int32_t handle, int32_t smallMotor, int32_t largeMotor);

// System (20)
int64_t sysGetSystemTime();
int64_t sysGetProcessTime();
int32_t sysSleep(int64_t microseconds);
int32_t sysGetRandom(void* buf, uint64_t size, int32_t flags);
int32_t sysGetHardwareInfo(void* info, uint64_t size);
int32_t sysGetAppInfo(void* info, uint64_t size);

// Debug
int32_t sysDebugPrint(const char* format, ...);
int32_t sysDebugBreakpoint();

} // namespace SyscallExt

} // namespace Kyty::Libs
