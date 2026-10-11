// SPDX-License-Identifier: MIT
#include "common/file.h"
#include "common/readOnlyFileSystem.h"
#include "package/gameImageVolume.h"
#include "kernel/fileSystem.h"
#include "libs/errno.h"
#include "loader/elf.h"
#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <string>
#include <thread>
#include <vector>

int main(int argc, char** argv) {
    if (argc != 4) { std::fprintf(stderr, "Usage: image mount test <ffpfsc> <helper> <expected-eboot>\n"); return 1; }
    int failures = 0;
    auto check = [&failures](bool ok, const char* message) {
        std::fprintf(stderr, "%s: %s\n", ok ? "PASS" : "FAIL", message); if (!ok) ++failures;
    };
    std::string error;
    auto volume = Libs::Firmware::OpenGameImage(argv[1], argv[2], error);
    if (!volume) { std::fprintf(stderr, "%s\n", error.c_str()); return 1; }
    auto root = std::filesystem::absolute(argv[1]) / "__kyty_image_root__";
    Common::MountReadOnlyFileSystem(root, volume);
    check(!std::filesystem::exists(root), "no extracted game directory");
    check(Common::File::IsDirectoryExisting(root), "virtual root exists");
    check(Common::File::IsFileExisting(root / "eboot.bin"), "image eboot exists");
    check(!Common::File::IsFileExisting(root / "../outside.bin"), "parent traversal rejected");
    auto entries = Common::File::GetDirEntries(root);
    check(std::any_of(entries.begin(), entries.end(), [](const auto& e) { return e.name == "eboot.bin" && e.is_file; }), "virtual directory lists eboot");
    Common::File expected, file;
    check(expected.Open(argv[3], Common::File::Mode::Read), "open reference eboot");
    check(file.Open(root / "eboot.bin", Common::File::Mode::Read), "open image eboot");
    if (file.IsInvalid() || expected.IsInvalid()) return 1;
    check(file.Size() == expected.Size(), "64-bit file size matches");
    std::vector<uint8_t> a(100000), b(100000);
    for (uint64_t offset: {uint64_t(0), uint64_t(65530), file.Size() / 2, file.Size() > a.size() ? file.Size() - a.size() : uint64_t(0)}) {
        auto take = static_cast<uint32_t>(std::min<uint64_t>(a.size(), file.Size() - offset));
        file.Seek(offset); expected.Seek(offset); uint32_t got = 0;
        file.Read(a.data(), take, &got); expected.Read(b.data(), take);
        check(got == take && std::equal(a.begin(), a.begin() + take, b.begin()), "random/cross-block image read equals reference");
    }
    file.Seek(file.Size() + 100); uint32_t got = 1; file.Read(a.data(), 1, &got);
    check(got == 0, "seek past EOF returns zero bytes");
    file.Close(); expected.Close();
    check(!file.Open(root / "eboot.bin", Common::File::Mode::Write), "write opens rejected");
    namespace FS = Libs::LibKernel::FileSystem;
    FS::FileSystemSubsystem::Instance()->Init(nullptr);
    FS::Mount(root, "/app0");
    int fd = FS::KernelOpen("/app0/eboot.bin", 0, 0);
    check(fd >= 3, "guest kernel open");
    if (fd >= 3) {
        FS::FileStat stat{}; check(FS::KernelFstat(fd, &stat) == 0 && stat.st_size == static_cast<int64_t>(Common::File::Size(root / "eboot.bin")), "guest fstat size");
        check(FS::KernelPread(fd, a.data(), 64, 0) == 64, "guest pread");
        check(FS::KernelLseek(fd, 10, 0) == 10 && FS::KernelRead(fd, a.data(), 32) == 32, "guest seek/read");
        FS::KernelClose(fd);
    }
    check(FS::KernelOpen("/app0/eboot.bin", 1, 0) == Libs::LibKernel::KERNEL_ERROR_EROFS, "guest write returns EROFS");
    fd = FS::KernelOpen("/app0", 0x20000, 0);
    check(fd >= 3, "guest directory open");
    if (fd >= 3) { check(FS::KernelGetdents(fd, reinterpret_cast<char*>(a.data()), 4096) > 0, "guest getdents"); FS::KernelClose(fd); }
    { Loader::Elf64 elf; elf.Open(root / "eboot.bin"); check(elf.IsValid(), "actual ELF/SELF loader reads mounted eboot"); }
    // Independent file handles share a serialized reader but not their offsets.
    auto read = [&]() { Common::File f; if (!f.Open(root / "eboot.bin", Common::File::Mode::Read)) return false; auto data = f.Read(64); f.Close(); return data.Size() == 64; };
    bool ok1 = false, ok2 = false;
    std::thread t1([&]() { ok1 = read(); }), t2([&]() { ok2 = read(); }); t1.join(); t2.join();
    check(ok1 && ok2, "concurrent handles read safely");
    Common::MountReadOnlyFileSystem({}, {});
    return failures == 0 ? 0 : 1;
}
