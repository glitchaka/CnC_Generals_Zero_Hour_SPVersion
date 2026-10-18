#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <gdiplus.h>
#include <string>
#include <vector>
#include <algorithm>
#include <cmath>

struct CC0Texture {
    int w=0,h=0;
    std::vector<unsigned int> rgba;
    bool valid() const { return w>0 && h>0 && !rgba.empty(); }
};

static std::wstring cc0ExeDir(){
    wchar_t buf[MAX_PATH]={0};
    GetModuleFileNameW(nullptr,buf,MAX_PATH);
    std::wstring p(buf);
    size_t k=p.find_last_of(L"\\/");
    return k==std::wstring::npos?L".":p.substr(0,k);
}

static bool cc0Exists(const std::wstring& p){ return GetFileAttributesW(p.c_str())!=INVALID_FILE_ATTRIBUTES; }

static std::wstring cc0FindAsset(const wchar_t* id){
    const wchar_t* exts[]={L".jpg",L".jpeg",L".png",L".bmp"};
    std::vector<std::wstring> roots={
        L"ExternalAssets\\PolyHaven\\",
        L"..\\..\\ExternalAssets\\PolyHaven\\",
        cc0ExeDir()+L"\\..\\..\\ExternalAssets\\PolyHaven\\"
    };
    for(const auto& root:roots){
        for(const wchar_t* ext:exts){
            std::wstring p=root+id+L"\\asset"+ext;
            if(cc0Exists(p)) return p;
        }
    }
    return L"";
}

static bool cc0Load(const std::wstring& path,CC0Texture& out){
    if(path.empty()) return false;
    Gdiplus::Bitmap bmp(path.c_str(),FALSE);
    if(bmp.GetLastStatus()!=Gdiplus::Ok) return false;
    const UINT w=bmp.GetWidth(),h=bmp.GetHeight();
    if(!w||!h) return false;
    Gdiplus::Rect r(0,0,(INT)w,(INT)h);
    Gdiplus::BitmapData bd={};
    if(bmp.LockBits(&r,Gdiplus::ImageLockModeRead,PixelFormat32bppARGB,&bd)!=Gdiplus::Ok) return false;
    out.w=(int)w; out.h=(int)h; out.rgba.resize((size_t)w*h);
    for(UINT y=0;y<h;++y){
        const unsigned char* src=(const unsigned char*)bd.Scan0+(ptrdiff_t)y*bd.Stride;
        unsigned int* dst=&out.rgba[(size_t)y*w];
        for(UINT x=0;x<w;++x){
            unsigned b=src[x*4+0],g=src[x*4+1],rr=src[x*4+2],a=src[x*4+3];
            dst[x]=(a<<24)|(rr<<16)|(g<<8)|b;
        }
    }
    bmp.UnlockBits(&bd);
    return true;
}

static COLORREF cc0Sample(const CC0Texture& t,float u,float v){
    if(!t.valid()) return RGB(96,88,70);
    u-=floorf(u); v-=floorf(v); if(u<0)u+=1; if(v<0)v+=1;
    float fx=u*(t.w-1),fy=v*(t.h-1);
    int x0=(int)fx,y0=(int)fy,x1=(x0+1)%t.w,y1=(y0+1)%t.h;
    float tx=fx-x0,ty=fy-y0;
    auto c=[&](int x,int y){return t.rgba[(size_t)y*t.w+x];};
    unsigned int c00=c(x0,y0),c10=c(x1,y0),c01=c(x0,y1),c11=c(x1,y1);
    auto ch=[&](int shift){
        float a=(float)((c00>>shift)&255),b=(float)((c10>>shift)&255),d=(float)((c01>>shift)&255),e=(float)((c11>>shift)&255);
        float p=a+(b-a)*tx,q=d+(e-d)*tx; return (int)std::lround(p+(q-p)*ty);
    };
    return RGB(ch(16),ch(8),ch(0));
}
