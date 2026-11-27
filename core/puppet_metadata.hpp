#pragma once

#include "serde_json.hpp"
#include "compat/assert_error.hpp"
#include "compat/retained_string.hpp"
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <initializer_list>
#include <stdexcept>

namespace nicxlive::core {

inline constexpr std::uint32_t NO_THUMBNAIL = std::numeric_limits<std::uint32_t>::max();

// D string enums retain the underlying nullable string, including explicit
// casts of values outside their named members. Keep distinct C++ field types.
template<class Tag> struct PuppetStringEnum {
    std::optional<std::string> value;
    explicit PuppetStringEnum(std::string_view text) : value(std::string(text)) {}
    explicit PuppetStringEnum(std::nullptr_t) : value(std::nullopt) {}
    void serializeNamed(serde::TypedJsonWriter& writer,
        std::initializer_list<std::pair<std::string_view, std::string_view>> names) const {
        for (const auto& [underlying, name] : names) {
            if (value && *value == underlying) { writer.putValue(name); return; }
        }
        d_runtime::ngAssertUnreachable("Assertion failure", "mir/serde.d", 522);
    }
    template<class S> void serializeNamedSource(S& writer,
        std::initializer_list<std::pair<std::string_view, d_runtime::RetainedString>> names) const {
        for (const auto& [underlying, name] : names) {
            if (value && *value == underlying) { writer.putValue(name); return; }
        }
        d_runtime::ngAssertUnreachable("Assertion failure", "mir/serde.d", 522);
    }
    bool operator==(const PuppetStringEnum&) const = default;
};
struct AllowedUsersTag;
struct AllowedRedistributionTag;
struct AllowedModificationTag;
struct PuppetAllowedUsers : PuppetStringEnum<AllowedUsersTag> {
    using PuppetStringEnum::PuppetStringEnum;
    PuppetAllowedUsers() : PuppetStringEnum("onlyAuthor") {}
    static const PuppetAllowedUsers OnlyAuthor, OnlyLicensee, Everyone;
    template<class S> void serialize(S& writer) const {
        serializeNamedSource(writer, {
            {"onlyAuthor", d_runtime::RetainedString::fromStaticLiteral("OnlyAuthor")},
            {"onlyLicensee", d_runtime::RetainedString::fromStaticLiteral("OnlyLicensee")},
            {"everyone", d_runtime::RetainedString::fromStaticLiteral("Everyone")}
        });
    }
    void serialize(serde::TypedJsonWriter& writer) const {
        serializeNamed(writer, {{"onlyAuthor", "OnlyAuthor"}, {"onlyLicensee", "OnlyLicensee"}, {"everyone", "Everyone"}});
    }
};
inline const PuppetAllowedUsers PuppetAllowedUsers::OnlyAuthor{"onlyAuthor"};
inline const PuppetAllowedUsers PuppetAllowedUsers::OnlyLicensee{"onlyLicensee"};
inline const PuppetAllowedUsers PuppetAllowedUsers::Everyone{"everyone"};

struct PuppetAllowedRedistribution : PuppetStringEnum<AllowedRedistributionTag> {
    using PuppetStringEnum::PuppetStringEnum;
    PuppetAllowedRedistribution() : PuppetStringEnum("prohibited") {}
    static const PuppetAllowedRedistribution Prohibited, ViralLicense, CopyleftLicense;
    template<class S> void serialize(S& writer) const {
        serializeNamedSource(writer, {
            {"prohibited", d_runtime::RetainedString::fromStaticLiteral("Prohibited")},
            {"viralLicense", d_runtime::RetainedString::fromStaticLiteral("ViralLicense")},
            {"copyleftLicense", d_runtime::RetainedString::fromStaticLiteral("CopyleftLicense")}
        });
    }
    void serialize(serde::TypedJsonWriter& writer) const {
        serializeNamed(writer, {{"prohibited", "Prohibited"}, {"viralLicense", "ViralLicense"},
            {"copyleftLicense", "CopyleftLicense"}});
    }
};
inline const PuppetAllowedRedistribution PuppetAllowedRedistribution::Prohibited{"prohibited"};
inline const PuppetAllowedRedistribution PuppetAllowedRedistribution::ViralLicense{"viralLicense"};
inline const PuppetAllowedRedistribution PuppetAllowedRedistribution::CopyleftLicense{"copyleftLicense"};

struct PuppetAllowedModification : PuppetStringEnum<AllowedModificationTag> {
    using PuppetStringEnum::PuppetStringEnum;
    PuppetAllowedModification() : PuppetStringEnum("prohibited") {}
    static const PuppetAllowedModification Prohibited, AllowPersonal, AllowRedistribute;
    template<class S> void serialize(S& writer) const {
        serializeNamedSource(writer, {
            {"prohibited", d_runtime::RetainedString::fromStaticLiteral("Prohibited")},
            {"allowPersonal", d_runtime::RetainedString::fromStaticLiteral("AllowPersonal")},
            {"allowRedistribute", d_runtime::RetainedString::fromStaticLiteral("AllowRedistribute")}
        });
    }
    void serialize(serde::TypedJsonWriter& writer) const {
        serializeNamed(writer, {{"prohibited", "Prohibited"}, {"allowPersonal", "AllowPersonal"},
            {"allowRedistribute", "AllowRedistribute"}});
    }
};
inline const PuppetAllowedModification PuppetAllowedModification::Prohibited{"prohibited"};
inline const PuppetAllowedModification PuppetAllowedModification::AllowPersonal{"allowPersonal"};
inline const PuppetAllowedModification PuppetAllowedModification::AllowRedistribute{"allowRedistribute"};

struct PuppetUsageRights {
    PuppetAllowedUsers allowedUsers = PuppetAllowedUsers::OnlyAuthor;
    bool allowViolence = false;
    bool allowSexual = false;
    bool allowCommercial = false;
    PuppetAllowedRedistribution allowRedistribution = PuppetAllowedRedistribution::Prohibited;
    PuppetAllowedModification allowModification = PuppetAllowedModification::Prohibited;
    bool requireAttribution = false;

