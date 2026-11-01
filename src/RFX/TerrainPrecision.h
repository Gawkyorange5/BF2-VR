#pragma once
#include <d3dx9.h>
#include <string>
#include <map>
#include <cstdint>

// Private r148 experiment. BF2's stock depth/low-detail/mountain shaders use
// a fused height-scale minus morph. FullDetail and FullDetailWithEnvMap add
// the always-zero Y translation first, producing a separately rounded height.
// Match the stock mountain/depth expression, without altering depth tests,
// textures, LOD, shadows, MSAA or the stereo renderer. No files are rewritten.
// Exact source identities deliberately exclude modified/unknown mod shaders.
namespace TerrainPrecision {
inline std::string Normalize(const void* bytes,UINT length){
    std::string out;out.reserve(length);
    const char* p=static_cast<const char*>(bytes);
    for(UINT i=0;i<length;++i)if(p[i]!='\r')out.push_back(p[i]);
    return out;
}
inline uint64_t Hash(const std::string& s){
    uint64_t h=14695981039346656037ull;
    for(unsigned char c:s){h^=c;h*=1099511628211ull;}return h;
}
inline bool Shared(const std::string& s){return s.size()==17946&&Hash(s)==0x8baee9f1e022311aull;}
inline bool Patch(std::string& s,bool sharedVerified){
    if(!sharedVerified||s.size()!=32395||Hash(s)!=0x5e59c84bc6db580aull)return false;
    const std::string from="wPos.yw = (indata.Pos1.xw * vScaleTransY.xy) + vScaleTransY.zw;";
    const std::string to="wPos.yw = indata.Pos1.xw * vScaleTransY.xy;";
    auto a=s.find(from);if(a==std::string::npos)return false;
    auto b=s.find(from,a+from.size());if(b==std::string::npos||s.find(from,b+from.size())!=std::string::npos)return false;
    s.replace(b,from.size(),to);s.replace(a,from.size(),to);return true;
}
class Includes:public ID3DXInclude {
    struct Buffer{LPCVOID original;std::string text;};
    ID3DXInclude* native;
    std::map<LPCVOID,Buffer*> buffers;
    bool shared=false;
public:
    unsigned applied=0,unknown=0;
    explicit Includes(ID3DXInclude* p):native(p){}
    ~Includes(){for(auto& entry:buffers){native->Close(entry.second->original);delete entry.second;}}
    HRESULT __stdcall Open(D3DXINCLUDE_TYPE type,LPCSTR name,LPCVOID parent,LPCVOID* data,UINT* bytes) override{
        auto p=buffers.find(parent);if(p!=buffers.end())parent=p->second->original;
        const HRESULT hr=native->Open(type,name,parent,data,bytes);if(FAILED(hr))return hr;
        const char* base=name;for(const char* c=name;*c;++c)if(*c=='/'||*c=='\\')base=c+1;
        if(_stricmp(base,"TerrainShader_Shared.fx")&&_stricmp(base,"TerrainShader_Hi.fx"))return hr;
        if(!*data||*bytes>1024*1024){++unknown;return hr;}
        std::string text=Normalize(*data,*bytes);
        if(!_stricmp(base,"TerrainShader_Shared.fx")){shared=Shared(text);if(!shared)++unknown;return hr;}
        if(!Patch(text,shared)){++unknown;return hr;}
        auto buffer=new Buffer{*data,std::move(text)};
        const void* replacement=buffer->text.data();buffers.emplace(replacement,buffer);
        *data=replacement;*bytes=static_cast<UINT>(buffer->text.size());++applied;return hr;
    }
    HRESULT __stdcall Close(LPCVOID data) override{
        auto p=buffers.find(data);if(p==buffers.end())return native->Close(data);
        Buffer* b=p->second;buffers.erase(p);const HRESULT hr=native->Close(b->original);delete b;return hr;
    }
};
}
