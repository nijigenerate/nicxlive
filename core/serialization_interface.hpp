#pragma once

#include <concepts>
#include <type_traits>

namespace nicxlive::core::fmt {

template<class Implementation = void> class ISerializable;

// All implementations share the Source interface identity. It can retain
// heterogeneous class handles without inventing a runtime template-method slot.
template<> class ISerializable<void> {
protected:
    ISerializable() = default;
    ISerializable(const ISerializable&) = default;
    ISerializable& operator=(const ISerializable&) = default;
public:
    virtual ~ISerializable() = 0;
};

inline ISerializable<void>::~ISerializable() = default;

// A template method cannot occupy a C++ virtual slot. Keep the concrete Source
// receiver in this base so every serializer S instantiates the actual body,
// taking the original serializer by mutable reference without type erasure.
template<class Implementation> class ISerializable : public ISerializable<> {
protected:
    ISerializable() = default;
    ISerializable(const ISerializable&) = default;
    ISerializable& operator=(const ISerializable&) = default;
public:
    using SerializationImplementation = Implementation;
    virtual ~ISerializable() = 0;

    template<class S> void serialize(S& serializer) {
        static_assert(!std::is_same_v<decltype(&Implementation::template serialize<S>),
            void (ISerializable::*)(S&)>, "The concrete class must implement generic serialize(S&)");
        static_cast<Implementation&>(*this).serialize(serializer);
    }
};

template<class Implementation>
inline ISerializable<Implementation>::~ISerializable() = default;

template<class T>
concept CustomSerializable = std::derived_from<T, ISerializable<>>;

template<CustomSerializable T, class S>
void ngSerializeCustom(T& value, S& serializer) {
    // Lookup on the actual static class also honors a subclass's own generic
    // member. If none exists, the inherited base still checks its contract.
    value.serialize(serializer);
}

} // namespace nicxlive::core::fmt
