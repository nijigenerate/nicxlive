// Original package initialization bodies; each retains its authored phase.
#include "initialization.hpp"
#include "node.hpp"
#include "part.hpp"
#include "mask.hpp"
#include "composite.hpp"
#include "mesh_group.hpp"
#include "dynamic_composite.hpp"
#include "grid_deformer.hpp"
#include "path_deformer.hpp"
#include "../runtime_state.hpp"

namespace nicxlive::core::nodes {
namespace {
template<class T, std::size_t N> void registerSourceType(const char (&id)[N]) {
    Node::inRegisterNodeTypeRetained(d_runtime::RetainedString::fromStaticLiteral(id),
        Node::ParentNodeFactory([](const std::shared_ptr<Node>& parent) -> std::shared_ptr<Node> {
            return T::create(parent);
        }));
}
}
void inInitDrawable() {
#ifndef NICXLIVE_SOURCE_NO_RENDER
    currentRenderBackend()->initializeDrawableResources();
#endif
}
void inInitPart() { registerSourceType<Part>("Part"); }
void inInitMask() { registerSourceType<Mask>("Mask"); }
void inInitComposite() { registerSourceType<Composite>("Composite"); }
void inInitMeshGroup() { registerSourceType<MeshGroup>("MeshGroup"); }
void inInitDComposite() { registerSourceType<DynamicComposite>("DynamicComposite"); }
void inInitGridDeformer() { registerSourceType<GridDeformer>("GridDeformer"); }
void inInitPathDeformer() {
    registerSourceType<PathDeformer>("PathDeformer");
    Node::inAliasNodeTypeRetained(d_runtime::RetainedString::fromStaticLiteral("PathDeformer"),
        d_runtime::RetainedString::fromStaticLiteral("BezierDeformer"));
}
} // namespace nicxlive::core::nodes
