#include "object_metadata.hpp"
#include "source_allocator.hpp"
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <stdexcept>
#include <typeindex>
#include <unordered_map>
#include <utility>

namespace nicxlive::core::d_runtime {
namespace {
struct SourceTypeNames {
    RetainedString className;
    std::optional<RetainedString> templateArgumentName;
    std::optional<RetainedString> unqualifiedArgument;
    std::optional<RetainedString> qualifiedElement;
    std::optional<RetainedString> unqualifiedElement;
};
struct ClassNames {
    std::shared_mutex mutex;
    std::unordered_map<std::type_index, SourceTypeNames, std::hash<std::type_index>,
        std::equal_to<std::type_index>, SourceAllocator<std::pair<const std::type_index, SourceTypeNames>>> names;
};
ClassNames& classNames() {
    // Source declaration metadata survives thread-local objects and model
    // finalizers, including C++ static-destruction ordering.
    static ClassNames* const registry = [] {
        SourceAllocator<ClassNames> allocator;
        auto* value = allocator.allocate(1);
        try {
            // Mutex and map construction are outside memory-failure conversion.
            std::construct_at(value);
        } catch (...) {
            allocator.deallocate(value, 1);
            throw;
        }
        return value;
    }();
    return *registry;
}
void insertNames(ClassNames& registry, const std::type_info& type, SourceTypeNames names) {
    try {
        // These passive keys and retained descriptors invoke no user callbacks.
        registry.names.emplace(type, std::move(names));
    } catch (const std::length_error&) {
        throw ngStaticError<OutOfMemoryError>(std::source_location::current());
    }
}
void requireSpellingContext(SourceTypeSpelling context) {
    switch (context) {
    case SourceTypeSpelling::QualifiedArgument:
    case SourceTypeSpelling::UnqualifiedArgument:
    case SourceTypeSpelling::QualifiedElement:
    case SourceTypeSpelling::UnqualifiedElement:
        return;
    }
    throw std::invalid_argument("Invalid Source type spelling context");
}
const std::optional<RetainedString>& sourceSpellingField(
    const SourceTypeNames& names, SourceTypeSpelling context) {
    switch (context) {
    case SourceTypeSpelling::QualifiedArgument: return names.templateArgumentName;
    case SourceTypeSpelling::UnqualifiedArgument: return names.unqualifiedArgument;
    case SourceTypeSpelling::QualifiedElement: return names.qualifiedElement;
    case SourceTypeSpelling::UnqualifiedElement: return names.unqualifiedElement;
    }
    throw std::invalid_argument("Invalid Source type spelling context");
}
}
void ngRegisterObjectClassName(const std::type_info& type, RetainedString name) {
    if (name.empty()) throw std::invalid_argument("Source runtime class name must not be empty");
    auto& registry = classNames();
    std::unique_lock lock(registry.mutex);
    const auto found = registry.names.find(type);
    if (found != registry.names.end()) {
        if (!found->second.className.empty() && found->second.className.view() != name.view())
            throw std::logic_error("Source runtime class name cannot change after registration");
        if (found->second.className.empty()) found->second.className = std::move(name);
        return;
    }
    insertNames(registry, type, SourceTypeNames{std::move(name), {}});
}
std::optional<RetainedString> ngFindObjectClassName(const std::type_info& type) {
    auto& registry = classNames();
    std::shared_lock lock(registry.mutex);
    const auto found = registry.names.find(type);
    if (found == registry.names.end() || found->second.className.empty()) return {};
    return found->second.className;
}
RetainedString ngObjectClassName(const std::type_info& type) {
    if (auto name = ngFindObjectClassName(type)) return *name;
    throw std::logic_error("Runtime class must register its original fully-qualified source name");
}
void ngRegisterObjectTypeNames(const std::type_info& type, RetainedString className,
                               RetainedString templateArgumentName) {
    if (className.empty()) throw std::invalid_argument("Source runtime class name must not be empty");
    if (templateArgumentName.empty())
        throw std::invalid_argument("Source template type-argument name must not be empty");
    auto& registry = classNames();
    std::unique_lock lock(registry.mutex);
    const auto found = registry.names.find(type);
    if (found != registry.names.end()) {
        if (!found->second.className.empty() && found->second.className.view() != className.view())
            throw std::logic_error("Source runtime class name cannot change after registration");
        if (found->second.templateArgumentName &&
            found->second.templateArgumentName->view() != templateArgumentName.view())
            throw std::logic_error("Source template type-argument name cannot change after registration");
        // Check both fields before extending either kind of partial entry.
        if (found->second.className.empty()) found->second.className = std::move(className);
        if (!found->second.templateArgumentName)
            found->second.templateArgumentName = std::move(templateArgumentName);
        return;
    }
    insertNames(registry, type, SourceTypeNames{std::move(className), std::move(templateArgumentName)});
}
void ngRegisterSourceTypeArgumentName(const std::type_info& type, RetainedString name) {
    if (name.empty()) throw std::invalid_argument("Source template type-argument name must not be empty");
    auto& registry = classNames();
    std::unique_lock lock(registry.mutex);
    const auto found = registry.names.find(type);
    if (found != registry.names.end()) {
        if (found->second.templateArgumentName && found->second.templateArgumentName->view() != name.view())
            throw std::logic_error("Source template type-argument name cannot change after registration");
        if (!found->second.templateArgumentName) found->second.templateArgumentName = std::move(name);
        return;
    }
    insertNames(registry, type, SourceTypeNames{{}, std::move(name)});
}
void ngRegisterSourceTypeSpellings(const std::type_info& type, SourceTypeSpellingNames names) {
    if (names.qualifiedArgument.empty() || names.unqualifiedArgument.empty() ||
        names.qualifiedElement.empty() || names.unqualifiedElement.empty())
        throw std::invalid_argument("Source type spellings must not be empty");
    auto& registry = classNames();
    std::unique_lock lock(registry.mutex);
    const auto found = registry.names.find(type);
    if (found != registry.names.end()) {
        const auto check = [](const auto& old, const RetainedString& incoming) {
            if (old && old->view() != incoming.view())
                throw std::logic_error("Source type spellings cannot change after registration");
        };
        auto& entry = found->second;
        check(entry.templateArgumentName, names.qualifiedArgument);
        check(entry.unqualifiedArgument, names.unqualifiedArgument);
        check(entry.qualifiedElement, names.qualifiedElement);
        check(entry.unqualifiedElement, names.unqualifiedElement);
        // Every conflict was checked before filling any missing field.
        if (!entry.templateArgumentName) entry.templateArgumentName = std::move(names.qualifiedArgument);
        if (!entry.unqualifiedArgument) entry.unqualifiedArgument = std::move(names.unqualifiedArgument);
        if (!entry.qualifiedElement) entry.qualifiedElement = std::move(names.qualifiedElement);
        if (!entry.unqualifiedElement) entry.unqualifiedElement = std::move(names.unqualifiedElement);
        return;
    }
    insertNames(registry, type, SourceTypeNames{{}, std::move(names.qualifiedArgument),
        std::move(names.unqualifiedArgument), std::move(names.qualifiedElement), std::move(names.unqualifiedElement)});
}
std::optional<RetainedString> ngFindSourceTypeSpelling(
    const std::type_info& type, SourceTypeSpelling context) {
    requireSpellingContext(context);
    auto& registry = classNames();
    std::shared_lock lock(registry.mutex);
    const auto found = registry.names.find(type);
    if (found == registry.names.end()) return {};
    return sourceSpellingField(found->second, context);
}
RetainedString ngSourceTypeSpelling(const std::type_info& type, SourceTypeSpelling context) {
    if (auto name = ngFindSourceTypeSpelling(type, context)) return *name;
    throw std::logic_error("Runtime type must register its exact Source spelling for the requested context");
}
std::optional<RetainedString> ngFindObjectTemplateArgumentName(const std::type_info& type) {
    auto& registry = classNames();
    std::shared_lock lock(registry.mutex);
    const auto found = registry.names.find(type);
    if (found == registry.names.end()) return {};
    return found->second.templateArgumentName;
}
RetainedString ngObjectTemplateArgumentName(const std::type_info& type) {
    if (auto name = ngFindObjectTemplateArgumentName(type)) return *name;
    throw std::logic_error("Runtime type must register its original source template type-argument spelling");
}
}
