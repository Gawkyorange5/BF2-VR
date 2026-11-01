#include "Refractor.h"
#include "ShaderHook.h"
#include "TerrainPrecision.h"
#include "VrSettings.h"

D3DXCreateEffect_t pOrigD3DXCreateEffect = D3DXCreateEffect;

HRESULT WINAPI MyD3DXCreateEffect(LPDIRECT3DDEVICE9 pDevice, LPCVOID pSrcData, UINT SrcDataLen, CONST D3DXMACRO* pDefines, LPD3DXINCLUDE pInclude, DWORD Flags, LPD3DXEFFECTPOOL pPool, LPD3DXEFFECT* ppEffect, LPD3DXBUFFER* ppCompilationErrors)
{
	auto str = reinterpret_cast<const char*>(pSrcData);

	const char* begin = nullptr;
	const char* end = nullptr;

	const char* limit = str + SrcDataLen;
	while (str < limit && *str && *str != '\n')
	{
		if (*str == '"')
		{
			if (begin)
			{
				end = str;
				break;
			}
			else
			{
				begin = ++str;
			}
		}
		++str;
	}
	std::string name;
	if (begin && end)
	{
		while (begin != end)
		{
			name.push_back(*begin);
			++begin;
		}
	}

	HRESULT retVal;
	char disabled[8]{};
	const bool enabled=!(GetEnvironmentVariableA("BF2VR_TERRAIN_PRECISION",disabled,sizeof(disabled))==1&&disabled[0]=='0');
	if (name=="TerrainShader.fx"&&pInclude&&ppEffect&&enabled) {
		TerrainPrecision::Includes includes(pInclude);
		ID3DXEffect* candidate=nullptr;ID3DXBuffer* errors=nullptr;
		retVal=pOrigD3DXCreateEffect(pDevice,pSrcData,SrcDataLen,pDefines,&includes,Flags,pPool,&candidate,ppCompilationErrors?&errors:nullptr);
		CleanVR::Log("TERRAIN148 candidate source=TerrainShader.fx correctedFiles=%u unknownFiles=%u result=%08X",includes.applied,includes.unknown,static_cast<unsigned>(retVal));
		if (FAILED(retVal)&&includes.applied) {
			if(candidate)candidate->Release();
			if(errors)errors->Release();
			retVal=pOrigD3DXCreateEffect(pDevice,pSrcData,SrcDataLen,pDefines,pInclude,Flags,pPool,ppEffect,ppCompilationErrors);
			CleanVR::Log("TERRAIN148 original shader fallback result=%08X",static_cast<unsigned>(retVal));
		} else {
			*ppEffect=candidate;if(ppCompilationErrors)*ppCompilationErrors=errors;
		}
	} else {
		retVal=pOrigD3DXCreateEffect(pDevice,pSrcData,SrcDataLen,pDefines,pInclude,Flags,pPool,ppEffect,ppCompilationErrors);
		if(name=="TerrainShader.fx")CleanVR::Log("TERRAIN148 original shader enabled=%d includeAvailable=%d result=%08X",enabled,pInclude!=nullptr,static_cast<unsigned>(retVal));
	}
	if (name != "" && SUCCEEDED(retVal) && ppEffect && *ppEffect)
	{
		DICE::detail::gShaders[name] = *ppEffect;
	}
	/*std::fstream file;
	file.open("RFX\\shaders.txt", std::fstream::out | std::fstream::app);
	file << name << ": " << *ppEffect << " Defines: {";
	while (pDefines && pDefines->Name)
	{
		file << pDefines->Name << '=' << pDefines->Definition << ';';
		++pDefines;
	}
	file << "}\n";
	file << str;
	file.close();*/
	return retVal;
}

namespace DICE
{
	namespace detail
	{
		::std::map<::std::string, ID3DXEffect*> gShaders;
	}
	namespace ShaderManager
	{
		ID3DXEffect* getShaderByName(const ::std::string& name)
		{
			auto it = detail::gShaders.find(name);
			if (it != detail::gShaders.end())
			{
				return it->second;
			}
			return nullptr;
		}

