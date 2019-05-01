#pragma once

using namespace std;
using namespace acLib::DX12;
class Shader
{
public:
    static bool CompileShader( const std::wstring& file, ComPtr<ID3DBlob>& pVSBlob, ComPtr<ID3DBlob>& pPSBlob );

    static bool SearchFilePath( const std::wstring& filePath, std::wstring& result );
};