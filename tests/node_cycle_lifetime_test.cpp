#include "puppet.hpp"
#include "render/backend_queue.hpp"
#include "runtime_state.hpp"
#include "timing.hpp"
#include "nodes/retained_graph.hpp"
#include "nodes/grid_deformer.hpp"
#include "nodes/path_deformer.hpp"
#include "nodes/simple_physics_systems.hpp"
#include "../fmt/fmt.hpp"
#include <cstdlib>
#include <iostream>
#include <new>
#include <stdexcept>
#include <string>

// Fault injection is enabled only immediately before releasing the model.
static bool rejectAllocation = false;
static long failAfter = -1;
void* operator new(std::size_t n) {
    if (rejectAllocation || failAfter == 0) throw std::bad_alloc();
    if (failAfter > 0) --failAfter;
    if (void* p = std::malloc(n ? n : 1)) return p;
    throw std::bad_alloc();
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }

using namespace nicxlive::core;
using nodes::Part;
using nodes::Drawable;
using nodes::Node;
using Ptr = std::shared_ptr<Part>;
using Weak = std::weak_ptr<Part>;
void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
void mask(const Ptr& target, const Ptr& source) {
    target->masks.push_back({source->uuid, std::static_pointer_cast<Drawable>(source), nodes::MaskingMode::Mask});
}
Ptr meshPart() {
    nodes::MeshData mesh;
    mesh.vertices={{0,0},{1,0},{0,1}};
    mesh.uvs=mesh.vertices;
    mesh.indices={0,1,2};
    auto part=Part::create();
    part->rebufferMesh(mesh);
    return part;
}
void ancestorUnload() {
    Weak a,b;
    {
        auto parent=Part::create(); auto child=Part::create(parent);
        auto owner=Puppet::create(parent); a=parent; b=child;
        mask(child,parent);
    }
    inUpdate();
    require(a.expired() && b.expired(),"ancestor mask cycle retained nodes");
}
void run(const std::string& name) {
    if (name=="acyclic") {
        Weak a,b; std::weak_ptr<Texture> texture;
        {
            auto owner=Puppet::create(); auto target=Part::create(owner->root); auto source=Part::create(owner->root);
            a=target; b=source; mask(target,source);
            target->textures[0]=std::make_shared<Texture>(std::vector<uint8_t>{1,2,3,255},1,1,4,4,false,false);
            texture=target->textures[0];
        }
        require(a.expired() && b.expired() && texture.expired(),"acyclic model retained resources");
    } else if (name=="ancestor") {
        ancestorUnload();
    } else if (name=="detached") {
        Weak a,b;
        {
            auto root=Part::create(); auto source=Part::create(); auto owner=Puppet::create(root);
            a=root; b=source; mask(root,source); mask(source,root);
        }
        inUpdate();
        require(a.expired() && b.expired(),"detached source cycle retained nodes");
    } else if (name=="self_mask") {
        Weak weak;
        { auto root=Part::create(); auto owner=Puppet::create(root); weak=root; mask(root,root); }
        inUpdate();
        require(weak.expired(),"self mask retained its node");
    } else if (name=="independent_models") {
        auto first=Puppet::create(); auto second=Puppet::create();
        auto target=Part::create(second->root); auto source=Part::create(second->root); mask(target,source);
        first.reset();
        require(target->maskCount()==1 && target->masks[0].maskSrc.lock()==source,"unrelated model was changed");
    } else if (name=="reparent" || name=="undo_reattach") {
        auto owner=Puppet::create(); auto first=Node::create(owner->root); auto second=Node::create(owner->root);
        auto target=Part::create(first); auto source=Part::create(first); mask(target,source);
        if (name=="undo_reattach") { target->insertInto(nullptr,0); owner->rescanNodes(); }
        second->addChild(target); owner->rescanNodes();
        require(target->maskCount()==1 && target->masks[0].maskSrc.lock()==source,"reparent/undo lost mask");
    } else if (name=="external_root") {
        // Equivalent to a document snapshot retaining nodes across close/reopen.
        auto root=Part::create(); auto child=Part::create(root); mask(child,root);
        { auto owner=Puppet::create(root); }
        require(child->maskCount()==1,"closing model erased mask on externally retained root");
        auto reopened=Puppet::create(root);
        require(child->masks[0].maskSrc.lock()==root,"reopening retained model lost mask source");
    } else if (name=="shared_root") {
        auto root=Part::create(); auto child=Part::create(root); mask(child,root);
        auto first=Puppet::create(root); auto second=Puppet::create(root);
        first.reset();
        require(child->maskCount()==1,"old owner erased masks after root was transferred to new owner");
        require(child->puppetRef()==second,"root transfer lost the new owner");
    } else if (name=="cross_model_source") {
        auto first=Puppet::create(); auto second=Puppet::create();
        auto a=Part::create(first->root); auto b=Part::create(second->root); auto c=Part::create(second->root);
        mask(a,b); mask(b,c);
        first.reset();
        require(b->maskCount()==1 && b->masks[0].maskSrc.lock()==c,"unloading A traversed mask reference and erased B's live masks");
    } else if (name=="root_replacement") {
        Weak a,b;
        {
            auto root=Part::create(); auto child=Part::create(root); mask(child,root);
            auto owner=Puppet::create(root); a=root; b=child;
            owner->setRootNode(Node::create()); owner->rescanNodes();
            root.reset(); child.reset();
        }
        require(a.expired() && b.expired(),"replaced root cycle survived final model unload");
    } else if (name=="deleted_subtree") {
        Weak a,b;
        {
            auto owner=Puppet::create(); auto group=Part::create(owner->root); auto child=Part::create(group);
            mask(child,group); a=group; b=child;
            group->insertInto(nullptr,0); owner->rescanNodes(); group.reset(); child.reset();
        }
        require(a.expired() && b.expired(),"deleted subtree cycle survived final model unload");
    } else if (name=="mask_snapshot") {
        Weak a,b;
        common::SharedArray<nodes::MaskBinding> saved;
        {
            auto root=Part::create(); auto child=Part::create(root); auto owner=Puppet::create(root);
            mask(child,root); a=root; b=child; saved=child->masks;
        }
        require(!a.expired(),"retained mask snapshot lost its source");
        require(saved.size()==1 && saved[0].maskSrc.lock()==a.lock(),"mask snapshot was mutated or invalidated");
        saved.clear();
        inUpdate();
        require(a.expired() && b.expired(),"releasing snapshot retained nodes");
    } else if (name=="welding_unload" || name=="welding_remove" || name=="welding_storage_control") {
        Weak a,b;
        auto backend=std::dynamic_pointer_cast<render::QueueRenderBackend>(getCurrentRenderBackend());
        const auto before=backend->drawableIndexBufferCount();
        {
            auto owner=Puppet::create(); auto first=meshPart(); auto second=meshPart();
            owner->root->addChild(first); owner->root->addChild(second); a=first; b=second;
            first->addWeldedTarget(second,std::vector<std::ptrdiff_t>{0,1,2},0.5f);
            if (name!="welding_unload") {
                first->removeWeldedTarget(second);
                require(first->weldedLinks.empty() && second->weldedLinks.empty(),"Welding removal left visible links");
                std::cout << "visible links and post filters after removal: " << first->weldedLinks.size()
                    << ',' << second->weldedLinks.size() << ',' << first->postProcessFilters.size()
                    << ',' << second->postProcessFilters.size() << '\n';
                if (name=="welding_storage_control") {
                    // Diagnostic control: explicitly release the zero-length backing arrays.
                    for(auto p:{first,second}) {
                        p->weldedLinks.clear(); p->postProcessFilters.clear(); p->deformPostProcessFilters.clear();
                    }
                }
            }
        }
        inUpdate();
        std::cout << "remaining geometry buffers: " << backend->drawableIndexBufferCount()-before << '\n';
        require(a.expired() && b.expired(),"Welding references/filters retained nodes after unload");
        require(backend->drawableIndexBufferCount()==before,"Welding unload retained geometry resources");
    } else if (name=="inp_cycle_roundtrip" || name=="inp_cycle_late_failure") {
        std::vector<uint8_t> bytes;
        {
            auto owner=Puppet::create(); auto source=meshPart(); auto target=meshPart();
            owner->root->addChild(source); source->addChild(target); mask(target,source);
            source->setName("source"); target->setName("target");
            auto texture=std::make_shared<Texture>(std::vector<uint8_t>{1,2,3,255},1,1,4,4,false,false);
            source->textures[0]=texture; target->textures[0]=texture; owner->populateTextureSlots();
            if(name=="inp_cycle_roundtrip") bytes=fmt::inWriteINPPuppetMemory(*owner);
            else {
                serde::TypedJsonWriter writer; owner->serialize(writer); writer.flush();
                const auto json=std::move(writer).takeString();
                bytes.assign(fmt::MAGIC_BYTES.begin(),fmt::MAGIC_BYTES.end());
                auto put32=[&](uint32_t n){for(int shift:{24,16,8,0})bytes.push_back(static_cast<uint8_t>(n>>shift));};
                put32(json.size()); bytes.insert(bytes.end(),json.begin(),json.end());
                bytes.insert(bytes.end(),fmt::TEX_SECTION.begin(),fmt::TEX_SECTION.end()); put32(2);
                auto good=fmt::encodeTga({1,2,3,255},1,1,4);
                put32(good.size()); bytes.push_back(0); bytes.insert(bytes.end(),good.begin(),good.end());
                put32(3); bytes.push_back(0); bytes.insert(bytes.end(),{1,2,3});
            }
        }
        inUpdate();
        auto backend=std::dynamic_pointer_cast<render::QueueRenderBackend>(getCurrentRenderBackend());
        const auto before=backend->drawableIndexBufferCount();
        for(int i=0;i<25;++i) {
            if(name=="inp_cycle_roundtrip") {
                Weak a,b; std::weak_ptr<Texture> texture;
                {
                    auto owner=fmt::inLoadPuppetFromMemory<Puppet>(bytes);
                    auto source=std::dynamic_pointer_cast<Part>(owner->findNodeByName("source"));
                    auto target=std::dynamic_pointer_cast<Part>(owner->findNodeByName("target"));
                    require(source && target,"serialized cyclic fixture lost its nodes");
                    require(target->maskCount()==1 && target->masks[0].maskSrc.lock()==source,"loaded ancestor mask lost its source");
                    a=source; b=target; texture=source->textures[0];
                    require(!texture.expired(),"loaded source texture was missing");
                }
                inUpdate();
                require(a.expired() && b.expired() && texture.expired(),"INP unload retained mask nodes or texture");
            } else {
                bool rejected=false;
                try { auto owner=fmt::inLoadPuppetFromMemory<Puppet>(bytes); }
                catch(const std::exception&) { rejected=true; }
                require(rejected,"corrupt late texture was accepted");
            }
            inUpdate();
            require(backend->drawableIndexBufferCount()==before,"INP unload/load failure retained geometry");
        }
    } else if (name=="external_release") {
        Weak a,b;
        auto root=Part::create(); auto child=Part::create(root); mask(child,root); a=root; b=child;
        { auto owner=Puppet::create(root); }
        inUpdate();
        require(child->maskCount()==1,"collection altered externally retained model");
        root.reset(); child.reset(); inUpdate();
        require(a.expired() && b.expired(),"last external reference release did not allow collection");
    } else if (name=="delegate_snapshot" || name=="filter_snapshot" || name=="children_snapshot"
            || name=="map_snapshot" || name=="map_entry_snapshot" || name=="welding_after_reset") {
        Weak a,b;
        Node::FilterHook::Func savedDelegate;
        Node::FilterArray savedFilters;
        Node::ChildArray savedChildren;
        Drawable::WeldingAppliedMap savedMap;
        std::shared_ptr<const bool> savedEntry;
        {
            auto owner=Puppet::create(); auto first=meshPart(); auto second=meshPart();
            owner->root->addChild(first); owner->root->addChild(second); a=first; b=second;
            first->addWeldedTarget(second,std::vector<std::ptrdiff_t>{0,1,2},0.5f);
            RenderContext context; first->runBeginTask(context); second->runBeginTask(context);
            if(name=="delegate_snapshot") savedDelegate=first->postProcessFilters[0].func;
            if(name=="filter_snapshot") savedFilters=first->postProcessFilters;
            if(name=="children_snapshot") savedChildren=owner->root->childrenList();
            if(name=="map_snapshot") savedMap=first->weldingApplied;
            if(name=="map_entry_snapshot") savedEntry=first->weldingApplied.findRetained(second);
        }
        inUpdate();
        if(name!="welding_after_reset") {
            require(!a.expired() && !b.expired(),"externally retained storage/delegate lost its nodes");
            require(a.lock()->weldedLinks.size()==1 && b.lock()->weldedLinks.size()==1,
                "collection altered retained Welding links");
        }
        savedDelegate={}; savedFilters.clear(); savedChildren.clear(); savedMap={}; savedEntry.reset();
        inUpdate();
        require(a.expired() && b.expired(),"releasing retained storage/delegate left a cycle");
    } else if (name=="deleted_subtree_live_model") {
        auto owner=Puppet::create(); Weak a,b;
        {
            auto group=Part::create(owner->root); auto child=Part::create(group); a=group; b=child; mask(child,group);
            group->insertInto(nullptr,0); owner->rescanNodes();
        }
        inUpdate();
        require(a.expired() && b.expired(),"deleted subtree leaked while the remaining model stayed open");
    } else if (name=="allocation_failure_recovery" || name=="allocation_failure_textured") {
        for(long budget=0;budget<250;++budget) {
            auto live=Part::create(); auto liveChild=Part::create(live); mask(liveChild,live);
            Weak a,b;
            auto owner=Puppet::create();
            std::weak_ptr<Texture> t0,t1;
            {
                auto root=meshPart(); auto child=meshPart(); owner->root->addChild(root); root->addChild(child);
                mask(child,root); a=root; b=child;
                if(name=="allocation_failure_textured") {
                    root->textures[0]=std::make_shared<Texture>(std::vector<uint8_t>{1,2,3,255},1,1,4,4,false,false);
                    root->textures[1]=Texture::createSource(1,1,4,false,false);
                    t0=root->textures[0]; t1=root->textures[1];
                }
            }
            std::set_terminate([] { std::_Exit(86); });
            failAfter=budget;
            owner.reset();
            failAfter=-1;
            require(liveChild->maskCount()==1,"failed collection changed a live mask");
            inUpdate();
            require(a.expired() && b.expired(),"allocation recovery failed to collect deferred nodes");
            require(t0.expired() && t1.expired(),"allocation recovery retained textures");
        }
    } else if (name=="source_texture_backend_switch") {
        auto firstBackend=std::make_shared<render::QueueRenderBackend>(); inSetRenderBackend(firstBackend);
        auto owner=Puppet::create(); std::uint32_t firstId;
        {
            auto root=Part::create(owner->root); auto child=Part::create(root); mask(child,root);
            root->textures[0]=Texture::createSource(1,1,4,false,false);
            firstId=root->textures[0]->getTextureId();
        }
        auto secondBackend=std::make_shared<render::QueueRenderBackend>(); inSetRenderBackend(secondBackend);
        auto liveTexture=Texture::createSource(1,1,4,false,false); const auto secondId=liveTexture->getTextureId();
        require(firstId==secondId,"fixture must exercise colliding per-backend texture IDs");
        owner.reset(); inUpdate();
        require(secondBackend->hasTexture(secondId),"unload destroyed the other backend's live texture");
        require(firstBackend->hasTexture(firstId),"source texture was disposed on the wrong current context");
        inSetRenderBackend(firstBackend); inUpdate();
        require(!firstBackend->hasTexture(firstId),"source texture was not retired on its owning backend");
        require(secondBackend->hasTexture(secondId),"retirement altered another backend's texture");
        liveTexture.reset(); inSetRenderBackend(secondBackend); inUpdate();
        require(!secondBackend->hasTexture(secondId),"second texture was not retired");
    } else if (name=="parameter_cycle" || name=="binding_snapshot" || name=="parameter_links") {
        Weak node; std::weak_ptr<Parameter> p,q;
        std::shared_ptr<param::ParameterBinding> held;
        {
            auto owner=Puppet::create(); auto part=meshPart(); owner->root->addChild(part); node=part;
            auto first=Parameter::createSource("first",false); auto second=Parameter::createSource("second",false);
            p=first; q=second; owner->parameters={first,second};
            auto value=first->createBinding(std::static_pointer_cast<Node>(part),std::string("opacity"),false);
            first->addBinding(value);
            auto deform=first->createBinding(std::static_pointer_cast<Node>(part),std::string("deform"),false);
            first->addBinding(deform);
            if(name=="parameter_links") {
                first->addBinding(first->createBinding(second,0,false));
                second->addBinding(second->createBinding(first,0,false));
            }
            if(name=="binding_snapshot") held=value;
        }
        inUpdate();
        if(held) {
            require(!node.expired() && !p.expired(),"retained parameter binding lost its target or parent");
            require(p.lock()->bindings.size()==2,"collection changed a live parameter's bindings");
        }
        held.reset(); inUpdate();
        require(node.expired() && p.expired() && q.expired(),"parameter/binding cycle retained data");
    } else if (name=="grid_filter_cycle" || name=="grid_delegate_snapshot") {
        std::weak_ptr<Node> gridWeak; Weak childWeak; Node::FilterHook::Func held;
        {
            auto owner=Puppet::create(); auto grid=nodes::GridDeformer::create(owner->root);
            auto child=meshPart(); grid->addChild(child); grid->setupChild(child);
            gridWeak=grid; childWeak=child;
            require(!child->preProcessFilters.empty() || !child->postProcessFilters.empty(),"grid fixture has no Source delegate");
            if(name=="grid_delegate_snapshot") held=child->preProcessFilters.empty()?child->postProcessFilters[0].func:child->preProcessFilters[0].func;
        }
        inUpdate();
        if(name=="grid_delegate_snapshot") require(!gridWeak.expired() && !childWeak.expired(),"retained grid method lost its receiver");
        held={}; inUpdate();
        require(gridWeak.expired() && childWeak.expired(),"grid child method retained its parent");
    } else if (name=="physics_system_cycle" || name=="physics_system_snapshot") {
        std::weak_ptr<Node> driverWeak; std::shared_ptr<nicxlive::phys::PhysicsSystem> held;
        {
            auto owner=Puppet::create(); auto driver=nodes::SimplePhysicsDriver::create(owner->root); driverWeak=driver;
            driver->system=nodes::Pendulum::create(driver);
            if(name=="physics_system_snapshot") held=driver->system;
        }
        inUpdate();
        if(held) require(!driverWeak.expired(),"retained physics system lost its driver");
        held.reset(); inUpdate();
        require(driverWeak.expired(),"physics system/driver cycle retained its node");
    } else if (name=="projectable_cache_cycle") {
        std::weak_ptr<Node> compositeWeak; Weak childWeak;
        {
            auto owner=Puppet::create(); auto composite=nodes::Composite::create(owner->root);
            auto child=Part::create(composite); mask(child,composite); composite->scanParts();
            compositeWeak=composite; childWeak=child;
        }
        inUpdate(); require(compositeWeak.expired() && childWeak.expired(),"projectable cached child cycle remained");
    } else if (name=="path_driver_cycle" || name=="path_driver_snapshot") {
        std::weak_ptr<Node> weak; std::shared_ptr<nodes::PathDeformer::PhysicsDriver> held;
        {
            auto owner=Puppet::create(); auto path=nodes::PathDeformer::create(owner->root); weak=path;
            auto native=std::make_unique<nodes::ConnectedPendulumDriver>(path);
            path->driver=std::make_shared<nodes::PathDeformer::ConnectedDriverAdapter>(std::move(native),path.get());
            if(name=="path_driver_snapshot") held=path->driver;
        }
        inUpdate();
        if(held) require(!weak.expired(),"retained path driver lost its deformer");
        held.reset(); inUpdate(); require(weak.expired(),"path driver/deformer cycle remained");
    } else if (name=="notification_cycle") {
        Weak a,b;
        {
            auto owner=Puppet::create(); auto parent=Part::create(owner->root); auto child=Part::create(parent);
            child->addNotifyListener(parent); a=parent; b=child;
        }
        inUpdate(); require(a.expired() && b.expired(),"native notification listener retained its ancestor");
    } else if (name=="surviving_atlas_after_unload") {
        auto first=Puppet::create(); auto second=Puppet::create();
        auto removed=meshPart(); auto kept=meshPart(); first->root->addChild(removed); second->root->addChild(kept);
        kept->getVertices().set(0,{17,23}); kept->deformation.set(0,{5,7});
        removed.reset(); first.reset(); inUpdate();
        require(kept->getVertices().xAt(0)==17 && kept->getVertices().yAt(0)==23,
            "atlas compaction changed the surviving model's vertices");
        require(kept->deformation.xAt(0)==5 && kept->deformation.yAt(0)==7,
            "atlas compaction changed surviving deformation values");
        require(kept->mesh->uvs.xAt(0)==0 && kept->mesh->uvs.yAt(0)==0,
            "atlas compaction changed surviving texture coordinates");
    } else if (name=="exception_unwind") {
        Weak a,b;
        try {
            auto root=Part::create(); auto child=Part::create(root); auto owner=Puppet::create(root);
            mask(child,root); a=root; b=child; throw std::runtime_error("simulated later load failure");
        } catch (const std::runtime_error&) {}
        inUpdate();
        require(a.expired() && b.expired(),"exception unwinding retained mask cycle");
    } else if (name=="repeated_unload") {
        for (int i=0;i<300;++i) { ancestorUnload(); inUpdate(); }
    } else if (name=="destructor_allocation_failure") {
        auto root=Part::create(); auto child=Part::create(root); auto owner=Puppet::create(root); mask(child,root);
        root.reset(); child.reset();
        std::set_terminate([] { std::_Exit(86); });
        rejectAllocation=true;
        owner.reset();
        rejectAllocation=false;
    } else { throw std::runtime_error("unknown scenario"); }
}
int main(int argc,char** argv) {
    if(argc!=2) return 2;
    inSetTimingFunc([] { return 0.0; });
    auto backend=std::make_shared<render::QueueRenderBackend>();
    inSetRenderBackend(backend); initRendererCommon();
    try {
        run(argv[1]); inUpdate();
        std::cout << "PASS " << argv[1] << '\n'; return 0;
    } catch(const std::exception& e) {
        std::cerr << "FAIL " << argv[1] << ": " << e.what() << '\n'; return 1;
    }
}
