#include "common/file.h"
#include "common/readOnlyFileSystem.h"

#include "common/assert.h"
#include "common/common.h"
#include "common/platform/sysFileIO.h"
#include "common/platform/sysTimer.h"
#include "common/stringUtils.h"

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <utility>
#include <vector>

// IWYU pragma: no_include <fileapi.h>
// IWYU pragma: no_include <windows.h>
// IWYU pragma: no_include <winbase.h>

#if KYTY_PLATFORM == KYTY_PLATFORM_WINDOWS
#ifdef CreateDirectory
#undef CreateDirectory
#endif
#ifdef DeleteFile
#undef DeleteFile
#endif
#ifdef CopyFile
#undef CopyFile
#endif
#ifdef MoveFile
#undef MoveFile
#endif
#endif

namespace Common {

namespace {

SysFileTimeStruct DateTimeToFileTimeUtc(const DateTime& date_time) {
	SysTimeStruct time {};
	time.is_invalid = false;

	int year  = 0;
	int month = 0;
	int day   = 0;
	date_time.GetDate().Get(&year, &month, &day);
	time.Year  = year;
	time.Month = month;
	time.Day   = day;

	int hour         = 0;
	int minute       = 0;
	int second       = 0;
	int milliseconds = 0;
	date_time.GetTime().Get(&hour, &minute, &second, &milliseconds);
	time.Hour         = hour;
	time.Minute       = minute;
	time.Second       = second;
	time.Milliseconds = milliseconds;

	SysFileTimeStruct file_time {};
	SysSystemToFileTimeUtc(time, file_time);
	return file_time;
}

} // namespace

struct File::FilePrivate {
	sys_file_t* f;
	std::shared_ptr<ReadOnlyFileSystem> volume;
	std::string relative;
	uint64_t virtual_size = 0;
	uint64_t virtual_offset = 0;
};

File::File(): m_p(std::make_unique<FilePrivate>()) {
	m_p->f = nullptr;
}

File::~File() {
	EXIT_IF(m_p->f != nullptr);
}

File::File(const std::filesystem::path& name): m_p(std::make_unique<FilePrivate>()) {
	m_p->f = nullptr;

	Create(name);
}

File::File(const std::filesystem::path& name, Mode mode): m_p(std::make_unique<FilePrivate>()) {
	m_p->f = nullptr;

	Open(name, mode);
}

bool File::IsInvalid() const {
	return m_p->f == nullptr && !m_p->volume;
}

bool File::Create(const std::filesystem::path& name) {
	EXIT_IF(m_p->f != nullptr);

	m_file_name = name;
	std::string relative;
	if (FindReadOnlyFileSystem(name, relative)) return false;

	m_p->f = SysFileCreate(name);

	if (SysFileIsError(*m_p->f)) {
		Close();
		return false;
	}

	return true;
}

bool File::Open(const std::filesystem::path& name, Mode mode) {
	EXIT_IF(m_p->f != nullptr);

	m_file_name = name;
	if (auto volume = FindReadOnlyFileSystem(name, m_p->relative)) {
		bool directory = false;
		if (mode != Mode::Read || !volume->Stat(m_p->relative, m_p->virtual_size, directory) || directory) return false;
		m_p->volume = std::move(volume);
		m_p->virtual_offset = 0;
		return true;
	}

	switch (mode) {
		case Mode::Read: m_p->f = SysFileOpenR(name); break;
		case Mode::Write: m_p->f = SysFileOpenW(name); break;
		case Mode::ReadWrite:
		case Mode::WriteRead: m_p->f = SysFileOpenRw(name); break;
	}

	if (SysFileIsError(*m_p->f)) {
		Close();
		return false;
	}

	return true;
}

bool File::OpenInMem(void* buf, uint32_t buf_size) {
	EXIT_IF(m_p->f != nullptr);

	m_p->f = SysFileOpen(static_cast<uint8_t*>(buf), buf_size);

	if (SysFileIsError(*m_p->f)) {
		Close();
		return false;
	}

	return true;
}

bool File::OpenInMem(ByteBuffer& buf) {
	return OpenInMem(buf.GetData(), buf.Size());
}

bool File::CreateInMem() {
	EXIT_IF(m_p->f != nullptr);

	m_p->f = SysFileCreate();

	if (SysFileIsError(*m_p->f)) {
		Close();
		return false;
	}

	return true;
}

void File::Close() {
	m_p->volume.reset();
	m_p->relative.clear();
	m_p->virtual_offset = 0;
	if (m_p->f != nullptr) {
		SysFileClose(m_p->f);
		m_p->f = nullptr;
	}
}

uint64_t File::Size() const {
	if (m_p->volume) return m_p->virtual_size;
	EXIT_IF(m_p->f == nullptr);

	if (m_p->f == nullptr) {
		return 0;
	}

	return SysFileSize(*m_p->f);
}

uint64_t File::Remaining() const {
	EXIT_IF(IsInvalid());

	return Size() - std::min(Tell(), Size());
}

uint64_t File::Size(const std::filesystem::path& name) {
	std::string relative;
	if (auto volume = FindReadOnlyFileSystem(name, relative)) {
		uint64_t size = 0; bool directory = false;
		return volume->Stat(relative, size, directory) && !directory ? size : 0;
	}
	return SysFileSize(name);
}

bool File::Seek(uint64_t offset) {
	if (m_p->volume) { m_p->virtual_offset = offset; return true; }
	EXIT_IF(m_p->f == nullptr);

	return SysFileSeek(*m_p->f, offset);
}

bool File::Truncate(uint64_t size) {
	if (m_p->volume) return false;
	EXIT_IF(m_p->f == nullptr);

	return SysFileTruncate(*m_p->f, size);
}

bool File::Unlink() {
	if (m_p->volume) return false;
	EXIT_IF(m_p->f == nullptr);

	return SysFileUnlink(*m_p->f, m_file_name);
}

uint64_t File::Tell() const {
	if (m_p->volume) return m_p->virtual_offset;
	EXIT_IF(m_p->f == nullptr);

	return SysFileTell(*m_p->f);
}

void File::Read(void* data, uint32_t size, uint32_t* bytes_read) {
	if (m_p->volume) {
		const auto available = m_p->virtual_size - std::min(m_p->virtual_offset, m_p->virtual_size);
		const auto take = static_cast<uint32_t>(std::min<uint64_t>(size, available));
		const bool ok = take == 0 || m_p->volume->Read(m_p->relative, m_p->virtual_offset, data, take);
		if (bytes_read != nullptr) *bytes_read = ok ? take : 0;
		if (ok) m_p->virtual_offset += take;
		else { ::printf("Image read failed: %s\n", m_p->relative.c_str()); m_p->volume.reset(); }
		return;
	}
	EXIT_IF(m_p->f == nullptr);

	if (m_p->f != nullptr) {
		SysFileRead(data, size, *m_p->f, bytes_read);
	}
}

void File::Write(const void* data, uint32_t size, uint32_t* bytes_written) {
	if (m_p->volume) { if (bytes_written != nullptr) *bytes_written = 0; return; }
	EXIT_IF(m_p->f == nullptr);

	SysFileWrite(data, size, *m_p->f, bytes_written);
}

static std::string FileVformat(const char* format, va_list args) {
	va_list copy {};
	va_copy(copy, args);
	const int size = std::vsnprintf(nullptr, 0, format, copy);
	va_end(copy);

	if (size <= 0) {
		return {};
	}

	std::vector<char> buffer(static_cast<size_t>(size) + 1u);
	va_list           copy2 {};
	va_copy(copy2, args);
	std::vsnprintf(buffer.data(), buffer.size(), format, copy2);
	va_end(copy2);

	return {buffer.data(), static_cast<size_t>(size)};
}

void File::Printf(const char* format, ...) {
	va_list args {};
	va_start(args, format);

	auto s = FileVformat(format, args);

	va_end(args);

	s = Common::ReplaceStr(s, "\n", "\r\n");
	Write(s.data(), static_cast<uint32_t>(s.size()));
}

bool File::IsDirectoryExisting(const std::filesystem::path& path) {
	std::string relative;
	if (auto volume = FindReadOnlyFileSystem(path, relative)) {
		uint64_t size = 0; bool directory = false;
		return volume->Stat(relative, size, directory) && directory;
	}
	auto path_str = PathToGenericString(path);
	return SysFileIsDirectoryExisting(
	    Common::EndsWith(path_str, "/") ? Common::RemoveLast(path_str, 1) : path_str);
}

bool File::IsFileExisting(const std::filesystem::path& name) {
	std::string relative;
	if (auto volume = FindReadOnlyFileSystem(name, relative)) {
		uint64_t size = 0; bool directory = false;
		return volume->Stat(relative, size, directory) && !directory;
	}
	return SysFileIsFileExisting(name);
}

bool File::CreateDirectory(
    const std::filesystem::path& path) // @suppress("Member declaration not found")
{
	auto path_str = PathToGenericString(path);
	return SysFileCreateDirectory(Common::EndsWith(path_str, "/") ? Common::RemoveLast(path_str, 1)
	                                                              : path_str);
}

bool File::DeleteDirectory(const std::filesystem::path& path) {
	auto path_str = PathToGenericString(path);
	return SysFileDeleteDirectory(Common::EndsWith(path_str, "/") ? Common::RemoveLast(path_str, 1)
	                                                              : path_str);
}

bool File::CreateDirectories(const std::filesystem::path& path) {
	std::string real_path = Common::ReplaceChar(PathToGenericString(path), '\\', '/');

	std::vector<std::string> list = Common::Split(real_path, "/");

	std::string p;

	for (uint32_t si = 0; si < list.size(); si++) {
		const std::string& s = list[si];

		if (si != 0 || Common::StartsWith(real_path, "/")) {
			p += "/";
		}

		p += s;

		if (IsDirectoryExisting(p)) {
			continue;
		}

		if (!CreateDirectory(p)) // @suppress("Invalid arguments")
		{
			return false;
		}
	}

	return true;
}

bool File::DeleteDirectories(const std::filesystem::path& path) {
	std::string real_path = Common::ReplaceChar(PathToGenericString(path), '\\', '/');

	std::vector<std::string> list = Common::Split(real_path, "/");

	std::string              p;
	std::vector<std::string> list2;

	for (uint32_t si = 0; si < list.size(); si++) {
		const std::string& s = list[si];

		if (si != 0 || Common::StartsWith(real_path, "/")) {
			p += "/";
		}

		p += s;

		list2.push_back(p);
	}

	uint32_t num = list2.size();

	for (uint32_t si = num - 1; si < num; si--) {
		if (!DeleteDirectory(list2[si])) {
			return false;
		}
	}

	return true;
}

bool File::DeleteFile(
    const std::filesystem::path& name) // @suppress("Member declaration not found")
{
	return SysFileDeleteFile(name);
}

bool File::Flush() {
	if (m_p->volume) return true;
	EXIT_IF(m_p->f == nullptr);

	return SysFileFlush(*m_p->f);
}

ByteBuffer File::ReadWholeBuffer() {
	EXIT_IF(IsInvalid());
	EXIT_IF(Tell() != 0);

	uint64_t s = Size();

	EXIT_IF((s >> 32u) != 0);

	ByteBuffer buf(s);

	Read(buf.GetData(), s);

	return buf;
}

ByteBuffer File::Read(uint32_t size) {
	ByteBuffer buf(size);
	uint32_t   b = 0;
	Read(buf.GetData(), size, &b);
	buf.RemoveAt(b, size - b);
	return buf;
}

void File::Write(const ByteBuffer& buf, uint32_t* bytes_written) {
	Write(buf.GetDataConst(), buf.Size(), bytes_written);
}

DateTime File::GetLastAccessTimeUTC(const std::filesystem::path& name) {
	std::string relative;
	if (FindReadOnlyFileSystem(name, relative)) return DateTime::FromSystemUTC();
	SysTimeStruct t {};
	SysFileToSystemTimeUtc(SysFileGetLastAccessTimeUtc(name), t);

	if (t.is_invalid) {
		return {};
	}

	return DateTime(Date(t.Year, t.Month, t.Day), Time(t.Hour, t.Minute, t.Second, t.Milliseconds));
}

DateTime File::GetLastWriteTimeUTC(const std::filesystem::path& name) {
	std::string relative;
	if (FindReadOnlyFileSystem(name, relative)) return DateTime::FromSystemUTC();
	SysTimeStruct t {};
	SysFileToSystemTimeUtc(SysFileGetLastWriteTimeUtc(name), t);

	if (t.is_invalid) {
		return {};
	}

	return DateTime(Date(t.Year, t.Month, t.Day), Time(t.Hour, t.Minute, t.Second, t.Milliseconds));
}

void File::GetLastAccessAndWriteTimeUTC(const std::filesystem::path& name, DateTime* access,
                                        DateTime* write) {
	std::string relative;
	if (FindReadOnlyFileSystem(name, relative)) { *access = DateTime::FromSystemUTC(); *write = *access; return; }
	EXIT_IF(access == nullptr);
	EXIT_IF(write == nullptr);

	SysTimeStruct     at {};
	SysTimeStruct     wt {};
	SysFileTimeStruct af {};
	SysFileTimeStruct wf {};

	SysFileGetLastAccessAndWriteTimeUtc(name, af, wf);

	SysFileToSystemTimeUtc(af, at);
	SysFileToSystemTimeUtc(wf, wt);

	if (at.is_invalid || wt.is_invalid) {
		*access = DateTime();
		*write  = DateTime();
	} else {
		*access = DateTime(Date(at.Year, at.Month, at.Day),
		                   Time(at.Hour, at.Minute, at.Second, at.Milliseconds));
		*write  = DateTime(Date(wt.Year, wt.Month, wt.Day),
		                   Time(wt.Hour, wt.Minute, wt.Second, wt.Milliseconds));
	}
}

void File::GetLastAccessAndWriteTimeUTC(DateTime* access, DateTime* write) {
	if (m_p->volume) { *access = DateTime::FromSystemUTC(); *write = *access; return; }
	EXIT_IF(access == nullptr);
	EXIT_IF(write == nullptr);

	EXIT_IF(m_p->f == nullptr);

	if (m_p->f == nullptr) {
		*access = DateTime();
		*write  = DateTime();
		return;
	}

	SysTimeStruct     at {};
	SysTimeStruct     wt {};
	SysFileTimeStruct af {};
	SysFileTimeStruct wf {};

	SysFileGetLastAccessAndWriteTimeUtc(*m_p->f, af, wf);

	SysFileToSystemTimeUtc(af, at);
	SysFileToSystemTimeUtc(wf, wt);

	if (at.is_invalid || wt.is_invalid) {
		*access = DateTime();
		*write  = DateTime();
	} else {
		*access = DateTime(Date(at.Year, at.Month, at.Day),
		                   Time(at.Hour, at.Minute, at.Second, at.Milliseconds));
		*write  = DateTime(Date(wt.Year, wt.Month, wt.Day),
		                   Time(wt.Hour, wt.Minute, wt.Second, wt.Milliseconds));
	}
}

bool File::SetLastAccessTimeUTC(const std::filesystem::path& name, const DateTime& dt) {
	auto f = DateTimeToFileTimeUtc(dt);
	return SysFileSetLastAccessTimeUtc(name, f);
}

bool File::SetLastWriteTimeUTC(const std::filesystem::path& name, const DateTime& dt) {
	auto f = DateTimeToFileTimeUtc(dt);
	return SysFileSetLastWriteTimeUtc(name, f);
}

bool File::SetLastAccessAndWriteTimeUTC(const std::filesystem::path& name, const DateTime& access,
                                        const DateTime& write) {
	auto af = DateTimeToFileTimeUtc(access);
	auto wf = DateTimeToFileTimeUtc(write);
	return SysFileSetLastAccessAndWriteTimeUtc(name, af, wf);
}

std::vector<File::FindInfo> File::FindFiles(const std::filesystem::path& path) {
	std::vector<sys_file_find_t> files;

	SysFileFindFiles(path, files);

	auto     path_str = PathToGenericString(path);
	uint32_t len      = static_cast<uint32_t>(path_str.size());

	if (!Common::EndsWith(path_str, "/") && !Common::EndsWith(path_str, "\\")) {
		len++;
	}

	std::vector<File::FindInfo> ret;
	ret.reserve(files.size());

	for (const auto& f: files) {
		File::FindInfo r {};

		r.path_with_name     = f.path_with_name;
		r.rel_path_with_name = Common::Mid(PathToGenericString(f.path_with_name), len);
		r.size               = f.size;

		SysTimeStruct at {};
		SysTimeStruct wt {};

		SysFileToSystemTimeUtc(f.last_access_time, at);
		SysFileToSystemTimeUtc(f.last_write_time, wt);

		if (!at.is_invalid && !wt.is_invalid) {
			r.last_access_time = DateTime(Date(at.Year, at.Month, at.Day),
			                              Time(at.Hour, at.Minute, at.Second, at.Milliseconds));
			r.last_write_time  = DateTime(Date(wt.Year, wt.Month, wt.Day),
			                              Time(wt.Hour, wt.Minute, wt.Second, wt.Milliseconds));
		}

		// printf("%s, %s, %" PRIu64", %s, %s\n", r.path_with_name.c_str(),
		// r.rel_path_with_name.c_str(), r.size, r.last_access_time.ToString().c_str(),
		// r.last_write_time.ToString().c_str());

		ret.push_back(std::move(r));
	}

	return ret;
}

std::vector<File::DirEntry> File::GetDirEntries(const std::filesystem::path& path) {
	std::string relative;
	if (auto volume = FindReadOnlyFileSystem(path, relative)) {
		std::vector<DirEntry> result;
		for (const auto& entry: volume->List(relative)) result.push_back({entry.name, entry.is_file});
		return result;
	}
	std::vector<sys_dir_entry_t> files;

	SysFileGetDents(path, files);

	std::vector<File::DirEntry> ret;
	ret.reserve(files.size());

	for (const auto& f: files) {
		File::DirEntry r {};

		r.name    = f.name;
		r.is_file = f.is_file;

		ret.push_back(std::move(r));
	}

	return ret;
}

bool File::CopyFile(const std::filesystem::path& src,
                    const std::filesystem::path& dst) // @suppress("Member declaration not found")
{
	return SysFileCopyFile(src, dst);
}

bool File::MoveFile(const std::filesystem::path& src,
                    const std::filesystem::path& dst) // @suppress("Member declaration not found")
{
	if (IsFileExisting(dst)) {
		DeleteFile(dst); // @suppress("Invalid arguments")
	}

	return SysFileMoveFile(src, dst);
}

void File::RemoveReadonly(const std::filesystem::path& name) {
	SysFileRemoveReadonly(name);
}

} // namespace Common
