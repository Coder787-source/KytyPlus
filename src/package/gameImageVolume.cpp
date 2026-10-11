// SPDX-License-Identifier: MIT
#include "package/gameImageVolume.h"
#include <algorithm>
#include <cctype>
#include <cstring>
#include <map>
#include <mutex>
#include <stdexcept>
#include <limits>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#else
#include <cerrno>
#include <csignal>
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>
#endif

namespace Libs::Firmware {
namespace {
std::string Key(std::string path) {
    std::replace(path.begin(), path.end(), '\\', '/');
    while (!path.empty() && path.back() == '/') path.pop_back();
    std::string result;
    size_t begin = 0;
    while (begin < path.size()) {
        auto end = path.find('/', begin);
        auto component = path.substr(begin, end == std::string::npos ? end : end - begin);
        if (component.empty() || component == ".." || component.find(':') != std::string::npos) return "!invalid";
        if (component != ".") { if (!result.empty()) result += '/'; result += component; }
        if (end == std::string::npos) break;
        begin = end + 1;
    }
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return result;
}

class ImageVolume final : public Common::ReadOnlyFileSystem {
    struct Node { uint32_t index; uint64_t size; bool directory; std::string name; };
    std::map<std::string, Node> nodes;
    std::map<std::string, std::vector<Entry>> children;
    std::mutex mutex;
#ifdef _WIN32
    HANDLE read_pipe = nullptr, write_pipe = nullptr, child = nullptr;
#else
    int read_pipe = -1, write_pipe = -1;
    pid_t child = -1;
#endif
    bool Receive(void* target, uint32_t size) {
        auto* bytes = static_cast<uint8_t*>(target);
        while (size != 0) {
#ifdef _WIN32
            DWORD got = 0;
            if (!ReadFile(read_pipe, bytes, size, &got, nullptr) || got == 0) return false;
#else
            auto got = ::read(read_pipe, bytes, size);
            if (got < 0 && errno == EINTR) continue;
            if (got <= 0) return false;
#endif
            bytes += got; size -= static_cast<uint32_t>(got);
        }
        return true;
    }
    bool Send(const void* target, uint32_t size) {
        auto* bytes = static_cast<const uint8_t*>(target);
        while (size != 0) {
#ifdef _WIN32
            DWORD sent = 0;
            if (!WriteFile(write_pipe, bytes, size, &sent, nullptr) || sent == 0) return false;
#else
            auto sent = ::write(write_pipe, bytes, size);
            if (sent < 0 && errno == EINTR) continue;
            if (sent <= 0) return false;
#endif
            bytes += sent; size -= static_cast<uint32_t>(sent);
        }
        return true;
    }
    std::string failure;
    bool Fail(const char* message) { failure = message; return false; }
    template<typename T> T Number() { T value{}; if (!Receive(&value, sizeof(value))) failure = "Game-image helper ended before its index was ready"; return value; }
public:
    ~ImageVolume() override {
#ifdef _WIN32
        if (write_pipe) CloseHandle(write_pipe);
        if (read_pipe) CloseHandle(read_pipe);
        if (child) { if (WaitForSingleObject(child, 1000) == WAIT_TIMEOUT) { TerminateProcess(child, 1); WaitForSingleObject(child, 1000); } CloseHandle(child); }
#else
        if (write_pipe >= 0) ::close(write_pipe);
        if (read_pipe >= 0) ::close(read_pipe);
        if (child > 0) { if (waitpid(child, nullptr, WNOHANG) == 0) { kill(child, SIGTERM); waitpid(child, nullptr, 0); } }
#endif
    }
    bool Start(const std::filesystem::path& image, const std::filesystem::path& helper,
               const std::function<void(uint32_t,uint32_t)>& progress) {
#ifdef _WIN32
        SECURITY_ATTRIBUTES security {sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
        HANDLE input = nullptr, output = nullptr;
        if (!CreatePipe(&input, &write_pipe, &security, 0)) return Fail("Cannot create image input pipe");
        if (!CreatePipe(&read_pipe, &output, &security, 0)) { CloseHandle(input); return Fail("Cannot create image output pipe"); }
        SetHandleInformation(write_pipe, HANDLE_FLAG_INHERIT, 0);
        SetHandleInformation(read_pipe, HANDLE_FLAG_INHERIT, 0);
        HANDLE error = CreateFileW(L"NUL", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &security, OPEN_EXISTING, 0, nullptr);
        // Attribute-list inheritance restricts the child to these three handles.
        SIZE_T bytes = 0; InitializeProcThreadAttributeList(nullptr, 1, 0, &bytes);
        std::vector<uint8_t> attributes(bytes);
        auto* list = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attributes.data());
        if (!InitializeProcThreadAttributeList(list, 1, 0, &bytes)) { CloseHandle(input); CloseHandle(output); if (error != INVALID_HANDLE_VALUE) CloseHandle(error); return Fail("Cannot configure image helper"); }
        HANDLE handles[] {input, output, error};
        bool configured = error != INVALID_HANDLE_VALUE && UpdateProcThreadAttribute(list, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, handles, sizeof(handles), nullptr, nullptr);
        STARTUPINFOEXW startup{}; startup.StartupInfo.cb = sizeof(startup); startup.lpAttributeList = list;
        startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
        startup.StartupInfo.hStdInput = input; startup.StartupInfo.hStdOutput = output; startup.StartupInfo.hStdError = error;
        PROCESS_INFORMATION process{};
        std::wstring command = L"\"" + helper.wstring() + L"\" --serve-image \"" + image.wstring() + L"\"";
        bool ok = configured && CreateProcessW(helper.c_str(), command.data(), nullptr, nullptr, TRUE,
            CREATE_NO_WINDOW | EXTENDED_STARTUPINFO_PRESENT, nullptr, nullptr, &startup.StartupInfo, &process);
        DeleteProcThreadAttributeList(list); CloseHandle(input); CloseHandle(output); if (error != INVALID_HANDLE_VALUE) CloseHandle(error);
        if (!ok) return Fail("Cannot start the game-image helper (check naps/ and the .NET runtime)");
        child = process.hProcess; CloseHandle(process.hThread);
#else
        int in[2], out[2];
        if (pipe(in) != 0) return Fail("Cannot create image input pipe");
        if (pipe(out) != 0) { close(in[0]); close(in[1]); return Fail("Cannot create image output pipe"); }
        child = fork();
        if (child == 0) {
            dup2(in[0], STDIN_FILENO); dup2(out[1], STDOUT_FILENO);
            close(in[0]); close(in[1]); close(out[0]); close(out[1]);
            execl(helper.c_str(), helper.c_str(), "--serve-image", image.c_str(), static_cast<char*>(nullptr)); _exit(127);
        }
        close(in[0]); close(out[1]); write_pipe = in[1]; read_pipe = out[0];
        if (child < 0) return Fail("Cannot start image helper");
#endif
        char magic[8];
        if (!Receive(magic, 8) || std::memcmp(magic, "KYTYIMG1", 8) != 0) return Fail("Cannot mount game image: helper did not return a valid index");
        uint32_t count = Number<uint32_t>();
        if (count == 0 || count > 1000000) return Fail("Invalid image file count");
        nodes.emplace("", Node{0, 0, true, ""});
        uint64_t names_size = 0;
        for (uint32_t i = 0; i < count; ++i) {
            uint32_t size = Number<uint32_t>(); names_size += size;
            if (size == 0 || size > 16384 || names_size > 128 * 1024 * 1024) return Fail("Image index path limit exceeded");
            std::string name(size, '\0');
            if (!Receive(name.data(), size)) return Fail("Truncated image index");
            uint64_t length = Number<uint64_t>();
            auto key = Key(name);
            if (key == "!invalid" || length > static_cast<uint64_t>(INT64_MAX) || !nodes.emplace(key, Node{i, length, false, name}).second)
                return Fail("Unsafe or duplicate image index path");
            if (progress && (i % 1000 == 0 || i + 1 == count)) progress(i + 1, count);
        }
        // Build directory entries once, avoiding O(file_count) for each readdir.
        std::vector<std::string> files;
        for (const auto& [key, node]: nodes) if (!node.directory) files.push_back(node.name);
        for (const auto& file: files) {
            size_t start = 0; std::string parent;
            while (start < file.size()) {
                auto end = file.find('/', start);
                auto name = file.substr(start, end == std::string::npos ? end : end - start);
                auto key = Key(parent.empty() ? name : parent + '/' + name);
                bool directory = end != std::string::npos;
                if (directory) {
                    auto [it, inserted] = nodes.emplace(key, Node{0,0,true,name});
                    if (!it->second.directory) return Fail("Image file/directory collision");
                    if (inserted) children[Key(parent)].push_back({name, false});
                } else children[Key(parent)].push_back({name, true});
                if (!directory) break;
                parent = parent.empty() ? name : parent + '/' + name;
                start = end + 1;
            }
        }
        if (!nodes.contains("eboot.bin")) return Fail("Game image lacks eboot.bin");
        return failure.empty();
    }
    const std::string& Error() const { return failure; }
    bool Stat(const std::string& path, uint64_t& size, bool& directory) const override {
        auto it = nodes.find(Key(path));
        if (it == nodes.end()) return false;
        size = it->second.size; directory = it->second.directory; return true;
    }
    std::vector<Entry> List(const std::string& path) const override {
        auto it = children.find(Key(path)); return it == children.end() ? std::vector<Entry>{} : it->second;
    }
    bool Read(const std::string& path, uint64_t offset, void* data, uint32_t length) override {
        auto it = nodes.find(Key(path));
        if (it == nodes.end() || it->second.directory || offset > it->second.size || length > it->second.size - offset) return false;
        std::lock_guard lock(mutex);
        auto* target = static_cast<uint8_t*>(data);
        while (length != 0) {
            uint32_t take = std::min(length, 1024u * 1024u);
            if (!Send(&it->second.index, 4) || !Send(&offset, 8) || !Send(&take, 4)) return false;
            int32_t result = 0;
            if (!Receive(&result, 4) || result != static_cast<int32_t>(take) || !Receive(target, take)) return false;
            target += take; offset += take; length -= take;
        }
        return true;
    }
};
}
std::shared_ptr<Common::ReadOnlyFileSystem> OpenGameImage(const std::filesystem::path& image,
    const std::filesystem::path& helper, std::string& error, const std::function<void(uint32_t,uint32_t)>& progress) {
    auto volume = std::make_shared<ImageVolume>();
    if (!volume->Start(image, helper, progress)) { error = volume->Error(); return {}; }
    error.clear(); return volume;
}
} // namespace Libs::Firmware
