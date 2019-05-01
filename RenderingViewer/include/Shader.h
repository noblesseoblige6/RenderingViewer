#pragma once

using namespace std;

class Shader
{
public:
    static bool CompileShader( const std::wstring& file, 
        shared_ptr<ID3DBlob>& pVSBlob, 
        shared_ptr<ID3DBlob>& pPSBlob );

    static bool SearchFilePath( const std::wstring& filePath, std::wstring& result );
};