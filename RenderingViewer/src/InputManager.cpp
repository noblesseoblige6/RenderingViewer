InputManager::InputManager( HWND hWnd, HINSTANCE hInst )
{
    InitDirectInput( hInst );

    InitKeyboard( hWnd, hInst );
    InitMouse( hWnd, hInst );
}


InputManager::~InputManager()
{
    Release();
}

void InputManager::Release()
{
    m_pDIKeyboard->Unacquire();

    m_pDInput->Release();
}

bool InputManager::InitDirectInput( HINSTANCE hInst )
{
    HRESULT hr = DirectInput8Create( hInst, DIRECTINPUT_VERSION,
        IID_IDirectInput8, (void**)&m_pDInput, NULL );

    if FAILED( hr )
    {
        Log::Output( Log::LOG_LEVEL_ERROR, "InputManager::InitDirectInput() Failed." );

        return false;
    }

    return true;
}

bool InputManager::InitKeyboard( HWND hWnd, HINSTANCE hInst )
{
    HRESULT hr = m_pDInput->CreateDevice( GUID_SysKeyboard, &m_pDIKeyboard, NULL );
    if FAILED( hr )
    {
        Log::Output( Log::LOG_LEVEL_ERROR, "InputManager::InitKeyboard() Failed." );
        return false;
    }

    hr = m_pDIKeyboard->SetDataFormat( &c_dfDIKeyboard );
    if FAILED( hr )
    {
        Log::Output( Log::LOG_LEVEL_ERROR, "InputManager::InitKeyboard() Failed." );
        return FALSE;
    }

    hr = m_pDIKeyboard->SetCooperativeLevel( hWnd,
        DISCL_FOREGROUND | DISCL_NONEXCLUSIVE | DISCL_NOWINKEY );
    if FAILED( hr )
    {
        Log::Output( Log::LOG_LEVEL_ERROR, "InputManager::InitKeyboard() Failed." );
        return FALSE;
    }

    m_pDIKeyboard->Acquire();

    return true;
}

bool InputManager::InitMouse( HWND hWnd, HINSTANCE hInst )
{
    HRESULT hr = m_pDInput->CreateDevice( GUID_SysMouse, &m_pDIMouse, NULL );
    if FAILED( hr )
    {
        Log::Output( Log::LOG_LEVEL_ERROR, "InputManager::InitMouse() Failed." );
        return false;
    }

    hr = m_pDIMouse->SetDataFormat( &c_dfDIMouse );
    if FAILED( hr )
    {
        Log::Output( Log::LOG_LEVEL_ERROR, "InputManager::InitMouse() Failed." );
        return false;
    }

    hr = m_pDIMouse->SetCooperativeLevel( hWnd,
        DISCL_FOREGROUND | DISCL_NONEXCLUSIVE );
    if FAILED( hr )
    {
        Log::Output( Log::LOG_LEVEL_ERROR, "InputManager::InitMouse() Failed." );
        return false;
    }

    // デバイスの設定
    DIPROPDWORD diprop;
    diprop.diph.dwSize = sizeof( diprop );
    diprop.diph.dwHeaderSize = sizeof( diprop.diph );
    diprop.diph.dwObj = 0;
    diprop.diph.dwHow = DIPH_DEVICE;
    diprop.dwData = DIPROPAXISMODE_REL;

    hr = m_pDIMouse->SetProperty( DIPROP_AXISMODE, &diprop.diph );
    if (FAILED( hr ))
    {
        Log::Output( Log::LOG_LEVEL_ERROR, "InputManager::InitMouse() Failed." );
        return false;
    }

    hr = m_pDIMouse->Acquire();

    return true;
}

void InputManager::ProcessKeyboard()
{
    memcpy( m_prevKeyboardBuffer, m_keyboardBuffer, sizeof( m_keyboardBuffer ) );

    ZeroMemory( m_keyboardBuffer, sizeof( m_keyboardBuffer ) );

    HRESULT hr = m_pDIKeyboard->GetDeviceState( sizeof( m_keyboardBuffer ), m_keyboardBuffer );
    if FAILED( hr )
    {
        hr = m_pDIKeyboard->Acquire();
        return;
    }
}

void InputManager::ProcessMouse()
{
    memcpy( &m_prevMouseState, &m_mouseState, sizeof( DIMOUSESTATE ) );

    HRESULT hr = m_pDIMouse->GetDeviceState( sizeof( DIMOUSESTATE ), &m_mouseState );
    if FAILED( hr )
    {
        hr = m_pDIMouse->Acquire();
        return;
    }
}
