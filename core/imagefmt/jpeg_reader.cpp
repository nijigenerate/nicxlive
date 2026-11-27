// Copyright 2019 Tero Hänninen. All rights reserved.
// C++ port of the independent imagefmt 2.1.2 jpeg.d. BSD-2-Clause; see LICENSE.imagefmt.
// IDCT retains the original attribution to Sean Barrett's public-domain stb_image.
#include "reader.hpp"
#include <compat/scope_exit.hpp>
#include <compat/array_range_error.hpp>
#include <compat/assert_error.hpp>
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstring>

namespace nicxlive::core::imagefmt::detail {
namespace {
using U8 = std::uint8_t;
using I16 = std::int16_t;
using Bytes = std::span<U8>;
using ConstBytes = std::span<const U8>;
template<class C> decltype(auto) at(C& a, std::size_t i, std::size_t line) {
    if (i >= a.size()) throw compat::ArrayIndexError(i, a.size(),
        compat::RetainedString::fromStaticLiteral("imagefmt/jpeg.d"), line);
    return a[i];
}
template<class T> std::span<T> slice(std::span<T> a, std::size_t first, std::size_t last, std::size_t line, const char* file = "imagefmt/jpeg.d") {
    if (first > last || last > a.size()) throw compat::ArraySliceError(first, last, a.size(),
        compat::RetainedString::fromStaticView(file), line);
    return a.subspan(first, last - first);
}
Bytes bytes(Buffer& b) { return {b.data(), b.size()}; }
I16 shortValue(int value) { return std::bit_cast<I16>(static_cast<std::uint16_t>(value)); }
int signedValue(std::uint32_t value) { return std::bit_cast<std::int32_t>(value); }
int add(int a, int b) { return signedValue(std::uint32_t(a) + std::uint32_t(b)); }
int mul(int a, int b) { return signedValue(std::uint32_t(a) * std::uint32_t(b)); }
int shift(int a, unsigned b) { return signedValue(std::uint32_t(a) << (b & 31)); }
void sourceAssert(bool value, std::size_t line) {
#ifndef NDEBUG
    if (!value) throw compat::AssertError{static_cast<const char*>(nullptr), "imagefmt/jpeg.d",
        static_cast<unsigned>(line)};
#else
    (void)value; (void)line;
#endif
}
[[noreturn]] void unreachable(std::size_t line) {
    compat::ngAssertUnreachable(static_cast<const char*>(nullptr), "imagefmt/jpeg.d",
        static_cast<unsigned>(line));
}
struct HuffTab {
    std::array<U8, 256> values{};
    std::array<U8, 257> sizes{};
    std::array<I16, 16> mincode{}, maxcode{}, valptr{};
};
struct Component {
    std::size_t x{}, y{};
    Bytes data;
    int pred{};
    U8 sfx{}, sfy{}, qtable{}, ac_table{}, dc_table{};
};
struct JPEGDecoder {
    Reader* rc{};
    std::array<std::array<U8, 64>, 4> qtables{};
    std::array<HuffTab, 2> ac_tables{}, dc_tables{};
    int bits_left{};
    U8 cb{};
    bool has_frame_header{}, eoi_reached{}, correct_comp_ids{};
    Buffer compsbuf;
    std::array<Component, 3> comps{};
    U8 num_comps{};
    int tchans{}, width{}, height{}, hmax{}, vmax{}, num_mcu_x{}, num_mcu_y{};
    std::uint16_t restart_interval{};
};
enum Marker : U8 { SOI=0xd8, SOF0=0xc0, SOF3=0xc3, SOF9=0xc9, SOF11=0xcb,
    DHT=0xc4, DQT=0xdb, DRI=0xdd, SOS=0xda, DNL=0xdc, RST0=0xd0, RST7=0xd7,
    APP0=0xe0, APPf=0xef, COM=0xfe, EOI=0xd9 };
ImageInfo infoerror(Error e) { ImageInfo info{}; info.e=e; return info; }
Error read_markers(JPEGDecoder&);
Error read_huffman_tables(JPEGDecoder&);
Error read_quantization_tables(JPEGDecoder&);
Error read_frame_header(JPEGDecoder&);
Error read_scan_header(JPEGDecoder&);
Error decode_scan(JPEGDecoder&);
Error decode_block(JPEGDecoder&, Component&, const std::array<U8,64>&, std::array<I16,64>&);
U8 decode_huff(JPEGDecoder&, const HuffTab&, Error&);
U8 nextbit(JPEGDecoder&, Error&);
int receive_and_extend(JPEGDecoder&, U8, Error&);
Buffer reconstruct(const JPEGDecoder&, Error&);
void stbi__idct_block(U8*, int, const std::array<I16,64>&);
void upsample_h2_v2(ConstBytes, ConstBytes, Bytes);
void upsample_h2_v1(ConstBytes, ConstBytes, Bytes);
void upsample_h1_v2(ConstBytes, ConstBytes, Bytes);
void upsample(const JPEGDecoder&, Bytes);
void upsample_luma(const JPEGDecoder&, Bytes);

void derive_mincode_maxcode_valptr(HuffTab& table, const std::array<I16,256>& codes, ConstBytes counts) {
    table.mincode.fill(-1); table.maxcode.fill(-1); table.valptr.fill(-1);
    int j=0;
    for (int i=0;i<16;++i) if (at(counts,i,355)) {
        at(table.valptr,i,356)=shortValue(j);
        at(table.mincode,i,357)=at(codes,j,357);
        j+=at(counts,i,358)-1;
        at(table.maxcode,i,359)=at(codes,j,359);
        ++j;
    }
}
void derive_table(HuffTab& table, ConstBytes counts, Error& e) {
    std::array<I16,256> codes{};
    int k=0;
    for (int i=0;i<16;++i) for (int j=0;j<at(counts,i,308);++j) {
        if (static_cast<std::size_t>(k)>table.sizes.size()) { e=Error::data; return; }
        at(table.sizes,k,313)=static_cast<U8>(i+1); ++k;
    }
    at(table.sizes,k,317)=0;
    k=0; I16 code=0; U8 si=at(table.sizes,k,321);
    for (;;) {
        do { at(codes,k,324)=code; code=shortValue(code+1); ++k; } while (si==at(table.sizes,k,327));
        if (at(table.sizes,k,329)==0) break;
        sourceAssert(si<at(table.sizes,k,332),332);
        do { code=shortValue(shift(code,1)); ++si; } while (si!=at(table.sizes,k,336));
    }
    derive_mincode_maxcode_valptr(table,codes,counts);
}
Error read_huffman_tables(JPEGDecoder& dc) {
    std::array<U8,17> tmp{};
    int len=dc.rc->big16()-2;
    if (dc.rc->fail) return Error::stream;
    Error e{};
    while (len>0) {
        dc.rc->block(tmp);
        if (dc.rc->fail) return Error::stream;
        const U8 slot=tmp[0]&15, kind=tmp[0]>>4;
        if (slot>1 || kind>1) return Error::unsupp;
        int mt=0; for (int i=1;i<17;++i) mt+=tmp[i];
        if (mt>256) return Error::data;
        auto& table=kind==0 ? dc.dc_tables[slot] : dc.ac_tables[slot];
        dc.rc->block(slice(Bytes(table.values),0,mt,kind==0 ? 288 : 291));
        derive_table(table,ConstBytes(tmp).subspan(1,16),e);
        len-=17+mt;
    }
    return dc.rc->fail ? Error::stream : e;
}
Error read_quantization_tables(JPEGDecoder& dc) {
    int len=dc.rc->big16();
    if (len%65!=2) return dc.rc->fail ? Error::stream : Error::data;
    len-=2;
    while (len>0) {
        const U8 info=dc.rc->byte(), slot=info&15, precision=info>>4;
        if (slot>3 || precision!=0) return dc.rc->fail ? Error::stream : Error::unsupp;
        dc.rc->block(dc.qtables[slot]); len-=65;
    }
    return dc.rc->fail ? Error::stream : Error::none;
}
Error read_frame_header(JPEGDecoder& dc) {
    const int len=dc.rc->big16(); const U8 precision=dc.rc->byte();
    dc.height=dc.rc->big16(); dc.width=dc.rc->big16(); dc.num_comps=dc.rc->byte();
    if ((dc.num_comps!=1 && dc.num_comps!=3) || precision!=8 || len!=8+dc.num_comps*3)
        return Error::unsupp;
    dc.hmax=dc.vmax=0; int mcu_du=0; std::array<U8,9> tmp{};
    dc.rc->block(slice(Bytes(tmp),0,dc.num_comps*3,401));
    if (dc.rc->fail) return Error::stream;
    for (int i=0;i<dc.num_comps;++i) {
        const U8 ci=tmp[i*3];
        if (i==0) dc.correct_comp_ids=ci==i+1;
        if ((dc.correct_comp_ids && ci!=i+1) || (!dc.correct_comp_ids && ci!=i)) return Error::data;
        auto& comp=dc.comps[i]; const U8 factors=tmp[i*3+1];
        comp.sfx=factors>>4; comp.sfy=factors&15; comp.qtable=tmp[i*3+2];
        if (comp.sfx<1 || comp.sfx>4 || comp.sfy<1 || comp.sfy>4 || comp.qtable>3) return Error::unsupp;
        dc.hmax=std::max(dc.hmax,int(comp.sfx)); dc.vmax=std::max(dc.vmax,int(comp.sfy));
        mcu_du+=comp.sfx*comp.sfy;
    }
    if (mcu_du>10) return Error::unsupp;
    sourceAssert(dc.hmax*dc.vmax!=0,430);
    for (int i=0;i<dc.num_comps;++i) {
        dc.comps[i].x=static_cast<std::size_t>(std::ceil(dc.width*(double(dc.comps[i].sfx)/dc.hmax)));
        dc.comps[i].y=static_cast<std::size_t>(std::ceil(dc.height*(double(dc.comps[i].sfy)/dc.vmax)));
    }
    const std::size_t w=dc.hmax*8,h=dc.vmax*8;
    dc.num_mcu_x=static_cast<int>((dc.width+w-1)/w); dc.num_mcu_y=static_cast<int>((dc.height+h-1)/h);
    return Error::none;
}
Error read_scan_header(JPEGDecoder& dc) {
    const auto len=dc.rc->big16(); const U8 count=dc.rc->byte();
    if (count!=dc.num_comps || len!=6+count*2) return dc.rc->fail ? Error::stream : Error::unsupp;
    std::array<U8,16> buf{}; dc.rc->block(slice(Bytes(buf),0,len-3,456));
    if (dc.rc->fail) return Error::stream;
    for (int i=0;i<count;++i) {
        const auto ci=static_cast<std::uint32_t>(buf[i*2]-(dc.correct_comp_ids?1:0));
        if (ci>=dc.num_comps) return Error::data;
        dc.comps[ci].dc_table=buf[i*2+1]>>4; dc.comps[ci].ac_table=buf[i*2+1]&15;
        if (dc.comps[ci].dc_table>1 || dc.comps[ci].ac_table>1) return Error::unsupp;
    }
    return Error::none;
}
Error read_markers(JPEGDecoder& dc) {
    bool next=false;
    while (!next && !dc.eoi_reached) {
        if (dc.rc->byte()!=255) return dc.rc->fail ? Error::stream : Error::data;
        U8 marker=dc.rc->byte();
        while (marker==255 && !dc.rc->fail) marker=dc.rc->byte();
        if (dc.rc->fail) return Error::stream;
        Error e{};
        switch (marker) {
        case DHT: e=read_huffman_tables(dc); break;
        case DQT: e=read_quantization_tables(dc); break;
        case SOF0:
            if (dc.has_frame_header) return Error::data;
            e=read_frame_header(dc); dc.has_frame_header=true; break;
        case SOS:
            if (!dc.has_frame_header) return Error::data;
            e=read_scan_header(dc); next=true; break;
        case DRI:
            if (dc.rc->big16()!=4) return dc.rc->fail ? Error::stream : Error::unsupp;
            dc.restart_interval=dc.rc->big16(); break;
        case EOI: dc.eoi_reached=true; break;
        default:
            if ((marker>=APP0 && marker<=APPf) || marker==COM) { const int len=dc.rc->big16()-2; dc.rc->skip(len); }
            else return Error::unsupp;
        }
        if (e!=Error::none) return dc.rc->fail ? Error::stream : e;
    }
    return Error::none;
}
constexpr std::array<U8,64> dezigzag{0,1,8,16,9,2,3,10,17,24,32,25,18,11,4,5,
    12,19,26,33,40,48,41,34,27,20,13,6,7,14,21,28,35,42,49,56,57,50,43,36,
    29,22,15,23,30,37,44,51,58,59,52,45,38,31,39,46,53,60,61,54,47,55,62,63};
U8 nextbit(JPEGDecoder& dc, Error& e) {
    if (!dc.bits_left) {
        dc.cb=dc.rc->byte(); dc.bits_left=8;
        if (dc.cb==255 && dc.rc->byte()!=0) { e=dc.rc->fail ? Error::stream : Error::data; return 0; }
        if (dc.rc->fail) e=Error::stream;
    }
    const U8 result=dc.cb>>7; dc.cb=static_cast<U8>(dc.cb<<1); --dc.bits_left; return result;
}
U8 decode_huff(JPEGDecoder& dc, const HuffTab& tab, Error& e) {
    I16 code=nextbit(dc,e); int i=0;
    while (at(tab.maxcode,i,621)<code) {
        const int bit=nextbit(dc,e); code=shortValue(add(shift(code,1),bit)); ++i;
        if (i>=int(tab.maxcode.size())) { e=Error::data; return 0; }
    }
    const auto j=static_cast<std::uint32_t>(tab.valptr[i]+code-tab.mincode[i]);
    if (j>=tab.values.size()) { e=Error::data; return 0; }
    return tab.values[j];
}
int receive_and_extend(JPEGDecoder& dc, U8 s, Error& e) {
    int symbol=0; for (int i=0;i<s;++i) { const int bit=nextbit(dc,e); symbol=add(shift(symbol,1),bit); }
    const int vt=shift(1,static_cast<unsigned>(s-1));
    return symbol<vt ? add(add(symbol,shift(-1,s)),1) : symbol;
}
Error decode_block(JPEGDecoder& dc, Component& comp, const std::array<U8,64>& q, std::array<I16,64>& result) {
    result.fill(0); Error e{};
    const U8 t=decode_huff(dc,dc.dc_tables[comp.dc_table],e); if (e!=Error::none) return e;
    const int diff=t?receive_and_extend(dc,t,e):0;
    comp.pred=add(comp.pred,diff); result[0]=shortValue(mul(comp.pred,q[0]));
    int k=1;
    do {
        const U8 rs=decode_huff(dc,dc.ac_tables[comp.ac_table],e); if (e!=Error::none) return e;
        const U8 r=rs>>4,s=rs&15;
        if (!s) { if (r!=15) break; k+=16; continue; }
        k+=r; if (k>63) return Error::data;
        result[dezigzag[k]]=shortValue(mul(receive_and_extend(dc,s,e),q[k])); ++k;
    } while (k<64);
    return Error::none;
}
Error read_restart(Reader& r) {
    const U8 a=r.byte(),b=r.byte(); if (r.fail) return Error::stream;
    return a!=255 || b<RST0 || b>RST7 ? Error::data : Error::none;
}
Error decode_scan(JPEGDecoder& dc) {
    int intervals{},mcus{};
    if (dc.restart_interval) {
        const int total=mul(dc.num_mcu_x,dc.num_mcu_y);
        intervals=add(add(total,dc.restart_interval),-1)/dc.restart_interval; mcus=dc.restart_interval;
    } else { intervals=1; mcus=mul(dc.num_mcu_x,dc.num_mcu_y); }
    Error e{};
    for (int j=0;j<dc.num_mcu_y;++j) for (int i=0;i<dc.num_mcu_x;++i) {
        for (int c=0;c<dc.num_comps;++c) {
            auto& comp=dc.comps[c];
            for (int y=0;y<comp.sfy;++y) for (int x=0;x<comp.sfx;++x) {
                std::array<I16,64> data{}; e=decode_block(dc,comp,dc.qtables[comp.qtable],data);
                if (e!=Error::none) return e;
                const int outx=mul(add(mul(i,comp.sfx),x),8);
                const int outy=mul(add(mul(j,comp.sfy),y),8);
                const int stride=mul(mul(dc.num_mcu_x,comp.sfx),8);
                const auto address=reinterpret_cast<std::uintptr_t>(comp.data.data()) +
                    static_cast<std::uintptr_t>(static_cast<std::intptr_t>(mul(outy,stride))) +
                    static_cast<std::uintptr_t>(static_cast<std::intptr_t>(outx));
                stbi__idct_block(reinterpret_cast<U8*>(address),stride,data);
            }
        }
        if (!--mcus) {
            if (!--intervals) return e;
            e=read_restart(*dc.rc);
            mcus=intervals==1 ? add(add(add(mul(add(add(dc.num_mcu_y,-j),-1),
                dc.num_mcu_x),dc.num_mcu_x),-i),-1) : dc.restart_interval;
            dc.cb=0; dc.bits_left=0; for (int k=0;k<dc.num_comps;++k) dc.comps[k].pred=0;
        }
    }
    return e;
}
U8 clamp(float x) { if (x<0) return 0; if (x>255) return 255; return static_cast<U8>(x); }
std::array<U8,3> ycbcr_to_rgb(U8 y,U8 cb,U8 cr) {
    return {clamp(static_cast<float>(y+1.402*(cr-128))),
        clamp(static_cast<float>(y-0.34414*(cb-128)-0.71414*(cr-128))),
        clamp(static_cast<float>(y+1.772*(cb-128)))};
}
void putRgb(Bytes result,std::size_t i,const std::array<U8,3>& rgb,std::size_t line) {
    const auto out=slice(result,i,i+3,line); std::copy(rgb.begin(),rgb.end(),out.begin());
}
void upsample_h2_v2(ConstBytes a,ConstBytes b,Bytes result) {
    const auto mix=[](U8 mm,U8 ms,U8 sm,U8 ss) { return U8((unsigned(mm)*9+unsigned(ms)*3+unsigned(sm)*3+ss+8)/16); };
    at(result,0, 793)=U8((unsigned(at(a,0, 793))*3+at(b,0, 794)+2)/4);
    if (a.size()==1) return;
    at(result,1, 798)=mix(at(a,0, 798),at(a,1, 798),at(b,0, 798),at(b,1, 798));
    std::size_t di=2;
    for (std::size_t i=1;i<a.size();++i) {
        at(result,di, 802)=mix(at(a,i, 802),at(a,i-1, 802),at(b,i, 802),at(b,i-1, 802)); ++di;
        if (i==a.size()-1) {
            if (di<result.size()) at(result,di, 806)=U8((unsigned(at(a,i, 806))*3+at(b,i, 807)+2)/4);
            return;
        }
        at(result,di, 812)=mix(at(a,i, 812),at(a,i+1, 812),at(b,i, 812),at(b,i+1, 812)); ++di;
    }
}
void upsample_h2_v1(ConstBytes a,ConstBytes,Bytes result) {
    at(result,0, 819)=at(a,0, 819); if (a.size()==1) return;
    at(result,1, 822)=U8((unsigned(at(a,0, 822))*3+at(a,1, 823)+2)/4);
    std::size_t di=2;
    for (std::size_t i=1;i<a.size();++i) {
        at(result,di, 827)=U8((unsigned(at(a,i-1, 827))+unsigned(at(a,i, 828))*3+2)/4); ++di;
        if (i==a.size()-1) { if (di<result.size()) at(result,di, 832)=at(a,i, 832); return; }
        at(result,di, 835)=U8((unsigned(at(a,i, 835))*3+at(a,i+1, 836)+2)/4); ++di;
    }
}
void upsample_h1_v2(ConstBytes a,ConstBytes b,Bytes result) {
    for (std::size_t i=0;i<result.size();++i) at(result,i, 845)=U8((unsigned(at(a,i, 845))*3+at(b,i, 846)+2)/4);
}
void upsample_luma(const JPEGDecoder& dc,Bytes result) {
    const std::size_t stride=dc.num_mcu_x*dc.comps[0].sfx*8;
    const float ys=float(dc.comps[0].sfy)/float(dc.vmax), xs=float(dc.comps[0].sfx)/float(dc.hmax);
    float y=static_cast<float>(ys*0.5); std::size_t yi=0,di=0;
    for (int j=0;j<dc.height;++j) {
        float x=static_cast<float>(xs*0.5); std::size_t xi=0;
        for (int i=0;i<dc.width;++i) {
            at(result,di, 867)=at(dc.comps[0].data,yi+xi, 867);
            if (dc.tchans==2) at(result,di+1, 869)=255;
            di+=dc.tchans; x+=xs;
            if (x>=1.0) { x=static_cast<float>(x-1.0); ++xi; }
        }
        y+=ys; if (y>=1.0) { y=static_cast<float>(y-1.0); yi+=stride; }
    }
}
void upsample(const JPEGDecoder& dc,Bytes result) {
    std::array<std::size_t,3> stride{},yi{};
    std::array<float,3> ys{},xs{},y{};
    for (int k=0;k<3;++k) {
        stride[k]=dc.num_mcu_x*dc.comps[k].sfx*8;
        ys[k]=float(dc.comps[k].sfy)/float(dc.vmax); xs[k]=float(dc.comps[k].sfx)/float(dc.hmax);
        y[k]=static_cast<float>(ys[k]*0.5);
    }
    std::size_t di=0;
    for (int j=0;j<dc.height;++j) {
        std::array<float,3> x{}; std::array<std::size_t,3> xi{};
        for (int k=0;k<3;++k) x[k]=static_cast<float>(xs[k]*0.5);
        for (int i=0;i<dc.width;++i) {
            putRgb(result,di,ycbcr_to_rgb(at(dc.comps[0].data,yi[0]+xi[0], 918),
                at(dc.comps[1].data,yi[1]+xi[1], 919),at(dc.comps[2].data,yi[2]+xi[2], 920)), 917);
            if (dc.tchans==4) at(result,di+3, 923)=255;
            di+=dc.tchans;
            for (int k=0;k<3;++k) x[k]+=xs[k];
            for (int k=0;k<3;++k) if (x[k]>=1.0) { x[k]=static_cast<float>(x[k]-1.0); ++xi[k]; }
        }
        for (int k=0;k<3;++k) y[k]+=ys[k];
        for (int k=0;k<3;++k) if (y[k]>=1.0) { y[k]=static_cast<float>(y[k]-1.0); yi[k]+=stride[k]; }
    }
}
Buffer reconstruct(const JPEGDecoder& dc,Error& e) {
    Buffer storage;
    if (!storage.sourceAllocate(static_cast<std::size_t>(mul(mul(dc.width,dc.height),dc.tchans)))) {
        e=Error::oom; return {};
    }
    auto result=bytes(storage);
    switch (dc.num_comps*10+dc.tchans) {
    case 34: case 33: {
        if (dc.comps[0].sfx<=2 && dc.comps[0].sfy<=2 && dc.comps[0].sfx+dc.comps[0].sfy>=3 &&
            dc.comps[1].sfx==1 && dc.comps[1].sfy==1 && dc.comps[2].sfx==1 && dc.comps[2].sfy==1) {
            void (*resample)(ConstBytes,ConstBytes,Bytes)=nullptr;
            switch (dc.comps[0].sfx*10+dc.comps[0].sfy) {
            case 22: resample=upsample_h2_v2; break;
            case 21: resample=upsample_h2_v1; break;
            case 12: resample=upsample_h1_v2; break;
            default: unreachable(678);
            }
            Buffer temporary;
            if (!temporary.sourceAllocate(static_cast<std::size_t>(mul(dc.width,2)))) { e=Error::oom; return {}; }
            d_runtime::ngRunWithScopeExit([&] {
            auto c1=slice(bytes(temporary),0,dc.width, 684),c2=slice(bytes(temporary),dc.width,temporary.size(), 685);
            std::size_t s=0,di=0;
            for (int j=0;j<dc.height;++j) {
                const std::size_t mi=j/dc.comps[0].sfy;
                const std::size_t si=(mi==0 || mi>=static_cast<std::size_t>((dc.height-1)/dc.comps[0].sfy)) ? mi : mi-1+s*2;
                s^=1;
                const std::size_t cs=dc.num_mcu_x*dc.comps[1].sfx*8,cl0=mi*cs,cl1=si*cs;
                resample(slice(ConstBytes(dc.comps[1].data),cl0,cl0+dc.comps[1].x, 698),
                    slice(ConstBytes(dc.comps[1].data),cl1,cl1+dc.comps[1].x, 699),c1);
                resample(slice(ConstBytes(dc.comps[2].data),cl0,cl0+dc.comps[2].x, 701),
                    slice(ConstBytes(dc.comps[2].data),cl1,cl1+dc.comps[2].x, 702),c2);
                for (int i=0;i<dc.width;++i) {
                    putRgb(result,di,ycbcr_to_rgb(at(dc.comps[0].data,
                        mul(mul(mul(j,dc.num_mcu_x),dc.comps[0].sfx),8)+i, 707),at(c1,i, 708),at(c2,i, 709)), 706);
                    if (dc.tchans==4) at(result,di+3, 712)=255;
                    di+=dc.tchans;
                }
            }
            }, [&] { temporary.sourceFree(); });
            return storage;
        }
        for (int i=0;i<dc.num_comps;++i) if (dc.comps[i].sfx!=dc.hmax || dc.comps[i].sfy!=dc.vmax) {
            upsample(dc,result); return storage;
        }
        std::size_t si=0,di=0;
        for (int j=0;j<dc.height;++j) {
            for (int i=0;i<dc.width;++i) {
                putRgb(result,di,ycbcr_to_rgb(at(dc.comps[0].data,si+i, 729),at(dc.comps[1].data,si+i, 730),at(dc.comps[2].data,si+i, 731)), 728);
                if (dc.tchans==4) at(result,di+3, 734)=255;
                di+=dc.tchans;
            }
            si+=dc.num_mcu_x*dc.comps[0].sfx*8;
        }
        return storage;
    }
    case 32: case 12: case 31: case 11: {
        const auto& comp=dc.comps[0];
        if (comp.sfx!=dc.hmax || comp.sfy!=dc.vmax) { upsample_luma(dc,result); return storage; }
        std::size_t si=0,di=0;
        for (int j=0;j<dc.height;++j) {
            if (dc.tchans==2) for (int i=0;i<dc.width;++i) { at(result,di++, 747)=at(comp.data,si+i, 747); at(result,di++, 748)=255; }
            else {
                const auto from=slice(ConstBytes(comp.data),si,si+dc.width, 754);
                auto to=slice(result,di,di+dc.width, 754); std::copy(from.begin(),from.end(),to.begin()); di+=dc.width;
            }
            si+=dc.num_mcu_x*comp.sfx*8;
        }
        return storage;
    }
    case 14: case 13: {
        const auto& comp=dc.comps[0]; std::size_t si=0,di=0;
        for (int j=0;j<dc.height;++j) {
            for (int i=0;i<dc.width;++i) {
                auto to=slice(result,di,di+3, 769); std::fill(to.begin(),to.end(),at(comp.data,si+i, 769));
                if (dc.tchans==4) at(result,di+3, 771)=255;
                di+=dc.tchans;
            }
            si+=dc.num_mcu_x*comp.sfx*8;
        }
        return storage;
    }
    default: unreachable(778);
    }
}

// Explicit 32-bit wrap avoids C++ signed overflow in the source integer IDCT.
struct Word {
    std::uint32_t bits{};
    Word()=default;
    Word(int value):bits(static_cast<std::uint32_t>(value)) {}
    int value() const { return signedValue(bits); }
    friend Word operator+(Word a,Word b) { Word r; r.bits=a.bits+b.bits; return r; }
    friend Word operator-(Word a,Word b) { Word r; r.bits=a.bits-b.bits; return r; }
    friend Word operator*(Word a,Word b) { Word r; r.bits=a.bits*b.bits; return r; }
    friend Word operator<<(Word a,int n) { Word r; r.bits=a.bits<<(n&31); return r; }
    friend Word operator>>(Word a,int n) { return Word(a.value()>>(n&31)); }
    Word& operator+=(Word b) { return *this=*this+b; }
};
Word f2f(float x) { return Word(static_cast<int>(double(x*4096)+0.5)); }
Word fsh(Word x) { return x<<12; }
U8 stbi__clamp(Word x) {
    if (x.bits>255) { if (x.value()<0) return 0; if (x.value()>255) return 255; }
    return static_cast<U8>(x.bits);
}

void STBI__IDCT_1D(Word& t0,Word& t1,Word& t2,Word& t3, Word& x0,Word& x1,Word& x2,Word& x3,
    Word s0,Word s1,Word s2,Word s3,Word s4,Word s5,Word s6,Word s7)
{
   Word p1,p2,p3,p4,p5;
   //int t0,t1,t2,t3,p1,p2,p3,p4,p5,x0,x1,x2,x3;
   p2 = s2;
   p3 = s6;
   p1 = (p2+p3) * f2f(0.5411961f);
   t2 = p1 + p3 * f2f(-1.847759065f);
   t3 = p1 + p2 * f2f( 0.765366865f);
   p2 = s0;
   p3 = s4;
   t0 = fsh(p2+p3);
   t1 = fsh(p2-p3);
   x0 = t0+t3;
   x3 = t0-t3;
   x1 = t1+t2;
   x2 = t1-t2;
   t0 = s7;
   t1 = s5;
   t2 = s3;
   t3 = s1;
   p3 = t0+t2;
   p4 = t1+t3;
   p1 = t0+t3;
   p2 = t1+t2;
   p5 = (p3+p4)*f2f( 1.175875602f);
   t0 = t0*f2f( 0.298631336f);
   t1 = t1*f2f( 2.053119869f);
   t2 = t2*f2f( 3.072711026f);
   t3 = t3*f2f( 1.501321110f);
   p1 = p5 + p1*f2f(-0.899976223f);
   p2 = p5 + p2*f2f(-2.562915447f);
   p3 = p3*f2f(-1.961570560f);
   p4 = p4*f2f(-0.390180644f);
   t3 += p1+p4;
   t2 += p2+p3;
   t1 += p2+p4;
   t0 += p1+p3;
}


void stbi__idct_block(U8* dst, int dst_stride, const std::array<I16,64>& data)
{
   int i;
   std::array<Word,64> val{};
   Word* v = val.data();
   const I16* d = data.data();

   // columns
   for (i=0; i < 8; ++i,++d, ++v) {
      // if all zeroes, shortcut -- this avoids dequantizing 0s and IDCTing
      if (d[ 8]==0 && d[16]==0 && d[24]==0 && d[32]==0
           && d[40]==0 && d[48]==0 && d[56]==0) {
         //    no shortcut                 0     seconds
         //    (1|2|3|4|5|6|7)==0          0     seconds
         //    all separate               -0.047 seconds
         //    1 && 2|3 && 4|5 && 6|7:    -0.047 seconds
         Word dcterm = Word(d[0]) << 2;
         v[0] = v[8] = v[16] = v[24] = v[32] = v[40] = v[48] = v[56] = dcterm;
      } else {
         Word t0,t1,t2,t3,x0,x1,x2,x3;
         STBI__IDCT_1D(
             t0, t1, t2, t3,
             x0, x1, x2, x3,
             d[ 0], d[ 8], d[16], d[24],
             d[32], d[40], d[48], d[56]
         );
         // constants scaled things up by 1<<12; let's bring them back
         // down, but keep 2 extra bits of precision
         x0 += 512; x1 += 512; x2 += 512; x3 += 512;
         v[ 0] = (x0+t3) >> 10;
         v[56] = (x0-t3) >> 10;
         v[ 8] = (x1+t2) >> 10;
         v[48] = (x1-t2) >> 10;
         v[16] = (x2+t1) >> 10;
         v[40] = (x2-t1) >> 10;
         v[24] = (x3+t0) >> 10;
         v[32] = (x3-t0) >> 10;
      }
   }

   U8* o = dst;
   for (i=0, v=val.data(); i < 8; ++i,v+=8,o+=dst_stride) {
      // no fast case since the first 1D IDCT spread components out
      Word t0,t1,t2,t3,x0,x1,x2,x3;
      STBI__IDCT_1D(
          t0, t1, t2, t3,
          x0, x1, x2, x3,
          v[0],v[1],v[2],v[3],v[4],v[5],v[6],v[7]
      );
      // constants scaled things up by 1<<12, plus we had 1<<2 from first
      // loop, plus horizontal and vertical each scale by sqrt(8) so together
      // we've got an extra 1<<3, so 1<<17 total we need to remove.
      // so we want to round that, which means adding 0.5 * 1<<17,
      // aka 65536. Also, we'll end up with -128 to 127 that we want
      // to encode as 0-255 by adding 128, so we'll add that before the shift
      x0 += 65536 + (128<<17);
      x1 += 65536 + (128<<17);
      x2 += 65536 + (128<<17);
      x3 += 65536 + (128<<17);
      // tried computing the shifts into temps, or'ing the temps to see
      // if any were out of range, but that was slower
      o[0] = stbi__clamp((x0+t3) >> 17);
      o[7] = stbi__clamp((x0-t3) >> 17);
      o[1] = stbi__clamp((x1+t2) >> 17);
      o[6] = stbi__clamp((x1-t2) >> 17);
      o[2] = stbi__clamp((x2+t1) >> 17);
      o[5] = stbi__clamp((x2-t1) >> 17);
      o[3] = stbi__clamp((x3+t0) >> 17);
      o[4] = stbi__clamp((x3-t0) >> 17);
   }
}

} // namespace

ImageInfo readJpegInfo(Reader& reader) {
    if (reader.byte() != 0xff || reader.byte() != SOI)
        return infoerror(reader.fail ? Error::stream : Error::data);
    for (;;) {
        if (reader.byte() != 0xff) return infoerror(reader.fail ? Error::stream : Error::data);
        U8 marker = reader.byte();
        while (marker == 0xff && !reader.fail) marker = reader.byte();
        if (reader.fail) return infoerror(Error::stream);
        if ((marker >= SOF0 && marker <= SOF3) || (marker >= SOF9 && marker <= SOF11)) {
            reader.skip(3);
            ImageInfo result;
            result.h = reader.big16();
            result.w = reader.big16();
            result.c = reader.byte();
            result.e = reader.fail ? Error::stream : Error::none;
            return result;
        }
        if (marker == SOS || marker == EOI) return infoerror(Error::data);
        if (marker == DRI || marker == DHT || marker == DQT || marker == COM ||
                (marker >= APP0 && marker <= APPf)) {
            const int length = reader.big16() - 2;
            reader.skip(length);
        } else return infoerror(Error::unsupp);
    }
}

bool detectJpeg(Reader& reader) {
    const auto info = readJpegInfo(reader);
    reader.reset();
    return info.e == Error::none;
}

Error readJpeg(Reader& reader, Image& image, int channels, int bits) {
    image = Image{};
    if (static_cast<unsigned>(channels) > 4) return Error::arg;
    const auto targetBits = static_cast<U8>(bits ? bits : 8);
    if (targetBits != 8 && targetBits != 16) return Error::unsupp;
    if (reader.byte() != 0xff || reader.byte() != SOI)
        return reader.fail ? Error::stream : Error::data;
    if (reader.fail) return Error::stream;
    JPEGDecoder decoder;
    decoder.rc = &reader;
    auto error = read_markers(decoder);
    if (error != Error::none) return error;
    if (decoder.eoi_reached) return Error::data;
    decoder.tchans = channels == 0 ? decoder.num_comps : channels;
    if (std::uint64_t(decoder.width) * decoder.height * decoder.tchans > maximumImageSize)
        return Error::bigimg;
    std::array<std::size_t, 3> sizes{};
    std::size_t total = 0;
    for (int i = 0; i < decoder.num_comps; ++i) {
        const auto& component = decoder.comps[i];
        sizes[i] = static_cast<std::size_t>(mul(mul(mul(mul(mul(decoder.num_mcu_x,
            component.sfx), 8), decoder.num_mcu_y), component.sfy), 8));
        total += sizes[i];
    }
    if (!decoder.compsbuf.sourceAllocate(total)) return Error::oom;
    total = 0;
    for (int i = 0; i < decoder.num_comps; ++i) {
        decoder.comps[i].data = slice(bytes(decoder.compsbuf), total, total + sizes[i], 173);
        total += sizes[i];
    }
    Error completed = Error::none;
    d_runtime::ngRunWithScopeExit([&] {
        completed = [&]() -> Error {
            error = decode_scan(decoder);
            if (error != Error::none) return error;
            auto pixels = reconstruct(decoder, error);
            if (error != Error::none) return error;
            if (verticalOrientationRead == -1) {
                const int stride = mul(decoder.width, decoder.tchans);
                Buffer temporary;
                // The original stores but does not return a flip allocation error.
                const bool allocated = temporary.sourceAllocate(static_cast<std::size_t>(stride));
                d_runtime::ngRunWithScopeExit([&] {
                if (allocated) {
                    for (int front = 0, back = mul(decoder.height - 1, stride); front < back;
                            front = add(front, stride), back = add(back, -stride)) {
                        const auto fromFront = slice(bytes(pixels), front, add(front, stride),
                            1068, "imagefmt/package.d");
                        auto swapTarget = slice(bytes(temporary), 0, stride, 1068, "imagefmt/package.d");
                        std::copy(fromFront.begin(), fromFront.end(), swapTarget.begin());
                        const auto fromBack = slice(bytes(pixels), back, add(back, stride),
                            1069, "imagefmt/package.d");
                        auto toFront = slice(bytes(pixels), front, add(front, stride),
                            1069, "imagefmt/package.d");
                        std::copy(fromBack.begin(), fromBack.end(), toFront.begin());
                        auto toBack = slice(bytes(pixels), back, add(back, stride),
                            1070, "imagefmt/package.d");
                        std::copy(temporary.data(), temporary.data() + temporary.size(), toBack.begin());
                    }
                }
                }, [&] { temporary.sourceFree(); });
            }
            image.w = decoder.width;
            image.h = decoder.height;
            image.c = static_cast<U8>(decoder.tchans);
            image.cinfile = decoder.num_comps;
            image.bpc = targetBits;
            if (targetBits == 16 && !convertBits(pixels, 8, 16)) return Error::oom;
            image.buffer = std::move(pixels);
            return Error::none;
        }();
    }, [&] { decoder.compsbuf.sourceFree(); });
    return completed;
}

} // namespace nicxlive::core::imagefmt::detail


