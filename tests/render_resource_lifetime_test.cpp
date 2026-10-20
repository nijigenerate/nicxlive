#include "nodes/drawable.hpp"
#include "render/shared_deform_buffer.hpp"
#include "runtime_state.hpp"
#include "puppet.hpp"
#include "texture.hpp"
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
        texture.reset();check(backend->textures.contains(id),"texture destructor disposed synchronously");
        inDrainPendingTextureDisposals();check(backend->textures.empty(),"texture not retired");
    }
    nodes::MeshData mesh;mesh.vertices={{0,0},{1,0},{0,1}};mesh.uvs=mesh.vertices;mesh.indices={0,1,2};
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
    auto last=std::make_shared<nodes::Drawable>(mesh,12,nullptr);
    last.reset();std::weak_ptr<Backend> weak=backend;
    inSetRenderBackend(nullptr);backend.reset();check(weak.expired(),"retirement queue owns backend");
    r::collectRenderResources();
    std::cout<<"PASS: deferred retirement, survivor integrity, empty atlases and repeated load/delete\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
