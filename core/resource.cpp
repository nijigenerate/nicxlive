#include "resource.hpp"
#include "puppet.hpp"
#include <mutex>
#include "param/parameter.hpp"
#include "param/binding_impl.hpp"
#include "nodes/node.hpp"
#include "nodes/deformable.hpp"
#include "nodes/drawable.hpp"
#include "nodes/part.hpp"
#include "nodes/projectable.hpp"
#include "nodes/composite.hpp"
#include "nodes/dynamic_composite.hpp"
#include "nodes/grid_deformer.hpp"
#include "nodes/path_deformer.hpp"
#include "nodes/mask.hpp"
#include "nodes/mesh_group.hpp"
#include "nodes/driver.hpp"
#include "nodes/simple_physics_driver.hpp"

namespace nicxlive::core {
void ngRegisterModelResourceClassNames() {
    static std::once_flag initialized;
    std::call_once(initialized, [] {
        d_runtime::ngRegisterObjectClassName(typeid(Puppet),
            d_runtime::RetainedString::fromStaticLiteral("nijilive.core.puppet.Puppet"));
        d_runtime::ngRegisterObjectClassName(typeid(nodes::Node),
            d_runtime::RetainedString::fromStaticLiteral("nijilive.core.nodes.node.Node"));
        d_runtime::ngRegisterObjectClassName(typeid(nodes::Node::MatrixHolder),
            d_runtime::RetainedString::fromStaticLiteral("nijilive.core.nodes.node.Node.MatrixHolder"));
        d_runtime::ngRegisterObjectClassName(typeid(nodes::TmpNode),
            d_runtime::RetainedString::fromStaticLiteral("nijilive.core.nodes.node.TmpNode"));
        d_runtime::ngRegisterObjectClassName(typeid(nodes::Deformable),
            d_runtime::RetainedString::fromStaticLiteral("nijilive.core.nodes.deformable.Deformable"));
        d_runtime::ngRegisterObjectClassName(typeid(nodes::Drawable),
            d_runtime::RetainedString::fromStaticLiteral("nijilive.core.nodes.drawable.Drawable"));
        d_runtime::ngRegisterObjectClassName(typeid(nodes::Part),
            d_runtime::RetainedString::fromStaticLiteral("nijilive.core.nodes.part.Part"));
        d_runtime::ngRegisterObjectClassName(typeid(nodes::Projectable),
            d_runtime::RetainedString::fromStaticLiteral("nijilive.core.nodes.composite.projectable.Projectable"));
        d_runtime::ngRegisterObjectClassName(typeid(nodes::Composite),
            d_runtime::RetainedString::fromStaticLiteral("nijilive.core.nodes.composite.composite.Composite"));
        d_runtime::ngRegisterObjectClassName(typeid(nodes::DynamicComposite),
            d_runtime::RetainedString::fromStaticLiteral("nijilive.core.nodes.composite.dcomposite.DynamicComposite"));
        d_runtime::ngRegisterObjectClassName(typeid(nodes::GridDeformer),
            d_runtime::RetainedString::fromStaticLiteral("nijilive.core.nodes.deformer.grid.GridDeformer"));
        d_runtime::ngRegisterObjectClassName(typeid(nodes::PathDeformer),
            d_runtime::RetainedString::fromStaticLiteral("nijilive.core.nodes.deformer.path.PathDeformer"));
        d_runtime::ngRegisterObjectClassName(typeid(nodes::Mask),
            d_runtime::RetainedString::fromStaticLiteral("nijilive.core.nodes.mask.Mask"));
        d_runtime::ngRegisterObjectClassName(typeid(nodes::MeshGroup),
            d_runtime::RetainedString::fromStaticLiteral("nijilive.core.nodes.meshgroup.MeshGroup"));
        d_runtime::ngRegisterObjectClassName(typeid(nodes::Driver),
            d_runtime::RetainedString::fromStaticLiteral("nijilive.core.nodes.drivers.Driver"));
        d_runtime::ngRegisterObjectClassName(typeid(nodes::SimplePhysicsDriver),
            d_runtime::RetainedString::fromStaticLiteral("nijilive.core.nodes.drivers.simplephysics.SimplePhysics"));
        d_runtime::ngRegisterObjectClassName(typeid(param::Parameter),
            d_runtime::RetainedString::fromStaticLiteral("nijilive.core.param.Parameter"));
        d_runtime::ngRegisterObjectClassName(typeid(param::ParameterBinding),
            d_runtime::RetainedString::fromStaticLiteral("nijilive.core.param.binding.ParameterBinding"));
        d_runtime::ngRegisterObjectClassName(typeid(param::ValueParameterBinding),
            d_runtime::RetainedString::fromStaticLiteral("nijilive.core.param.binding.ValueParameterBinding"));
        d_runtime::ngRegisterObjectClassName(typeid(param::DeformationParameterBinding),
            d_runtime::RetainedString::fromStaticLiteral("nijilive.core.param.binding.DeformationParameterBinding"));
        d_runtime::ngRegisterObjectClassName(typeid(param::ParameterParameterBinding),
            d_runtime::RetainedString::fromStaticLiteral("nijilive.core.param.binding.ParameterParameterBinding"));
    });
}
}
