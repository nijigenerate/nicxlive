#include "texture.hpp"
#include "render/backend_queue.hpp"
#include <iostream>
#include <stdexcept>
using namespace nicxlive::core;
void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
int main() {
    auto backend = std::make_shared<render::QueueRenderBackend>();
    setCurrentRenderBackend(backend);
    {
        // Source textures have no legacy CPU mirror; host upload must read the backend.
        SourceTextureBytes pixels(8, 0);
        pixels[0] = 42; pixels[3] = 255; pixels[4] = 91; pixels[7] = 128;
        auto source = Texture::createSource(pixels, 2, 1, 4, 4, false, false);
        check(source->data().empty(), "source unexpectedly has a legacy CPU mirror");
        auto bytes = source->getSourceTextureData();
        check(bytes.size() == 8 && bytes[0] == 42 && bytes[4] == 91 && bytes[7] == 128,
              "opaque source texture lost pixels on queue readback");
        pixels[0] = 73;
        source->setData(pixels, 4);
        check(source->getSourceTextureData()[0] == 73, "opaque source upload did not update pixels");
    }
    Texture texture(std::vector<uint8_t>{10, 20, 30, 128, 7, 8, 9, 0}, 2, 1);
    check(texture.getSourceTextureData()[0] == 10, "legacy texture source readback failed");
    auto first = texture.getTextureData();
    check(*first == texture.data(), "initial readback");
    // Exercise real queue storage changes, independent of Texture's CPU mirror.
    backend->updateTexture(texture.backendId(), {40, 50, 60, 128, 1, 2, 3, 0}, 2, 1, 4);
    auto read = texture.getTextureData();
    check((*read)[0] == 40 && texture.data()[0] == 10, "readback ignored backend");
    auto unpremultiplied = texture.getTextureData(true);
    check((*unpremultiplied)[0] == 79 && (*unpremultiplied)[1] == 99
        && (*unpremultiplied)[2] == 119 && (*unpremultiplied)[4] == 1, "unmultiply integer order");
    (*read)[0] = 99;
    check((*texture.getTextureData())[0] == 40, "unlocked read leaked mutation");
    texture.lock();
    auto staging = texture.getTextureData();
    check(staging == texture.getTextureData(true), "locked getter did not retain alias or bypass unmultiply");
    check((*staging)[0] == 40, "lock failed readback");
    texture.lock();
    check(staging == texture.getTextureData(), "second lock discarded staging");
    (*staging)[0] = 70;
    texture.unlock();
    check((*texture.getTextureData())[0] == 40, "unmarked mutation unexpectedly uploaded");
    check((*staging)[0] == 70, "unlock invalidated retained slice");
    texture.lock();
    auto replacement = std::make_shared<std::vector<uint8_t>>(std::initializer_list<uint8_t>{120, 50, 60, 128, 1, 2, 3, 0});
    texture.setData(replacement);
    check(texture.modified() && replacement == texture.getTextureData(), "locked set did not retain input");
    (*replacement)[1] = 80;
    texture.unlock();
    check(!texture.modified() && !texture.locked(), "unlock state");
    check((*texture.getTextureData())[0] == 120 && (*texture.getTextureData())[1] == 80, "unlock upload stale");
    texture.unlock();
    check((*texture.getTextureData())[0] == 120, "second unlock changed data");
    std::vector<uint8_t> oversized(12, 55);
    backend->readTextureData(texture.backendId(), 4, false, oversized);
    check(oversized[8] == 0 && oversized[11] == 0, "queue readback padding");
    std::vector<uint8_t> missing(4, 55);
    backend->readTextureData(999999, 4, false, missing);
    check(missing == std::vector<uint8_t>(4, 55), "missing queue handle changed buffer");
    texture.setData(std::make_shared<std::vector<uint8_t>>(std::initializer_list<uint8_t>{255, 200, 100, 1}), 4);
    auto wrapped = texture.getTextureData(true);
    check((*wrapped)[0] == 1 && (*wrapped)[1] == 56 && (*wrapped)[2] == 156, "unmultiply must wrap, not saturate");
    setCurrentRenderBackend(nullptr);
    Texture noBackend(std::vector<uint8_t>{1, 2, 3, 4}, 1, 1);
    check(*noBackend.getTextureData() == std::vector<uint8_t>(4, 0), "no-handle read must be zero");
    noBackend.setData(std::make_shared<std::vector<uint8_t>>(4, 9));
    check(*noBackend.getTextureData() == std::vector<uint8_t>(4, 0), "no-handle write must be inert");
    std::cout << "Texture readback, staging identity, upload and unpremultiply passed\n";
}
