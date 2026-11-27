// Used Phobos std.stdio File/LockingTextWriter provider.
// Copyright The D Language Foundation 2007-.
// Authors: Walter Bright, Andrei Alexandrescu, Alex Rønne Petersen.
// Boost Software License 1.0; see licenses/phobos-stdio.txt.
// Phobos selects the GNU strerror_r ABI on Glibc/UClibc. Request its real
// declaration before any system header; other POSIX CRTs retain their XSI ABI.
#if defined(__linux__) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif
#if defined(_WIN32) && !defined(NOMINMAX)
#define NOMINMAX
#endif
#include "phobos_stdio.hpp"
#include "phobos_conv_exception.hpp"
#include "array_range_error.hpp"
#include "assert_error.hpp"
#include "phobos_unicode_cursor.hpp"
#include "scope_exit.hpp"
#include <array>
#include <bit>
#include <charconv>
#include <cwchar>
#include <limits>
#include <cerrno>
#include <cstring>
#include <cstdlib>
#include <stdexcept>
#include <utility>
#ifdef _WIN32
#include <Windows.h>
#include <io.h>
#include <fcntl.h>
#else
#include <unistd.h>
#include <sys/select.h>
#endif

namespace nijigenerate::providers::mcp::stdio {
namespace {
#if !defined(_WIN32)
[[maybe_unused]] ::nicxlive::core::d_runtime::RetainedString errnoStringResult(int status, const char* buffer) {
    if (status != 0) return ::nicxlive::core::d_runtime::RetainedString::fromStaticLiteral("Unknown error");
    return ::nicxlive::core::d_runtime::RetainedString(std::string_view(buffer, std::strlen(buffer)));
}
[[maybe_unused]] ::nicxlive::core::d_runtime::RetainedString errnoStringResult(const char* text, const char*) {
    return ::nicxlive::core::d_runtime::RetainedString(std::string_view(text, std::strlen(text)));
}
#endif
::nicxlive::core::d_runtime::RetainedString errnoString(int error) {
#if defined(_WIN32)
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4996)
#endif
    const auto* text = std::strerror(error);
#if defined(_MSC_VER)
#pragma warning(pop)
#endif
    return ::nicxlive::core::d_runtime::RetainedString(std::string_view(text, std::strlen(text)));
#else
    char buffer[1024];
    return errnoStringResult(::strerror_r(error, buffer, sizeof(buffer)), buffer);
#endif
}

#if !defined(_WIN32)
void appendReadlnScalar(std::string& output, std::uint32_t scalar) {
    if ((scalar >= 0xd800 && scalar <= 0xdfff) || scalar > 0x10ffff) {
        auto message = scalar <= 0xffff
            ? ::nicxlive::core::d_runtime::RetainedString::fromStaticLiteral("Encoding a surrogate code point in UTF-8")
            : ::nicxlive::core::d_runtime::RetainedString::fromStaticLiteral("Encoding an invalid code point in UTF-8");
        ::nicxlive::core::d_runtime::UTFException error(std::move(message),
            ::nicxlive::core::d_runtime::RetainedString::fromStaticLiteral("std/utf.d"), 2311);
        error.setSequence(std::span<const std::uint32_t>(&scalar, 1));
        throw error;
    }
    char bytes[4];
    std::size_t count;
    if (scalar < 0x80) { bytes[0] = static_cast<char>(scalar); count = 1; }
    else if (scalar < 0x800) {
        bytes[0] = static_cast<char>(0xc0 | (scalar >> 6));
        bytes[1] = static_cast<char>(0x80 | (scalar & 0x3f)); count = 2;
    } else if (scalar < 0x10000) {
        bytes[0] = static_cast<char>(0xe0 | (scalar >> 12));
        bytes[1] = static_cast<char>(0x80 | ((scalar >> 6) & 0x3f));
        bytes[2] = static_cast<char>(0x80 | (scalar & 0x3f)); count = 3;
    } else {
        bytes[0] = static_cast<char>(0xf0 | (scalar >> 18));
        bytes[1] = static_cast<char>(0x80 | ((scalar >> 12) & 0x3f));
        bytes[2] = static_cast<char>(0x80 | ((scalar >> 6) & 0x3f));
        bytes[3] = static_cast<char>(0x80 | (scalar & 0x3f)); count = 4;
    }
    output.append(bytes, count);
}
#endif

struct LiveFileHandle {
    const void* state;
    std::FILE* (*read)(const void*) noexcept;
    std::FILE* get() const noexcept { return read(state); }
    operator std::FILE*() const noexcept { return get(); }
};

class NativeFileLock {
public:
    explicit NativeFileLock(LiveFileHandle file) : file_(file) {
#ifdef _WIN32
        ::_lock_file(file_);
#else
        ::flockfile(file_);
#endif
    }
    NativeFileLock(const NativeFileLock&) = delete;
    NativeFileLock& operator=(const NativeFileLock&) = delete;
    ~NativeFileLock() {
        if (auto* file = file_.get()) {
#ifdef _WIN32
            ::_unlock_file(file);
#else
            ::funlockfile(file);
#endif
        }
    }
private:
    LiveFileHandle file_;
};

[[noreturn]] void ioFailure(const std::string& message, std::size_t line) {
    const int error = errno;
    throw ErrnoException(::nicxlive::core::d_runtime::RetainedString(message), error,
        ::nicxlive::core::d_runtime::RetainedString::fromStaticLiteral("std/stdio.d"), line);
}

template<std::size_t N> [[noreturn]] void ioFailure(const char (&message)[N], std::size_t line) {
    const int error = errno;
    throw ErrnoException(::nicxlive::core::d_runtime::RetainedString::fromStaticLiteral(message), error,
        ::nicxlive::core::d_runtime::RetainedString::fromStaticLiteral("std/stdio.d"), line);
}

// The actual char/string input branch of Phobos LockingTextWriter.
class NativeTextWriter final : public FileTextWriter {
public:
    explicit NativeTextWriter(LiveFileHandle file)
        : file_(file), orientation_(orientation(file)), lock_(file) {}
    void put(std::string_view bytes) override {
        if (orientation_ <= 0) {
            if (std::fwrite(bytes.data(), 1, bytes.size(), file_) != bytes.size())
                ioFailure("Enforcement failed", 3216);
        } else {
            for (unsigned char byte : bytes) put(byte);
        }
    }
    void put(unsigned char byte) override {
        if (orientation_ <= 0) { putByte(byte); return; }
        if (byte <= 0x7f) { putWide(byte); return; }
        if (byte >= 0xc0) { pending_[0] = static_cast<char>(byte); filled_ = 1; return; }
        if (filled_ >= pending_.size()) throw ::nicxlive::core::d_runtime::ngStaticError<::nicxlive::core::d_runtime::ArrayIndexError>(
            filled_, pending_.size(), ::nicxlive::core::d_runtime::RetainedString::fromStaticLiteral("std/stdio.d"), 3246);
        pending_[filled_++] = static_cast<char>(byte);
        const auto first = static_cast<unsigned char>(pending_[0]);
        const auto width = std::countl_one(first);
        if (first == 0xff || width < 2 || width > 4) throw ::nicxlive::core::d_runtime::UTFException(
            ::nicxlive::core::d_runtime::RetainedString::fromStaticLiteral("Invalid UTF-8 sequence"), std::size_t{0},
            ::nicxlive::core::d_runtime::RetainedString::fromStaticLiteral("std/utf.d"), 688);
        if (static_cast<std::size_t>(width) != filled_) return;
        using Cursor = ::nicxlive::core::d_runtime::phobos::detail::UnicodeCursor<std::string_view, char>;
        const auto scalar = Cursor(std::string_view(pending_.data(), filled_)).front();
        if constexpr (sizeof(wchar_t) == 2) {
            if (scalar <= 0xffff) putWide(static_cast<wchar_t>(scalar));
            else {
                putWide(static_cast<wchar_t>(0xd800 + ((scalar - 0x10000) >> 10)));
                putWide(static_cast<wchar_t>(0xdc00 + ((scalar - 0x10000) & 0x3ff)));
            }
        } else putWide(static_cast<wchar_t>(scalar));
        filled_ = 0;
    }
private:
    void putByte(unsigned char byte) {
#ifdef _WIN32
        (void)::_fputc_nolock(byte, file_);
#elif defined(__GLIBC__)
        (void)::fputc_unlocked(byte, file_);
#else
        (void)std::fputc(byte, file_);
#endif
    }
    void putWide(wchar_t value) {
#ifdef _WIN32
        (void)::_fputwc_nolock(value, file_);
#elif defined(__GLIBC__)
        (void)::fputwc_unlocked(value, file_);
#else
        (void)std::fputwc(value, file_);
#endif
    }
    static int orientation(std::FILE* file) {
#ifdef _WIN32
        const auto descriptor = _fileno(file);
        const auto mode = _setmode(descriptor, _O_TEXT);
        if (mode == -1) return 0;
        (void)_setmode(descriptor, mode);
        return (mode & (_O_WTEXT | _O_U16TEXT | _O_U8TEXT)) ? 1 : 0;
#else
        return std::fwide(file, 0);
#endif
    }
    LiveFileHandle file_;
    int orientation_;
    NativeFileLock lock_;
    std::array<char, 4> pending_{static_cast<char>(0xff), static_cast<char>(0xff),
        static_cast<char>(0xff), static_cast<char>(0xff)};
    std::size_t filled_ = 0;
};

} // namespace

