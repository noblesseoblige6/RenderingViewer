bool Shader::CompileShader( const std::wstring& file, ComPtr<ID3DBlob>& pVSBlob, ComPtr<ID3DBlob>& pPSBlob )
{
    // 頂点シェーダのファイルパスを検索.
    std::wstring path;
    if (!SearchFilePath( file.c_str(), path ))
    {
        Log::Output( Log::LOG_LEVEL_ERROR, "File Not Found." );
        return false;
    }

#if defined(_DEBUG)
    // Enable better shader debugging with the graphics debugging tools.
    UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
    UINT compileFlags = 0;
#endif

    HRESULT hr = S_OK;

    ComPtr<ID3DBlob> pBlob;
    hr = D3DCompileFromFile( path.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "VSMain", "vs_5_0", compileFlags, 0, pVSBlob.ReleaseAndGetAddressOf(), pBlob.ReleaseAndGetAddressOf() );
    if (FAILED( hr ))
    {
        Log::Output( Log::LOG_LEVEL_ERROR, "D3DCompileFromFile() Failed." );
        return false;
    }

    hr = D3DCompileFromFile( path.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "PSMain", "ps_5_0", compileFlags, 0, pPSBlob.ReleaseAndGetAddressOf(), nullptr );
    if (FAILED( hr ))
    {
        Log::Output( Log::LOG_LEVEL_ERROR, "D3DCompileFromFile Failed." );
        return false;
    }

    return true;
}

bool Shader::SearchFilePath( const std::wstring& filePath, std::wstring& result )
{
    if (filePath.length() == 0 || filePath == L" ")
    {
        return false;
    }

    wchar_t exePath[520] = { 0 };
    GetModuleFileNameW( nullptr, exePath, 520 );
    exePath[519] = 0; // null終端化.
    PathRemoveFileSpecW( exePath );

    wchar_t dstPath[520] = { 0 };

    swprintf_s( dstPath, L"%s", filePath.c_str() );
    if (PathFileExistsW( dstPath ) == TRUE)
    {
        result = dstPath;
        return true;
    }

    swprintf_s( dstPath, L".\\shader\\%s", filePath.c_str() );
    if (PathFileExistsW( dstPath ) == TRUE)
    {
        result = dstPath;
        return true;
    }

    swprintf_s( dstPath, L"..\\%s", filePath.c_str() );
    if (PathFileExistsW( dstPath ) == TRUE)
    {
        result = dstPath;
        return true;
    }

    return false;
}
