#include "puppet.hpp"
#include "nodes/part.hpp"
#include "nodes/composite.hpp"
#include "nodes/dynamic_composite.hpp"
#include "render/backend_queue.hpp"
#include "runtime_state.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace nicxlive::core;
int main() { try {
    auto queue=std::make_shared<render::QueueRenderBackend>();
    inSetRenderBackend(queue);
    auto texture=std::make_shared<Texture>(std::vector<uint8_t>{255,255,255,255},1,1,4,4,false,false);
    auto root=nodes::Part::createSimple(texture,"parent");
    auto puppet=std::make_shared<Puppet>(root);
    root->setPuppet(puppet);
    root->build(false);
    auto checkDrawCount=[&](size_t expected) {
        puppet->update();queue->clear();puppet->draw();
        const auto count=std::count_if(queue->queue.begin(),queue->queue.end(),[](const auto& command){return command.kind==render::RenderCommandKind::DrawPart;});
        if(size_t(count)!=expected)throw std::runtime_error("Rescanned hierarchy is missing from rendered commands");
    };
    checkDrawCount(1);
    auto child=nodes::Part::createSimple(texture,"attachment");
    child->reparent(root,0,true);
    puppet->rescanNodes();
    checkDrawCount(2);
    child->reparent(nullptr,0,true);
    puppet->rescanNodes();
    checkDrawCount(1);
    std::cout<<"Rescan updates cached render tasks after attachment and removal\n";
    auto checkCompositeRescan = [&]<typename CompositeType>() {
        auto composite = std::make_shared<CompositeType>();
        auto nested = std::make_shared<nodes::Composite>();
        auto part = nodes::Part::createSimple(texture, "nested attachment");
        composite->addChild(nested);
        nested->addChild(part);
        auto owner = std::make_shared<Puppet>(composite);
        composite->setPuppet(owner);
        owner->rescanNodes();
        if (composite->subParts.size() != 1 || composite->subParts.front() != nested ||
            nested->subParts.size() != 1 || nested->subParts.front() != part)
            throw std::runtime_error("Puppet rescan did not refresh nested Composite parts");
        nested->clearChildren();
        owner->rescanNodes();
        if (!nested->subParts.empty())
            throw std::runtime_error("Puppet rescan retained removed Composite parts");
        composite->clearChildren();
        owner->rescanNodes();
        if (!composite->subParts.empty())
            throw std::runtime_error("Puppet rescan retained removed nested Composite");
    };
    checkCompositeRescan.operator()<nodes::Composite>();
    checkCompositeRescan.operator()<nodes::DynamicComposite>();
    auto pinParent = nodes::Part::createSimple(texture, "deforming parent");
    auto pinned = nodes::Part::createSimple(texture, "runtime pin");
    pinned->reparent(pinParent, 0, true);
    pinned->setPinToMesh(true);
    constexpr std::uintptr_t attachTag = 0x6e617474u;
    if (!pinned->hasDeformPreProcessFilter(0, attachTag))
        throw std::runtime_error("Runtime pin did not register deformation following");
    pinParent->setupChild(pinned);
    if (pinned->deformPreProcessFilters.size() != 1)
        throw std::runtime_error("Repeated pin setup duplicated the attachment filter");
    RenderContext pinContext;
    pinParent->runBeginTask(pinContext);
    pinned->runBeginTask(pinContext);
    for (auto& vertex : pinParent->getMesh().vertices) { vertex.x *= 100; vertex.y *= 100; }
    pinParent->updateVertices();
    pinParent->deformation.fill({40.0f, 20.0f});
    auto beforePin = pinned->transform().translation;
    pinned->preProcess();
    auto position = pinned->transform().translation;
    if (std::abs(position.x - beforePin.x - 40.0f) > 0.001f || std::abs(position.y - beforePin.y - 20.0f) > 0.001f)
        throw std::runtime_error("Runtime pin did not follow parent mesh deformation");
    pinned->setPinToMesh(false);
    if (pinned->hasDeformPreProcessFilter(0, attachTag))
        throw std::runtime_error("Disabling pin retained the attachment filter");
    pinned->setPinToMesh(true);
    pinned->reparent(nullptr, 0, true);
    if (pinned->hasDeformPreProcessFilter(0, attachTag))
        throw std::runtime_error("Detaching retained the attachment filter");
    auto nextParent = nodes::Part::createSimple(texture, "next parent");
    pinned->reparent(nextParent, 0, true);
    if (!pinned->hasDeformPreProcessFilter(0, attachTag))
        throw std::runtime_error("Reattaching did not register the new parent");
    std::cout << "Runtime pin follows mesh deformation and releases its filter on disable/detach\n";
    auto checkCompositePinBuild = [&]<typename CompositeType>() {
        auto composite = std::make_shared<CompositeType>();
        auto child = nodes::Part::createSimple(texture, "loaded pin");
        child->pinToMesh = true; // Serialized flag: no runtime setter call.
        composite->addChild(child);
        composite->build(false);
        if (!child->hasDeformPreProcessFilter(0, 0x6e617474u))
            throw std::runtime_error("Composite build omitted the attachment filter");
        composite->build(false);
        if (child->deformPreProcessFilters.size() != 1)
            throw std::runtime_error("Composite rebuild duplicated the attachment filter");
        composite->releaseChildDrawable();
        if (child->hasDeformPreProcessFilter(0, 0x6e617474u))
            throw std::runtime_error("Composite drawable release retained the attachment filter");
    };
    checkCompositePinBuild.operator()<nodes::Composite>();
    checkCompositePinBuild.operator()<nodes::DynamicComposite>();
    return 0;
} catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;} }