ErrnoException::Details ErrnoException::prepare(std::string_view message, int error) {
    const auto number = static_cast<std::uint32_t>(error);
    const auto systemMessage = errnoString(error);
    std::string fullMessage(message);
    fullMessage.append(" (");
    fullMessage.append(systemMessage.view());
    fullMessage.push_back(')');
    return {number, systemMessage, ::nicxlive::core::d_runtime::RetainedString(std::move(fullMessage))};
}
ErrnoException::ErrnoException(::nicxlive::core::d_runtime::RetainedString message, int error,
    ::nicxlive::core::d_runtime::RetainedString sourceFile, std::size_t sourceLine)
    : ErrnoException(prepare(message.view(), error), std::move(sourceFile), sourceLine) {}
ErrnoException::ErrnoException(int error, std::string_view message,
    ::nicxlive::core::d_runtime::RetainedString sourceFile, std::size_t sourceLine)
    : ErrnoException(prepare(message, error), std::move(sourceFile), sourceLine) {}
ErrnoException::ErrnoException(Details details, ::nicxlive::core::d_runtime::RetainedString sourceFile, std::size_t sourceLine)
    : ::nicxlive::core::d_runtime::ThrowableException(std::move(details.fullMessage), std::move(sourceFile), sourceLine),
      errorNumber_(details.number), errnoMessage_(std::move(details.systemMessage)) {}