    void serialize(serde::TypedJsonWriter& writer) const {
        const auto state = writer.structBegin();
        writer.putField("allowedUsers", allowedUsers);
        writer.putField("allowViolence", allowViolence);
        writer.putField("allowSexual", allowSexual);
        writer.putField("allowCommercial", allowCommercial);
        writer.putField("allowRedistribution", allowRedistribution);
        writer.putField("allowModification", allowModification);
        writer.putField("requireAttribution", requireAttribution);
        writer.structEnd(state);
    }
};

struct PuppetMeta {
    std::optional<std::string> name;
    std::optional<std::string> version_ = "1.0-alpha";
    std::optional<std::string> rigger;
    std::optional<std::string> artist;
    std::shared_ptr<PuppetUsageRights> rights;
    std::optional<std::string> copyright;
    std::optional<std::string> licenseURL;
    std::optional<std::string> contact;
    std::optional<std::string> reference;
    std::uint32_t thumbnailId = NO_THUMBNAIL;
    bool preservePixels = false;

    void serialize(serde::TypedJsonWriter& writer) const {
        const auto state = writer.structBegin();
        writer.putField("name", name);
        writer.putField("version", version_);
        writer.putField("rigger", rigger);
        writer.putField("artist", artist);
        writer.putField("rights", rights);
        writer.putField("copyright", copyright);
        writer.putField("licenseURL", licenseURL);
        writer.putField("contact", contact);
        writer.putField("reference", reference);
        writer.putField("thumbnailId", thumbnailId);
        writer.putField("preservePixels", preservePixels);
        writer.structEnd(state);
    }
};

struct PuppetPhysics {
    float pixelsPerMeter = 1000.0f;
    float gravity = 9.8f;
    void serialize(serde::TypedJsonWriter& writer) const {
        const auto state = writer.structBegin();
        writer.putField("pixelsPerMeter", pixelsPerMeter);
        writer.putField("gravity", gravity);
        writer.structEnd(state);
    }
};
} // namespace nicxlive::core
