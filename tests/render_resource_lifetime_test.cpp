#include "nodes/drawable.hpp"
#include "render/shared_deform_buffer.hpp"
#include "runtime_state.hpp"
#include "puppet.hpp"
#include "texture.hpp"
#include "render/backend_queue.hpp"
#include "unity_native.hpp"
#include <filesystem>
#include <chrono>
#include <map>
#include <iostream>
#include <stdexcept>
using namespace nicxlive::core;
namespace r=nicxlive::core::render;
void check(bool b,const char* m){if(!b)throw std::runtime_error(m);}
struct Backend:RenderBackend {
    uint32_t next=1;size_t batches=0,vertex=0,uv=0,deform=0;
    std::map<uint32_t,std::vector<uint16_t>> indices;
    std::map<uint32_t,std::vector<uint8_t>> textures;
    uint32_t createTexture(const std::vector<uint8_t>& data,int,int,int,bool)override {
        auto id=next++;textures[id]=data;return id;
    }
    void updateTexture(uint32_t id,const std::vector<uint8_t>& data,int,int,int)override {textures.at(id)=data;}
    void disposeTexture(uint32_t id)override {textures.erase(id);}
    void createDrawableBuffers(uint32_t& id)override{id=next++;}
    void uploadDrawableIndices(uint32_t id,const std::vector<uint16_t>& data)override{indices[id]=data;}
    void disposeDrawableBuffers(uint32_t id)override{indices.erase(id);}
    void collectDrawableBuffers()override{++batches;}
    void uploadSharedVertexBuffer(const common::Vec2Array& v)override{vertex=v.size();}
    void uploadSharedUvBuffer(const common::Vec2Array& v)override{uv=v.size();}
    void uploadSharedDeformBuffer(const common::Vec2Array& v)override{deform=v.size();}
};
int main(){try{
    inSetRenderBackend(nullptr);
    {
        auto standalone=std::make_shared<Puppet>();
        check(getCurrentRenderBackend()!=nullptr,"standalone Puppet has no default backend");
    }
    auto backend=std::make_shared<Backend>();inSetRenderBackend(backend);
    {
        auto puppet=std::make_shared<Puppet>();
        check(getCurrentRenderBackend()==backend,"Puppet construction replaced host backend");
        puppet->getRootParts().clear();puppet->getDrivers().clear();
        auto texture=std::make_shared<Texture>(std::vector<uint8_t>{12,34,56},1,1,3,4,false,false);
        const auto id=texture->backendId();
        check(backend->textures.at(id)==std::vector<uint8_t>({12,34,56,255}),"RGB input was not converted to RGBA");
        texture->lock();texture->setData({7,8,9,10},-1);
        check(backend->textures.at(id)[0]==12,"locked texture updated early");
        texture->unlock();
        check(backend->textures.at(id)==std::vector<uint8_t>({7,8,9,10}),"unlock lost texture data");
        texture->setData({21,22,23},3);
        check(texture->channels()==4 && backend->textures.at(id)==std::vector<uint8_t>({21,22,23,255}),"input channels changed stored format");
        bool rejected=false;try{texture->setData({1},3);}catch(const std::invalid_argument&){rejected=true;}
        check(rejected,"truncated texture data accepted");
        texture->setData({37,99},2);
        check(backend->textures.at(id)==std::vector<uint8_t>({37,37,37,99}),"gray alpha expansion lost luminance or alpha");
        texture->setData({42},1);
        check(backend->textures.at(id)==std::vector<uint8_t>({42,42,42,255}),"gray expansion lost luminance");
        texture.reset();check(backend->textures.contains(id),"texture destructor disposed synchronously");
        inDrainPendingTextureDisposals();check(backend->textures.empty(),"texture not retired");
    }
    {
        auto other=std::make_shared<Backend>();
        auto texture=std::make_shared<Texture>(std::vector<uint8_t>{1,2,3,255},1,1,4,4,false,false);
        auto id=texture->backendId(); texture.reset();
        inSetRenderBackend(other); inDrainPendingTextureDisposals();
        check(backend->textures.contains(id),"texture disposal ran on wrong current context");
        inSetRenderBackend(backend); inDrainPendingTextureDisposals();
        check(!backend->textures.contains(id),"texture disposal missing on owner context");
        inSetRenderBackend(other);
        auto late=std::make_shared<Texture>(std::vector<uint8_t>{1,2,3,255},1,1,4,4,false,false);
        late.reset(); std::weak_ptr<Backend> weakOther=other;
        inSetRenderBackend(backend); other.reset();
        check(weakOther.expired(),"pending texture disposal owns backend");
        inDrainPendingTextureDisposals();
    }
    nodes::MeshData mesh;mesh.vertices={{0,0},{1,0},{0,1}};mesh.uvs=mesh.vertices;mesh.indices={0,1,2};
    {
        auto drawable=std::make_shared<nodes::Drawable>(mesh,14,nullptr);
        nodes::inSetUpdateBounds(false);
        auto fallback=drawable->getCombinedBounds();
        for(float value:fallback)check(std::isfinite(value),"missing cached bounds returned NaN");
        auto puppet=std::make_shared<Puppet>();drawable->setPuppet(puppet);
        drawable->bounds=std::array<float,4>{1,2,3,4};
        puppet->transform.scale={-1,-1};puppet->transform.update();
        auto mirrored=drawable->getCombinedBounds(false,true);
        check(mirrored[0]==-3 && mirrored[1]==-4 && mirrored[2]==-1 && mirrored[3]==-2,"mirrored bounds not normalized");
    }
    r::collectRenderResources();
    for(int cycle=0;cycle<8;++cycle){
        auto first=std::make_shared<nodes::Drawable>(mesh,10,nullptr);
        auto second=std::make_shared<nodes::Drawable>(mesh,11,nullptr);
        r::collectRenderResources();
        check(backend->indices.size()==2,"two drawables not allocated");
        auto remaining=second->ibo;first.reset();
        check(backend->indices.size()==2,"destructor called backend directly");
        r::collectRenderResources();
        check(backend->indices.size()==1&&backend->indices.at(remaining)==mesh.indices,"remaining drawable changed");
        second.reset();r::collectRenderResources();
        check(backend->indices.empty(),"index retirement leak");
        check(!backend->vertex&&!backend->uv&&!backend->deform,"empty atlas was not sent to backend");
        check(!r::sharedVertexAtlasStride()&&!r::sharedUvAtlasStride()&&!r::sharedDeformAtlasStride(),"CPU atlas retained data");
    }
    {
        auto other=std::make_shared<Backend>();
        auto first=std::make_shared<nodes::Drawable>(mesh,16,nullptr);
        inSetRenderBackend(other);
        auto second=std::make_shared<nodes::Drawable>(mesh,17,nullptr);
        r::syncSharedBuffers(backend); r::syncSharedBuffers(other);
        check(backend->vertex==6 && other->vertex==6,"initial multi-backend upload failed");
        first.reset(); r::collectRenderResources();
        check(backend->indices.size()==1,"retirement ran on the wrong current context");
        inSetRenderBackend(backend); r::collectRenderResources();
        check(backend->indices.empty(),"owner context failed to retire indices");
        check(backend->vertex==3 && other->vertex==3,"current-context compaction uploads failed");
        r::syncSharedBuffers(other);
        check(other->vertex==3 && other->uv==3 && other->deform==3,"second backend missed atlas compaction");
        second.reset(); r::collectRenderResources(); r::syncSharedBuffers(other);
        check(!other->vertex && !other->uv && !other->deform,"second backend retained empty atlas");
        check(other->indices.size()==1,"second retirement ran on wrong context");
        inSetRenderBackend(other); r::collectRenderResources();
        check(other->indices.empty(),"second owner failed to collect");
        inSetRenderBackend(backend);
    }
    {
        auto queue=std::make_shared<r::QueueRenderBackend>();
        inSetRenderBackend(queue);
        auto puppet=std::make_shared<Puppet>();
        check(puppet->commandEmitter()!=nullptr,"queue puppet has no emitter");
        nodes::PartDrawPacket packet{};
        puppet->commandEmitter()->drawPartPacket(packet);
        check(queue->queue.size()==1,"Puppet emitter did not adopt the host queue");
        auto drawable=std::make_shared<nodes::Drawable>(mesh,13,nullptr);
        check(queue->drawableIndexBufferCount()==1,"queue geometry not registered");
        drawable.reset();r::collectRenderResources();
        check(queue->drawableIndexBufferCount()==0,"queue backend retained retired indices");
        inSetRenderBackend(backend);
    }
    {
        const auto path=std::filesystem::temp_directory_path()/(
            "nicxlive-retirement-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".inp");
        struct TempFile {std::filesystem::path path;~TempFile(){std::error_code error;std::filesystem::remove(path,error);}} cleanup{path};
        // An empty puppet suffices to exercise the last-handle unload path.
        { Puppet fixture;
          fixture.textureSlots={nullptr, std::make_shared<Texture>(), std::make_shared<Texture>(std::vector<uint8_t>{3,4,5,255},1,1,4,4,false,false)};
          nicxlive::core::fmt::inWriteINPPuppet(fixture,path.string()); }
        { auto loaded=fmt::inLoadPuppet<Puppet>(path.string());
          check(loaded->textureSlots.size()==3 && !loaded->textureSlots[0] && !loaded->textureSlots[1] && loaded->textureSlots[2],"empty texture slots failed roundtrip"); }
        inDrainPendingTextureDisposals();
        UnityRendererConfig config{32,32};void* renderer=nullptr;void* puppet=nullptr;
        check(njgCreateRenderer(&config,nullptr,&renderer)==NjgResult::Ok,"C API renderer creation failed");
        auto queue=std::dynamic_pointer_cast<r::QueueRenderBackend>(getCurrentRenderBackend());
        void* otherRenderer=nullptr;
        check(njgCreateRenderer(&config,nullptr,&otherRenderer)==NjgResult::Ok,"second renderer creation failed");
        auto otherQueue=std::dynamic_pointer_cast<r::QueueRenderBackend>(getCurrentRenderBackend());
        queue->resourceQueue.clear(); otherQueue->resourceQueue.clear();
        check(njgLoadPuppet(renderer,path.string().c_str(),&puppet)==NjgResult::Ok,"C API puppet load failed");
        check(!queue->resourceQueue.empty() && otherQueue->resourceQueue.empty(),"load allocated textures on wrong renderer");
        check(getCurrentRenderBackend()==otherQueue,"load did not restore calling backend");
        inSetRenderBackend(queue);
        std::weak_ptr<Texture> pendingTexture;
        { DynamicCompositePass pass;
          pass.textures.push_back(std::make_shared<Texture>(std::vector<uint8_t>{1,2,3,255},1,1,4,4,false,false));
          pendingTexture=pass.textures[0]; queue->beginDynamicComposite(pass); queue->endDynamicComposite(pass); }
        check(!pendingTexture.expired(),"test did not retain queued surface");
        otherQueue->beginMask(false);
        check(njgUnloadPuppet(otherRenderer,puppet)==NjgResult::InvalidArgument,"cross-renderer unload accepted");
        check(otherQueue->queue.size()==1,"invalid unload erased unrelated renderer commands");
        // Queue a real Drawable retirement before unload; no subsequent tick is allowed.
        { auto drawable=std::make_shared<nodes::Drawable>(mesh,15,nullptr); }
        check(queue->drawableIndexBufferCount()==1,"C API renderer has no pending retirement");
        check(njgUnloadPuppet(renderer,puppet)==NjgResult::Ok,"C API unload failed");
        check(queue->drawableIndexBufferCount()==0,"last C API puppet unload left pending indices");
        check(pendingTexture.expired(),"unload retained queued surface texture");
        check(std::any_of(queue->resourceQueue.begin(),queue->resourceQueue.end(),[](const auto& command){return command.kind==r::TextureCommandKind::Dispose;}),"unload did not drain surface texture disposal");
        queue->beginMask(false);
        check(njgUnloadPuppet(renderer,puppet)==NjgResult::InvalidArgument,"double unload accepted");
        check(queue->queue.size()==1,"double unload erased renderer commands");
        // A late texture error must retire geometry and earlier valid textures
        // before restoring the caller's backend, without a subsequent tick.
        const std::string geometry=R"({"nodes":{"type":"Part","uuid":91,"mesh":{"verts":[0,0,1,0,0,1],"uvs":[0,0,1,0,0,1],"indices":[0,1,2]}}})";
        inSetRenderBackend(queue);
        { auto probe=fmt::inLoadJSONPuppet<Puppet>(geometry);
          check(queue->drawableIndexBufferCount()>0,"failure fixture contains no allocated geometry"); }
        r::collectRenderResources();
        std::vector<uint8_t> invalid(fmt::MAGIC_BYTES.begin(),fmt::MAGIC_BYTES.end());
        auto append32=[&](uint32_t n){for(int shift:{24,16,8,0})invalid.push_back(static_cast<uint8_t>(n>>shift));};
        append32(static_cast<uint32_t>(geometry.size())); invalid.insert(invalid.end(),geometry.begin(),geometry.end());
        invalid.insert(invalid.end(),fmt::TEX_SECTION.begin(),fmt::TEX_SECTION.end()); append32(2);
        auto good=fmt::encodeTga({1,2,3,255},1,1,4);
        append32(static_cast<uint32_t>(good.size())); invalid.push_back(0); invalid.insert(invalid.end(),good.begin(),good.end());
        append32(3); invalid.push_back(0); invalid.insert(invalid.end(),{1,2,3});
        { std::ofstream file(path,std::ios::binary|std::ios::trunc); file.write(reinterpret_cast<const char*>(invalid.data()),static_cast<std::streamsize>(invalid.size())); }
        inSetRenderBackend(otherQueue);
        for(int attempt=0;attempt<3;++attempt) {
            queue->resourceQueue.clear(); void* failed=nullptr;
            check(njgLoadPuppet(renderer,path.string().c_str(),&failed)==NjgResult::Failure,"invalid texture load succeeded");
            check(getCurrentRenderBackend()==otherQueue,"failed load did not restore caller");
            check(queue->drawableIndexBufferCount()==0,"failed load retained indices");
            check(std::count_if(queue->resourceQueue.begin(),queue->resourceQueue.end(),[](const auto& c){return c.kind==r::TextureCommandKind::Dispose;})==1,"failed load retained earlier valid texture");
        }
        njgDestroyRenderer(otherRenderer);
        njgDestroyRenderer(renderer);njgRuntimeTerm();inSetRenderBackend(backend);
    }
    auto last=std::make_shared<nodes::Drawable>(mesh,12,nullptr);
    last.reset();std::weak_ptr<Backend> weak=backend;
    inSetRenderBackend(nullptr);backend.reset();check(weak.expired(),"retirement queue owns backend");
    r::collectRenderResources();
    std::cout<<"PASS: deferred retirement, survivor integrity, empty atlases and repeated load/delete\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