StdioException::Details StdioException::prepare(::nicxlive::core::d_runtime::RetainedString message, std::uint32_t error) {
    const auto systemMessage = errnoString(std::bit_cast<std::int32_t>(error));
    if (error == 0) return {error, std::move(message)};
    if (message.empty()) return {error, systemMessage};
    std::string fullMessage(message.view());
    fullMessage.append(" (");
    fullMessage.append(systemMessage.view());
    fullMessage.push_back(')');
    return {error, ::nicxlive::core::d_runtime::RetainedString(std::move(fullMessage))};
}
StdioException::StdioException()
    : StdioException(::nicxlive::core::d_runtime::RetainedString{}, static_cast<std::uint32_t>(errno)) {}
StdioException::StdioException(::nicxlive::core::d_runtime::RetainedString message)
    : StdioException(std::move(message), static_cast<std::uint32_t>(errno)) {}
StdioException::StdioException(::nicxlive::core::d_runtime::RetainedString message, std::uint32_t error)
    : StdioException(prepare(std::move(message), error)) {}
StdioException::StdioException(Details details)
    : ::nicxlive::core::d_runtime::ThrowableException(std::move(details.message),
          ::nicxlive::core::d_runtime::RetainedString::fromStaticLiteral("std/stdio.d"), 5309), errorNumber(details.number) {}

struct SharedFile::State {
    enum class Orientation { unknown, narrow, wide };
    std::FILE* handle;
    std::string name;
    bool closeOnLastReference;
    Orientation orientation = Orientation::unknown;

