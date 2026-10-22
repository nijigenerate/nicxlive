#include "puppet.hpp"
#include "nodes/part.hpp"
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
    return 0;
} catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;} }
