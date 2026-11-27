#pragma once
// Source Object and TypeInfo_Class protocol, object.d (Digital Mars 2000-2011).
// BSL-1.0; see licenses/druntime-associative-array.txt.
#include "../compat/object_metadata.hpp"
#include <cstddef>
#include <memory>
#include <typeinfo>

namespace nicxlive::core::common {
template<class T> const void* ngClassObjectIdentity(const std::shared_ptr<T>& object) noexcept {
    // Identity is a source intrinsic, never a user-hideable member dispatch.
    return object ? dynamic_cast<const void*>(object.get()) : nullptr;
}

template<class L,class R> bool ngClassObjectIs(const std::shared_ptr<L>& left,
    const std::shared_ptr<R>& right) noexcept {
    return ngClassObjectIdentity(left)==ngClassObjectIdentity(right);
}

// object.d global opEquals: metadata-equivalent classes use one virtual call;
// distinct classes require both directions. This is NOT AA TypeInfo equality.
template<class T> bool ngClassObjectEquals(std::shared_ptr<T> left,std::shared_ptr<T> right,
    void (*prepareMetadata)()=nullptr) {
    if(ngClassObjectIs(left,right)) return true;
    if(!left || !right) return false;
    if(!left->opEquals(right)) return false;
    if(typeid(*left)==typeid(*right)) return true;
    if(prepareMetadata) prepareMetadata();
    const auto leftName=d_runtime::ngFindObjectClassName(typeid(*left));
    const auto rightName=d_runtime::ngFindObjectClassName(typeid(*right));
    if(leftName && rightName && leftName->view()==rightName->view()) return true;
    return right->opEquals(left);
}

// object.d __cmp orders class/interface references before virtual opCmp.
// Static null operands have their own branches, distinct from null handles.
inline int ngClassObjectCompare(std::nullptr_t, std::nullptr_t) noexcept { return 0; }
template<class T>
int ngClassObjectCompare(std::nullptr_t, std::shared_ptr<T>) noexcept { return -1; }
template<class T>
int ngClassObjectCompare(std::shared_ptr<T>, std::nullptr_t) noexcept { return 1; }
template<class T>
int ngClassObjectCompare(std::shared_ptr<T> left, std::shared_ptr<T> right) {
    if (ngClassObjectIs(left, right)) return 0;
    if (!left) return -1;
    if (!right) return 1;
    return left->opCmp(right);
}

template<class Key> struct ObjectClassKeyPolicy;
template<class T> struct ObjectClassKeyPolicy<std::shared_ptr<T>> {
    struct Hash {
        std::size_t operator()(const std::shared_ptr<T>& query) const {
            // TypeInfo_Class.getHash copies the actual handle before dispatch;
            // a virtual override may replace the original caller key cell.
            // D nothrow still permits Error propagation through this call.
            const auto object=query;
            return object ? object->toHash() : 0;
        }
    };
    struct Equal {
        bool operator()(const std::shared_ptr<T>& query,const std::shared_ptr<T>& stored) const {
            // TypeInfo_Class.equals (object.d1650): query first, no symmetric
            // call, no stored-null rejection and no opCmp fallback.
            const auto left=query;
            const auto right=stored;
            return ngClassObjectIs(left,right) || (left && left->opEquals(right));
        }
    };
    struct Compare {
        int operator()(const std::shared_ptr<T>& query,const std::shared_ptr<T>& stored) const {
            const auto left=query;
            const auto right=stored;
            if(ngClassObjectIs(left,right)) return 0;
            if(!left) return -1;
            if(!right) return 1;
            return left->opCmp(right);
        }
    };
};
}