    State(std::FILE* handle, std::string name, bool closeOnLastReference) noexcept
        : handle(handle), name(std::move(name)), closeOnLastReference(closeOnLastReference) {}
    ~State() noexcept { if (handle && closeOnLastReference) (void)std::fclose(handle); }
    void closeHandle() {
        auto* file = std::exchange(handle, nullptr);
        if (!file) return;
        const auto result = std::fclose(file);
        const auto error = errno;
        if (result != 0) throw ErrnoException(error, "Could not close file `" + name + "'",
            ::nicxlive::core::d_runtime::RetainedString::fromStaticLiteral("std/stdio.d"), 693);
    }
    template<std::size_t N> std::FILE* require(const char (&message)[N], std::size_t line = 0) const {
        if (!handle) {
            if (line) throw ::nicxlive::core::d_runtime::ThrowableException(::nicxlive::core::d_runtime::RetainedString::fromStaticLiteral(message),
                ::nicxlive::core::d_runtime::RetainedString::fromStaticLiteral("std/stdio.d"), line);
            throw ::nicxlive::core::d_runtime::ThrowableException(::nicxlive::core::d_runtime::RetainedString::fromStaticLiteral(message));
        }
        return handle;
    }
};

SharedFile& SharedFile::operator=(SharedFile other) {
    state_.swap(other.state_);
    return *this;
}
SharedFile::~SharedFile() noexcept(false) { detach(); }
void SharedFile::detach() {
    if (!state_) return;
    ::nicxlive::core::d_runtime::ngRunWithScopeExit([&] {
        if (state_.use_count() == 1 && state_->closeOnLastReference) state_->closeHandle();
    }, [&] { state_.reset(); });
}

SharedFile::SharedFile(std::FILE* file, std::string name, bool closeOnLastReference) {
    std::unique_ptr<std::FILE, decltype(&std::fclose)> pending(
        closeOnLastReference ? file : nullptr, &std::fclose);
    state_ = std::make_shared<State>(file, std::move(name), closeOnLastReference);
    (void)pending.release();
}

bool SharedFile::eof() const {
    if (!state_) throw ::nicxlive::core::d_runtime::ThrowableException(
        ::nicxlive::core::d_runtime::RetainedString::fromStaticLiteral("Calling eof() against an unopened file."),
        ::nicxlive::core::d_runtime::RetainedString::fromStaticLiteral("std/stdio.d"), 906);

    return std::feof(state_->require("Calling eof() against an unopened file.", 906)) != 0;
}

bool SharedFile::isOpen() const {
    if (!state_) return false;

    return state_->handle != nullptr;
}
std::intptr_t SharedFile::nativeHandle() const {
    if (!state_) return -1;

    if (!state_->handle) return -1;
#ifdef _WIN32
    return _get_osfhandle(_fileno(state_->handle));
#else
    return fileno(state_->handle);
#endif
}
void SharedFile::withTextWriterImpl(void* context, void (*emit)(void*, FileTextWriter&)) {
    if (!state_) throw ::nicxlive::core::d_runtime::ThrowableException(
        ::nicxlive::core::d_runtime::RetainedString::fromStaticLiteral("Attempting to write to closed File"),
        ::nicxlive::core::d_runtime::RetainedString::fromStaticLiteral("std/stdio.d"), 3148);

    (void)state_->require("Attempting to write to closed File", 3148);
    SharedFile retained(*this);
    ::nicxlive::core::d_runtime::ngRunWithScopeExit([&] {
        NativeTextWriter writer({retained.state_.get(), [](const void* state) noexcept {
            return static_cast<const State*>(state)->handle;
        }});
        emit(context, writer);
    }, [&] { retained.detach(); });
}
void SharedFile::write(std::string_view bytes) {
    withTextWriter([&](FileTextWriter& writer) { writer.put(bytes); });
}
bool SharedFile::readAvailable(std::string& bytes) {
    bytes.clear();
    if (!state_) return false;

    if (!state_->handle) return false;
    char buffer[4096];
#ifdef _WIN32
    const auto handle = reinterpret_cast<HANDLE>(_get_osfhandle(_fileno(state_->handle)));
    if (!handle || handle == INVALID_HANDLE_VALUE) return false;
    DWORD available = 0, read = 0;
    if (!PeekNamedPipe(handle, nullptr, 0, nullptr, &available, nullptr) || !available) return false;
    if (!ReadFile(handle, buffer, available < sizeof(buffer) ? available : DWORD(sizeof(buffer)), &read, nullptr)
        || !read) return false;
    bytes.assign(buffer, read);
#else
    const auto fd = fileno(state_->handle);
    if (fd < 0) return false;
    fd_set descriptors;
    FD_ZERO(&descriptors); FD_SET(fd, &descriptors);
    timeval timeout{};
    if (select(fd + 1, &descriptors, nullptr, nullptr, &timeout) <= 0) return false;
    const auto read = ::read(fd, buffer, sizeof(buffer));
    if (read <= 0) return false;
    bytes.assign(buffer, static_cast<std::size_t>(read));
#endif
    return true;
}

