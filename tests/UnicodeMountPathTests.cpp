#include "common/file.h"
#include "common/stringUtils.h"
#include "common/threads.h"
#include "kernel/fileSystem.h"

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

int main() {
    namespace FS = Libs::LibKernel::FileSystem;
    Common::ThreadsSubsystem::Instance()->Init(nullptr);
    FS::FileSystemSubsystem::Instance()->Init(nullptr);
    int failures = 0;
    auto check = [&](bool ok, const char* message) {
        std::printf("%s: %s\n", ok ? "PASS" : "FAIL", message);
        if (!ok) ++failures;
    };
    const auto root = std::filesystem::temp_directory_path() /
        ("kyty-unicode-mount-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    // UTF-8 bytes rather than source/execution character-set dependent literals.
    const std::string folder = "NEW Joe & Mac \xE2\x80\x93 Caveman Ninja PS5 BASE GAME";
    const std::string unicode_file = "caf\xC3\xA9-\xE6\x97\xA5\xE6\x9C\xAC.bin";
    const auto game = root / Common::PathFromUtf8(folder);
    const auto subdir = game / Common::PathFromUtf8("donn\xC3\xA9" "es");
    std::filesystem::create_directories(subdir);
    { std::ofstream file(game / "eboot.bin", std::ios::binary); file << "test eboot"; }
    { std::ofstream file(subdir / Common::PathFromUtf8(unicode_file), std::ios::binary); file << "unicode data"; }
    FS::Mount(game, "/app0");
    FS::Mount(game, "/hostapp");
    check(FS::GetRealFilename("/app0/eboot.bin") == game / "eboot.bin", "en dash mount preserves native path");
    check(FS::GetRealFilename("/hostapp/eboot.bin") == game / "eboot.bin", "hostapp alias preserves native path");
    const std::string relative = "donn\xC3\xA9" "es/" + unicode_file;
    check(FS::GetRealFilename("/app0/" + relative) == subdir / Common::PathFromUtf8(unicode_file), "Unicode guest filename resolves correctly");
    int directory_fd = FS::KernelOpen("/app0/donn\xC3\xA9" "es", 0x20000, 0);
    check(directory_fd >= 3, "Unicode guest directory opens correctly");
    if (directory_fd >= 3) FS::KernelClose(directory_fd);
    Common::File file;
    check(file.Open(FS::GetRealFilename("/app0/eboot.bin"), Common::File::Mode::Read), "loader file-open path works");
    if (!file.IsInvalid()) file.Close();
    int fd = FS::KernelOpen(("/app0/" + relative).c_str(), 0, 0);
    check(fd >= 3, "kernel opens Unicode guest filename");
    if (fd >= 3) {
        char bytes[12] {};
        check(FS::KernelRead(fd, bytes, sizeof(bytes)) == sizeof(bytes) &&
              std::string(bytes, sizeof(bytes)) == "unicode data", "kernel reads exact file contents");
        FS::KernelClose(fd);
    }
    check(FS::GetRealFilename(Common::PathToGenericString(game / "eboot.bin")) == game / "eboot.bin", "unmounted UTF-8 host path resolves correctly");
    directory_fd = FS::KernelOpen(Common::PathToGenericString(subdir).c_str(), 0x20000, 0);
    check(directory_fd >= 3, "unmounted UTF-8 directory opens correctly");
    if (directory_fd >= 3) FS::KernelClose(directory_fd);
    FS::Umount("/app0");
    check(FS::GetRealFilename("/app0/eboot.bin") == std::filesystem::path("/app0/eboot.bin"), "unmount by guest point");
    FS::Umount(Common::PathToGenericString(game));
    check(FS::GetRealFilename("/hostapp/eboot.bin") == std::filesystem::path("/hostapp/eboot.bin"), "unmount by Unicode host directory");
    std::filesystem::remove_all(root);
    return failures ? 1 : 0;
}
