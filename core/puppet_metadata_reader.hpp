#pragma once

#include "puppet_metadata.hpp"
#include "serde_reader.hpp"
#include <set>

namespace nicxlive::core::serde {
namespace metadata_detail {
inline ReadError checkDuplicate(const std::string& key, std::set<std::string>& seen, std::string_view typeName,
    std::initializer_list<std::string_view> fields) {
    bool known = false;
    for (const auto name : fields) if (name == key) known = true;
    if (!known || seen.insert(key).second) return {};
    return "ASDF deserialisation: multiple keys for member '" + (key == "version" ? "version_" : key) +
        "' in " + std::string(typeName) + " are not allowed.";
}
template<class T>
ReadError readEnum(TypedJsonView data, T& value, std::string_view typeName,
    std::initializer_list<std::pair<std::string_view, const T*>> names) {
    // The original enum decoder ignores a failed scoped-string read.
    std::string_view text;
    const auto kind = data.kind();
    if (kind == JsonKind::String) text = data.stringBytes();
    else if (kind != JsonKind::Null) (void)unexpectedJsonKind(data);
    for (const auto& [name, candidate] : names) {
        if (text == name) { value = *candidate; return {}; }
    }
    std::string message = "Unable to deserialize string '";
    if (!text.empty()) message.append(text);
    message += "' to ";
    message.append(typeName);
    message += "Allowed keys:allowedKeys";
    throw d_runtime::ThrowableException(d_runtime::RetainedString(std::move(message)),
        d_runtime::RetainedString::fromStaticLiteral("fghj/serialization.d"), 2167);
}
inline ReadError wrongClassKind(TypedJsonView data, std::string_view typeName) {
    static constexpr const char* names[] = {"null_", "false_", "number", "string", "array", "object"};
    const auto name = data.kind() == JsonKind::Boolean && data.boolean() ? "true_" : names[static_cast<unsigned>(data.kind())];
    return "Cann't deserialize " + std::string(typeName) + ". Unexpected data: " + name;
}
template<class T> ReadError prepare(TypedJsonView data, std::shared_ptr<T>& value, std::string_view typeName) {
    if (data.kind() == JsonKind::Null) { value.reset(); return {}; }
    if (data.kind() != JsonKind::Object) return wrongClassKind(data, typeName);
    if (!value) value = std::make_shared<T>();
    return {};
}
}

inline ReadError deserializeValue(TypedJsonView data, PuppetAllowedUsers& value) {
    return metadata_detail::readEnum(data, value, "PuppetAllowedUsers", {
        {"OnlyAuthor", &PuppetAllowedUsers::OnlyAuthor}, {"OnlyLicensee", &PuppetAllowedUsers::OnlyLicensee},
        {"Everyone", &PuppetAllowedUsers::Everyone}});
}
inline ReadError deserializeValue(TypedJsonView data, PuppetAllowedRedistribution& value) {
    return metadata_detail::readEnum(data, value, "PuppetAllowedRedistribution", {
        {"Prohibited", &PuppetAllowedRedistribution::Prohibited}, {"ViralLicense", &PuppetAllowedRedistribution::ViralLicense},
        {"CopyleftLicense", &PuppetAllowedRedistribution::CopyleftLicense}});
}
inline ReadError deserializeValue(TypedJsonView data, PuppetAllowedModification& value) {
    return metadata_detail::readEnum(data, value, "PuppetAllowedModification", {
        {"Prohibited", &PuppetAllowedModification::Prohibited}, {"AllowPersonal", &PuppetAllowedModification::AllowPersonal},
        {"AllowRedistribute", &PuppetAllowedModification::AllowRedistribute}});
}
inline ReadError deserializeValue(TypedJsonView data, std::shared_ptr<PuppetUsageRights>& value) {
    if (auto error = metadata_detail::prepare(data, value, "PuppetUsageRights")) return error;
    if (!value) return {};
    std::set<std::string> seen;
    for (const auto& [key, member] : data.members()) {
        if (auto error = metadata_detail::checkDuplicate(key, seen, "PuppetUsageRights", {
            "allowedUsers", "allowViolence", "allowSexual", "allowCommercial", "allowRedistribution",
            "allowModification", "requireAttribution"})) return error;
        ReadError error;
        if (key == "allowedUsers") error = deserializeValue(member, value->allowedUsers);
        else if (key == "allowViolence") error = deserializeValue(member, value->allowViolence);
        else if (key == "allowSexual") error = deserializeValue(member, value->allowSexual);
        else if (key == "allowCommercial") error = deserializeValue(member, value->allowCommercial);
        else if (key == "allowRedistribution") error = deserializeValue(member, value->allowRedistribution);
        else if (key == "allowModification") error = deserializeValue(member, value->allowModification);
        else if (key == "requireAttribution") error = deserializeValue(member, value->requireAttribution);
        if (error) return error;
    }
    return {};
}
inline ReadError deserializeValue(TypedJsonView data, std::shared_ptr<PuppetMeta>& value) {
    if (auto error = metadata_detail::prepare(data, value, "PuppetMeta")) return error;
    if (!value) return {};
    bool nameSeen = false, versionSeen = false;
    std::set<std::string> seen;
    for (const auto& [key, member] : data.members()) {
        if (auto error = metadata_detail::checkDuplicate(key, seen, "PuppetMeta", {
            "name", "version", "rigger", "artist", "rights", "copyright", "licenseURL", "contact",
            "reference", "thumbnailId", "preservePixels"})) return error;
        ReadError error;
        if (key == "name") { error = deserializeValue(member, value->name); nameSeen = true; }
        else if (key == "version") { error = deserializeValue(member, value->version_); versionSeen = true; }
        else if (key == "rigger") error = deserializeValue(member, value->rigger);
        else if (key == "artist") error = deserializeValue(member, value->artist);
        else if (key == "rights") error = deserializeValue(member, value->rights);
        else if (key == "copyright") error = deserializeValue(member, value->copyright);
        else if (key == "licenseURL") error = deserializeValue(member, value->licenseURL);
        else if (key == "contact") error = deserializeValue(member, value->contact);
        else if (key == "reference") error = deserializeValue(member, value->reference);
        else if (key == "thumbnailId") error = deserializeValue(member, value->thumbnailId);
        else if (key == "preservePixels") error = deserializeValue(member, value->preservePixels);
        if (error) return error;
    }
    if (!nameSeen) return "FGHJ deserialisation: non-optional member 'name' in PuppetMeta is missing.";
    if (!versionSeen) return "FGHJ deserialisation: non-optional member 'version_' in PuppetMeta is missing.";
    return {};
}
inline ReadError deserializeValue(TypedJsonView data, std::shared_ptr<PuppetPhysics>& value) {
    if (auto error = metadata_detail::prepare(data, value, "PuppetPhysics")) return error;
    if (!value) return {};
    std::set<std::string> seen;
    for (const auto& [key, member] : data.members()) {
        if (auto error = metadata_detail::checkDuplicate(key, seen, "PuppetPhysics", {"pixelsPerMeter", "gravity"})) return error;
        ReadError error;
        if (key == "pixelsPerMeter") error = deserializeValue(member, value->pixelsPerMeter);
        else if (key == "gravity") error = deserializeValue(member, value->gravity);
        if (error) return error;
    }
    return {};
}
} // namespace nicxlive::core::serde