std::string SharedFile::readln() {
    auto* const state = state_.get();
    if (!state) throw ::nicxlive::core::d_runtime::ThrowableException(
        ::nicxlive::core::d_runtime::RetainedString::fromStaticLiteral("Attempt to read from an unopened file."),
        ::nicxlive::core::d_runtime::RetainedString::fromStaticLiteral("std/stdio.d"), 1931);
    auto* file = state->require("Attempt to read from an unopened file.", 1931);
#ifndef _WIN32
    if (state->orientation == State::Orientation::unknown) {
        const auto orientation = std::fwide(file, 0);
        if (orientation < 0) state->orientation = State::Orientation::narrow;
        else if (orientation > 0) state->orientation = State::Orientation::wide;
    }
#endif
    std::string result;
#ifdef _WIN32
    // Microsoft's source fwide(mode0) is a no-op; readlnImpl uses byte input.
    ::_lock_file(file);
    ::nicxlive::core::d_runtime::ngRunWithScopeExit([&] {
        int byte;
        while ((byte = ::_fgetc_nolock(file)) != EOF) {
            result.push_back(static_cast<char>(byte));
            if (byte == '\n') return;
        }
        if (std::ferror(file)) throw StdioException();
    }, [&] { ::_unlock_file(file); });
#else
    if (state->orientation == State::Orientation::wide) {
        ::flockfile(file);
        ::nicxlive::core::d_runtime::ngRunWithScopeExit([&] {
            for (;;) {
#if defined(__GLIBC__)
                const auto value = ::fgetwc_unlocked(file);
#else
                const auto value = std::fgetwc(file);
#endif
                if (value == WEOF) break;
                const auto scalar = static_cast<std::uint32_t>(value);
                appendReadlnScalar(result, scalar);
                if (scalar == '\n') break;
            }
            if (std::ferror(file)) throw StdioException();
        }, [&] { ::funlockfile(file); });
    } else {
        // These are the source readlnImpl's actual per-thread CRT buffer fields.
        static thread_local char* line = nullptr;
        static thread_local std::size_t capacity = 0;
        ::nicxlive::core::d_runtime::ngRunWithScopeExit([&] {
            const auto count = ::getdelim(&line, &capacity, '\n', file);
            if (count < 0) {
                if (std::ferror(file)) throw StdioException();
                return;
            }
            result.assign(line, static_cast<std::size_t>(count));
        }, [&] {
            if (capacity > 128 * 1024) {
                std::free(line); line = nullptr; capacity = 0;
            }
        });
    }
#endif
    return result;
}

std::span<char> SharedFile::rawRead(std::span<char> buffer) {
    if (buffer.empty()) return buffer;
    auto* const state = state_.get();
    if (!state) throw ::nicxlive::core::d_runtime::ThrowableException(
        ::nicxlive::core::d_runtime::RetainedString::fromStaticLiteral("Attempting to read from an unopened file"),
        ::nicxlive::core::d_runtime::RetainedString::fromStaticLiteral("std/stdio.d"), 1104);

    auto* file = state->require("Attempting to read from an unopened file", 1104);
#ifdef _WIN32
    struct RestoreMode {
        int descriptor;
        int mode;
        ~RestoreMode() { (void)_setmode(descriptor, mode); }
    };
    const auto descriptor = _fileno(file);
    const RestoreMode restore{descriptor, _setmode(descriptor, _O_BINARY)};
#endif
    const auto count = std::fread(buffer.data(), 1, buffer.size(), file);
#ifndef NDEBUG
    if (count > buffer.size()) throw ::nicxlive::core::d_runtime::ngStaticError<::nicxlive::core::d_runtime::AssertError>(
        ::nicxlive::core::d_runtime::RetainedString::fromStaticLiteral("Assertion failure"),
        ::nicxlive::core::d_runtime::RetainedString::fromStaticLiteral("std/stdio.d"), 1112);
#endif
    if (count != buffer.size() && std::ferror(file)) ioFailure("Enforcement failed", 1115);
    return buffer.first(count);
}

