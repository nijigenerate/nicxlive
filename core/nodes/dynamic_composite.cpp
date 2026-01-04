#include "dynamic_composite.hpp"

namespace nicxlive::core::nodes {

DynamicComposite::DynamicComposite(const MeshData& data, uint32_t uuidVal) : Projectable() {
    *mesh = data;
    if (uuidVal != 0) uuid = uuidVal;
}

const std::string& DynamicComposite::typeId() const {
    static const std::string k = "DynamicComposite";
    return k;
}

} // namespace nicxlive::core::nodes
