#include "nodes/part.hpp"
#include "render/backend_queue.hpp"
#include "runtime_state.hpp"
#include <algorithm>
#include <iostream>
#include <stdexcept>

using namespace nicxlive::core;
using Indices = common::SharedArray<std::ptrdiff_t>;

void requireIndices(const Indices& actual, std::initializer_list<std::ptrdiff_t> expected) {
    if (!std::ranges::equal(actual, expected)) throw std::runtime_error("Incorrect reciprocal welding indices");
}

template<class Operation> void requireRejected(Operation operation) {
    try { operation(); }
    catch (const std::exception&) { return; }
    throw std::runtime_error("Invalid welding indices were accepted");
}

int main() { try {
    inSetRenderBackend(std::make_shared<render::QueueRenderBackend>());
    initRendererCommon();
    nodes::MeshData mesh;
    mesh.vertices={{0,0},{1,0},{0,1}};
    mesh.uvs=mesh.vertices;
    mesh.indices={0,1,2};
    const auto source=std::make_shared<nodes::Part>(mesh,
        std::array<std::shared_ptr<Texture>,3>{},101);
    const auto target=std::make_shared<nodes::Part>(mesh.copy(),
        std::array<std::shared_ptr<Texture>,3>{},102);
    target->localTransform.translation={1000,2000,0};
    target->localTransform.update();
    const std::shared_ptr<nodes::Drawable> destination=target;

    // Authored pairs remain reciprocal even when their rest positions are far
    // apart. Neither a proximity search nor transforms may choose another pair.
    const Indices authored{2,nodes::NOINDEX,0};
    requireIndices(source->counterWeldingIndices(destination,authored),{2,nodes::NOINDEX,0});
    requireIndices(source->counterWeldingIndices(destination,
        std::span<const std::ptrdiff_t>(authored.data(),authored.size())),{2,nodes::NOINDEX,0});
    requireIndices(source->counterWeldingIndices(destination,Indices{1,1,nodes::NOINDEX}),
        {nodes::NOINDEX,0,nodes::NOINDEX});
    requireRejected([&] { source->counterWeldingIndices(destination,Indices{0,1,3}); });
    requireRejected([&] { source->counterWeldingIndices(destination,Indices{0,-2,1}); });
    requireRejected([&] { source->counterWeldingIndices(destination,Indices{0,1}); });
    requireRejected([&] { source->counterWeldingIndices(source,authored); });
    requireRejected([&] { source->updatedCounterWeldingIndices(destination,authored); });

    target->localTransform.translation={0,0,0};
    target->localTransform.update();
    target->getVertices().set(1,{0.1f,0});
    target->getVertices().set(2,{0,0.1f});
    const Indices singlePair{0,nodes::NOINDEX,nodes::NOINDEX};
    requireIndices(source->counterWeldingIndices(destination,singlePair),
        {0,nodes::NOINDEX,nodes::NOINDEX});

    // Updating authored pairs must discard obsolete reverse mappings in both
    // the native span API and the retained-array API.
    source->weldedLinks.push_back({target->uuid,destination,authored,0.5f});
    target->weldedLinks.push_back({source->uuid,std::static_pointer_cast<nodes::Drawable>(source),
        Indices{2,nodes::NOINDEX,0},0.5f});
    struct ReleaseLinks {
        std::shared_ptr<nodes::Part> source,target;
        ~ReleaseLinks() { source->weldedLinks.clear(); target->weldedLinks.clear(); }
    } release{source,target};
    const Indices replacement{nodes::NOINDEX,0,1};
    requireIndices(source->updatedCounterWeldingIndices(destination,replacement),{1,2,nodes::NOINDEX});
    requireIndices(source->updatedCounterWeldingIndices(destination,
        std::span<const std::ptrdiff_t>(replacement.data(),replacement.size())),{1,2,nodes::NOINDEX});
    source->weldedLinks[0].indices=singlePair;
    target->weldedLinks[0].indices=Indices{0,0,0};
    requireIndices(source->updatedCounterWeldingIndices(destination,singlePair),
        {0,nodes::NOINDEX,nodes::NOINDEX});
    std::cout << "PASS: authored welding pairs, updates and invalid-index rejection\n";
    return 0;
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
} }
