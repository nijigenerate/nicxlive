#pragma once
// Used Phobos std.stdio File/LockingTextWriter provider.
// Copyright The D Language Foundation 2007-.
// Authors: Walter Bright, Andrei Alexandrescu, Alex Rønne Petersen.
// Boost Software License 1.0; see licenses/phobos-stdio.txt.

#include "throwable.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace nijigenerate::providers::mcp::stdio {
class ErrnoException : public ::nicxlive::core::d_runtime::ThrowableException {
public:
    ErrnoException(::nicxlive::core::d_runtime::RetainedString message, int error,
        ::nicxlive::core::d_runtime::RetainedString sourceFile = {}, std::size_t sourceLine = 0);
    // Preserve existing native File call expressions with the canonical type.
    ErrnoException(int error, std::string_view message,
        ::nicxlive::core::d_runtime::RetainedString sourceFile = {}, std::size_t sourceLine = 0);
    std::uint32_t errorNumber() const noexcept { return errorNumber_; }
    ::nicxlive::core::d_runtime::RetainedString errnoMessage() const noexcept { return errnoMessage_; }
    std::string_view ngThrowableTypeName() const noexcept override { return "std.exception.ErrnoException"; }
private:
    struct Details {
        std::uint32_t number;
        ::nicxlive::core::d_runtime::RetainedString systemMessage;
        ::nicxlive::core::d_runtime::RetainedString fullMessage;
    };
    static Details prepare(std::string_view message, int error);
    ErrnoException(Details details, ::nicxlive::core::d_runtime::RetainedString sourceFile, std::size_t sourceLine);
    std::uint32_t errorNumber_;
    ::nicxlive::core::d_runtime::RetainedString errnoMessage_;
};
class StdioException : public ::nicxlive::core::d_runtime::ThrowableException {
public:
    StdioException();
    explicit StdioException(::nicxlive::core::d_runtime::RetainedString message);
    StdioException(::nicxlive::core::d_runtime::RetainedString message, std::uint32_t error);
    std::uint32_t errorNumber;
    std::string_view ngThrowableTypeName() const noexcept override { return "std.stdio.StdioException"; }
private:
    struct Details {
        std::uint32_t number;
        ::nicxlive::core::d_runtime::RetainedString message;
    };
    static Details prepare(::nicxlive::core::d_runtime::RetainedString message, std::uint32_t error);
    explicit StdioException(Details details);
};

// The actual locked writer is borrowed only during SharedFile::withTextWriter.
class FileTextWriter {
public:
    virtual void put(std::string_view bytes) = 0;
    virtual void put(unsigned char byte) = 0;
protected:
    ~FileTextWriter() = default;
};

// Copies share the live FILE handle; explicitly closing any copy closes every alias.
class SharedFile {
public:
    SharedFile() = default;
    ~SharedFile() noexcept(false);
    SharedFile(const SharedFile&) = default;
    SharedFile& operator=(SharedFile other);
    SharedFile(SharedFile&& other) noexcept = default;
    explicit SharedFile(std::FILE* file, std::string name = {}, bool closeOnLastReference = true);
    bool eof() const;
    bool isOpen() const;
    void write(std::string_view bytes);
    template<class Emit>
        requires (std::is_object_v<std::remove_reference_t<Emit>> &&
            !std::is_volatile_v<std::remove_reference_t<Emit>> &&
            std::is_invocable_v<Emit&, FileTextWriter&>)
    void withTextWriter(Emit&& emit) {
        using Operation = std::remove_reference_t<Emit>;
        auto* operation = const_cast<void*>(static_cast<const void*>(std::addressof(emit)));
        withTextWriterImpl(operation, [](void* context, FileTextWriter& writer) {
            std::invoke(*static_cast<Operation*>(context), writer);
        });
    }
    bool readAvailable(std::string& bytes);
    std::intptr_t nativeHandle() const;
    std::string readln();
    std::span<char> rawRead(std::span<char> buffer);
    void writeln(std::string_view bytes);
    void writeln(std::string_view prefix, std::size_t value);
    void seek(std::int64_t offset, int origin = SEEK_SET);
    void flush();
    void setLineBuffering(std::size_t size);
    void close();
private:
    void detach();
    void withTextWriterImpl(void* context, void (*emit)(void*, FileTextWriter&));
    struct State;
    std::shared_ptr<State> state_;
};

SharedFile& standardInput();
SharedFile& standardOutput();
SharedFile& standardError();

} // namespace nijigenerate::providers::mcp::stdio

// SDK consumers use the same actual types, streams and locked writer.
namespace nicxlive::core::d_runtime::phobos::stdio {
using ::nijigenerate::providers::mcp::stdio::ErrnoException;
using ::nijigenerate::providers::mcp::stdio::StdioException;
using ::nijigenerate::providers::mcp::stdio::FileTextWriter;
using ::nijigenerate::providers::mcp::stdio::SharedFile;
using ::nijigenerate::providers::mcp::stdio::standardInput;
using ::nijigenerate::providers::mcp::stdio::standardOutput;
using ::nijigenerate::providers::mcp::stdio::standardError;
}
