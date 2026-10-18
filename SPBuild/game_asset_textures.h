#pragma once
#define NOMINMAX
#include <windows.h>
#include <stdint.h>
#include <vector>
#include <string>
#include <algorithm>
#include <fstream>
#include <filesystem>

namespace ZHAssets {
struct Texture {
    int w=0,h=0;
    std::vector<uint32_t> px;
    std::wstring path;
    bool valid() const { return w>0 && h>0 && px.size()==(size_t)w*h; }
    uint32_t sample(float u,float v) const {
        if(!valid()) return 0xff557755u;
        u-=floorf(u); v-=floorf(v); if(u<0)u+=1; if(v<0)v+=1;
        int x=(int)(u*w)%w, y=(int)(v*h)%h;
        return px[(size_t)y*w+x];
    }
};

static inline uint16_t rd16(std::ifstream& f){ unsigned char b[2]; f.read((char*)b,2); return (uint16_t)(b[0]|(b[1]<<8)); }

static bool loadTGA(const std::wstring& path, Texture& out){
    std::ifstream f(std::filesystem::path(path),std::ios::binary); if(!f)return false;
    unsigned char idlen=0,cmap=0,type=0; f.read((char*)&idlen,1); f.read((char*)&cmap,1); f.read((char*)&type,1);
    unsigned char skip[5]; f.read((char*)skip,5); (void)rd16(f); (void)rd16(f); uint16_t w=rd16(f),h=rd16(f); unsigned char bpp=0,desc=0; f.read((char*)&bpp,1); f.read((char*)&desc,1);
    if(cmap!=0 || (type!=2 && type!=10) || (bpp!=24 && bpp!=32) || !w || !h) return false;
    if(idlen) f.seekg(idlen,std::ios::cur);
    out.w=w; out.h=h; out.px.assign((size_t)w*h,0xff000000u); out.path=path;
    const bool top=(desc&0x20)!=0; const int bytes=bpp/8; size_t total=(size_t)w*h, i=0;
    auto readPixel=[&]()->uint32_t{ unsigned char c[4]={0,0,0,255}; f.read((char*)c,bytes); return 0xff000000u|((uint32_t)c[2]<<16)|((uint32_t)c[1]<<8)|c[0]; };
    auto store=[&](size_t pos,uint32_t p){ size_t y=pos/w,x=pos%w; size_t yy=top?y:(h-1-y); out.px[yy*w+x]=p; };
    if(type==2){ for(;i<total && f;i++) store(i,readPixel()); }
    else {
        while(i<total && f){ unsigned char hdr=0; f.read((char*)&hdr,1); int count=(hdr&0x7f)+1; if(hdr&0x80){ uint32_t p=readPixel(); for(int k=0;k<count && i<total;k++,i++)store(i,p); } else { for(int k=0;k<count && i<total;k++,i++)store(i,readPixel()); } }
    }
    return f.good() || i==total;
}

static bool loadBMP(const std::wstring& path, Texture& out){
    HBITMAP hb=(HBITMAP)LoadImageW(NULL,path.c_str(),IMAGE_BITMAP,0,0,LR_LOADFROMFILE|LR_CREATEDIBSECTION); if(!hb)return false;
    BITMAP bm={}; GetObject(hb,sizeof(bm),&bm); if(bm.bmWidth<=0||bm.bmHeight==0){DeleteObject(hb);return false;}
    int w=bm.bmWidth,h=abs(bm.bmHeight); BITMAPINFO bi={}; bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER); bi.bmiHeader.biWidth=w; bi.bmiHeader.biHeight=-h; bi.bmiHeader.biPlanes=1; bi.bmiHeader.biBitCount=32; bi.bmiHeader.biCompression=BI_RGB;
    out.w=w;out.h=h;out.px.resize((size_t)w*h);out.path=path;
    HDC dc=GetDC(NULL); int got=GetDIBits(dc,hb,0,h,out.px.data(),&bi,DIB_RGB_COLORS); ReleaseDC(NULL,dc); DeleteObject(hb);
    if(!got){out={};return false;} for(uint32_t& p:out.px)p|=0xff000000u; return true;
}

static int scorePath(std::wstring s){
    std::transform(s.begin(),s.end(),s.begin(),::towlower); int q=0;
    if(s.find(L"terrain")!=std::wstring::npos)q+=120;
    if(s.find(L"grass")!=std::wstring::npos)q+=35;
    if(s.find(L"dirt")!=std::wstring::npos)q+=35;
    if(s.find(L"rock")!=std::wstring::npos)q+=20;
    if(s.find(L"sand")!=std::wstring::npos)q+=20;
    if(s.find(L"cliff")!=std::wstring::npos)q+=10;
    if(s.find(L"ui\\")!=std::wstring::npos||s.find(L"shell")!=std::wstring::npos||s.find(L"portrait")!=std::wstring::npos||s.find(L"button")!=std::wstring::npos||s.find(L"commandbar")!=std::wstring::npos)q-=200;
    return q;
}

static std::filesystem::path findRoot(){
    std::vector<std::filesystem::path> c={L"GameAssetsExtracted\\ZeroHour",L"..\\GameAssetsExtracted\\ZeroHour",L"..\\..\\GameAssetsExtracted\\ZeroHour"};
    wchar_t exe[MAX_PATH]={}; GetModuleFileNameW(NULL,exe,MAX_PATH); auto d=std::filesystem::path(exe).parent_path(); c.push_back(d/L"GameAssetsExtracted"/L"ZeroHour"); c.push_back(d/L".."/L".."/L"GameAssetsExtracted"/L"ZeroHour");
    for(auto&p:c){std::error_code ec;auto a=std::filesystem::weakly_canonical(p,ec);if(!ec&&std::filesystem::exists(a))return a;} return {};
}

static bool loadTerrainSet(std::vector<Texture>& textures,std::wstring& status){
    textures.clear(); auto root=findRoot(); if(root.empty()){status=L"Zero Hour assets: NOT FOUND";return false;}
    struct C{int score;std::filesystem::path p;}; std::vector<C> cand; std::error_code ec;
    for(auto it=std::filesystem::recursive_directory_iterator(root,std::filesystem::directory_options::skip_permission_denied,ec);!ec&&it!=std::filesystem::recursive_directory_iterator();it.increment(ec)){
        if(ec||!it->is_regular_file())continue; auto e=it->path().extension().wstring(); std::transform(e.begin(),e.end(),e.begin(),::towlower); if(e!=L".tga"&&e!=L".bmp")continue; int s=scorePath(it->path().wstring()); if(s>0)cand.push_back({s,it->path()});
    }
    std::sort(cand.begin(),cand.end(),[](const C&a,const C&b){return a.score>b.score;});
    for(const auto&c:cand){Texture t;auto e=c.p.extension().wstring();std::transform(e.begin(),e.end(),e.begin(),::towlower);bool ok=e==L".tga"?loadTGA(c.p.wstring(),t):loadBMP(c.p.wstring(),t);if(ok&&t.w>=32&&t.h>=32){textures.push_back(std::move(t));if(textures.size()>=8)break;}}
    status=L"Zero Hour terrain textures: "+std::to_wstring(textures.size()); return !textures.empty();
}

static COLORREF toColor(uint32_t p){return RGB((p>>16)&255,(p>>8)&255,p&255);} 
}