void SharedFile::seek(std::int64_t offset, int origin) {
    using namespace ::nicxlive::core::d_runtime;
    const auto source = RetainedString::fromStaticLiteral("std/stdio.d");
    if (origin != SEEK_SET && origin != SEEK_CUR && origin != SEEK_END)
        throw ThrowableException(RetainedString::fromStaticLiteral(
            "Invalid `origin` argument passed to `seek`, must be one of: SEEK_SET, SEEK_CUR, SEEK_END"), source, 1261);
    if (!isOpen()) throw ThrowableException(
        RetainedString::fromStaticLiteral("Attempting to seek() in an unopened file"), source, 1264);
    auto retained = state_;
#if defined(_WIN32) && defined(_MSC_VER)
    const auto status = ::_fseeki64(retained->handle, offset, origin);
#else
#if defined(_WIN32)
    using SeekOffset = std::int32_t;
#else
    using SeekOffset = ::off_t;
#endif
    if constexpr (std::numeric_limits<SeekOffset>::min() > std::numeric_limits<std::int64_t>::min()) {
        if (offset < std::numeric_limits<SeekOffset>::min())
            throw phobos::conv::ConvOverflowException(RetainedString::fromStaticLiteral("Conversion negative overflow"),
                RetainedString::fromStaticLiteral("std/conv.d"), 1581);
    }
    if constexpr (std::numeric_limits<SeekOffset>::max() < std::numeric_limits<std::int64_t>::max()) {
        if (offset > std::numeric_limits<SeekOffset>::max())
            throw phobos::conv::ConvOverflowException(RetainedString::fromStaticLiteral("Conversion positive overflow"),
                RetainedString::fromStaticLiteral("std/conv.d"), 1587);
    }
#if defined(_WIN32)
    const auto status = std::fseek(retained->handle, static_cast<SeekOffset>(offset), origin);
#else
    const auto status = ::fseeko(retained->handle, static_cast<SeekOffset>(offset), origin);
#endif
#endif
    const auto error = errno;
    if (status != 0) throw ErrnoException(error, "Could not seek in file `" + retained->name + "'", source, 1283);
}

void SharedFile::writeln(std::string_view bytes) {
    withTextWriter([&](FileTextWriter& writer) {
        writer.put(bytes);
        writer.put(static_cast<unsigned char>('\n'));
    });
}

void SharedFile::writeln(std::string_view prefix, std::size_t value) {
    withTextWriter([&](FileTextWriter& writer) {
        writer.put(prefix);
        std::array<char, std::numeric_limits<std::size_t>::digits10 + 1> digits;
        const auto end = std::to_chars(digits.data(), digits.data() + digits.size(), value).ptr;
        writer.put(std::string_view(digits.data(), static_cast<std::size_t>(end - digits.data())));
        writer.put(static_cast<unsigned char>('\n'));
    });
}

void SharedFile::flush() {
    if (!state_) throw ::nicxlive::core::d_runtime::ThrowableException(
        ::nicxlive::core::d_runtime::RetainedString::fromStaticLiteral("Attempting to flush() in an unopened file"),
        ::nicxlive::core::d_runtime::RetainedString::fromStaticLiteral("std/stdio.d"), 1028);

    if (std::fflush(state_->require("Attempting to flush() in an unopened file", 1028)) != 0)
        ioFailure("Enforcement failed", 1029);
}

void SharedFile::setLineBuffering(std::size_t size) {
    if (!state_) throw ::nicxlive::core::d_runtime::ThrowableException(
        ::nicxlive::core::d_runtime::RetainedString::fromStaticLiteral("Attempting to call setvbuf() on an unopened file"),
        ::nicxlive::core::d_runtime::RetainedString::fromStaticLiteral("std/stdio.d"), 1384);

    if (std::setvbuf(state_->require("Attempting to call setvbuf() on an unopened file", 1384),
        nullptr, _IOLBF, size) != 0) ioFailure("Could not set buffering for file `" + state_->name + "'", 1385);
}

void SharedFile::close() {
    auto* const state = state_.get();
    if (!state) return;
    ::nicxlive::core::d_runtime::ngRunWithScopeExit([&] { state->closeHandle(); }, [&] { state_.reset(); });
}

SharedFile& standardInput() {
    static SharedFile input(stdin, "", false);
    return input;
}
SharedFile& standardOutput() {
    static SharedFile output(stdout, "", false);
    return output;
}
SharedFile& standardError() {
    static SharedFile error(stderr, "", false);
    return error;
}

} // namespace nijigenerate::providers::mcp::stdio
