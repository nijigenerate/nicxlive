#include "puppet.hpp"
#include "nodes/part.hpp"
#include "nodes/composite.hpp"
#include "nodes/dynamic_composite.hpp"
#include "render/backend_queue.hpp"
#include "runtime_state.hpp"
#include <algorithm>
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
    return 0;
} catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;} }