		void init()
		{
			createConsoleObject(&setFloatBySemantic, "shadermanagersetFloatBySemantic");
			createConsoleObject(&setFloat2BySemantic, "shadermanagersetFloat2BySemantic");
			createConsoleObject(&setFloat3BySemantic, "shadermanagersetFloat3BySemantic");
			createConsoleObject(&setFloat4BySemantic, "shadermanagersetFloat4BySemantic");

			// TODO: stream operator from string to IDirect3DTexture9* 
			//createConsoleObject(&setShaderTextureBySemantic, "shadermanagersetShaderTextureBySemantic");
		}

		::std::string setFloatBySemantic(const ::std::string& shaderName, const ::std::string& semantic, float val)
		{
			auto pShader = getShaderByName(shaderName);
			if (!pShader) return "Error: Shader not found.";
			auto hParameter = pShader->GetParameterBySemantic(NULL, semantic.c_str());
			if (!hParameter) return "Error: Parameter not found.";
			auto hr = pShader->SetFloat(hParameter, val);
			if (hr != S_OK) return "Error: Failed to set parameter value.";
			return "1";
		}

		::std::string setFloat2BySemantic(const ::std::string& shaderName, const ::std::string& semantic, const D3DXVECTOR2& val)
		{
			auto pShader = getShaderByName(shaderName);
			if (!pShader) return "Error: Shader not found.";
			auto hParameter = pShader->GetParameterBySemantic(NULL, semantic.c_str());
			if (!hParameter) return "Error: Parameter not found.";
			auto hr = pShader->SetFloatArray(hParameter, reinterpret_cast<const float*>(&val), 2);
			if (hr != S_OK) return "Error: Failed to set parameter value.";
			return "1";
		}

		::std::string setFloat3BySemantic(const ::std::string& shaderName, const ::std::string& semantic, const D3DXVECTOR3& val)
		{
			auto pShader = getShaderByName(shaderName);
			if (!pShader) return "Error: Shader not found.";
			auto hParameter = pShader->GetParameterBySemantic(NULL, semantic.c_str());
			if (!hParameter) return "Error: Parameter not found.";
			auto hr = pShader->SetFloatArray(hParameter, reinterpret_cast<const float*>(&val), 3);
			if (hr != S_OK) return "Error: Failed to set parameter value.";
			return "1";
		}

		::std::string setFloat4BySemantic(const ::std::string& shaderName, const ::std::string& semantic, const D3DXVECTOR4& val)
		{
			auto pShader = getShaderByName(shaderName);
			if (!pShader) return "Error: Shader not found.";
			auto hParameter = pShader->GetParameterBySemantic(NULL, semantic.c_str());
			if (!hParameter) return "Error: Parameter not found.";
			auto hr = pShader->SetFloatArray(hParameter, reinterpret_cast<const float*>(&val), 4);
			if (hr != S_OK) return "Error: Failed to set parameter value.";
			return "1";
		}

		::std::string setMatrixBySemantic(const ::std::string& shaderName, const ::std::string& semantic, const D3DXMATRIX& val)
		{
			auto pShader = getShaderByName(shaderName);
			if (!pShader) return "Error: Shader not found.";
			auto hParameter = pShader->GetParameterBySemantic(NULL, semantic.c_str());
			if (!hParameter) return "Error: Parameter not found.";
			auto hr = pShader->SetMatrix(hParameter, &val);
			if (hr != S_OK) return "Error: Failed to set parameter value.";
			return "1";
		}

		::std::string setTextureBySemantic(const ::std::string& shaderName, const ::std::string& semantic, IDirect3DTexture9* val)
		{
			auto pShader = getShaderByName(shaderName);
			if (!pShader) return "Error: Shader not found.";
			auto hParameter = pShader->GetParameterBySemantic(NULL, semantic.c_str());
			if (!hParameter) return "Error: Parameter not found.";
			auto hr = pShader->SetTexture(hParameter, val);
			if (hr != S_OK) return "Error: Failed to set parameter value.";
			return "1";
		}
	}
}
