InputManager::InputManager( HWND hWnd, HINSTANCE hInst )
    : m_pDInput( nullptr )
    , m_pDIKeyboard( nullptr )
    , m_pDIMouse( nullptr )
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
    if (m_pDIKeyboard != nullptr)
    {
        m_pDIKeyboard->Release();
        m_pDIKeyboard = nullptr;
    }

    if (m_pDIMouse != nullptr)
    {
        m_pDIMouse->Release();
        m_pDIMouse = nullptr;
    }

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

    m_pDIMouse->Acquire();

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

#if 0
    // For Debug
    char buf[256];
    sprintf_s( buf, 256, "(%d, %d, %d) %s %s %s",
        m_mouseState.lX,
        m_mouseState.lY,
        m_mouseState.lZ,
        (m_mouseState.rgbButtons[0] & 0x80) ? "Left" : "--",
        (m_mouseState.rgbButtons[1] & 0x80) ? "Right" : "--",
        (m_mouseState.rgbButtons[2] & 0x80) ? "Center" : "--" );
    Log::Output( Log::LOG_LEVEL_DEBUG, buf );
#endif
}

bool InputManager::CheckMouseButton( MOUSE_BUTTON button, ACTION_STATE state ) const
{
    bool bStatus = false;

    int index = 0;
    switch (button)
    {
    case InputManager::MOUSE_BUTTON_LEFT:
        index = 0;
        break;
    case InputManager::MOUSE_BUTTON_RIGHT:
        index = 1;
        break;
    case InputManager::MOUSE_BUTTON_CENTER:
        index = 2;
        break;
    default:
        break;
    }

    if (state & ACTION_STATE_PRESSED)
    {
        bStatus |= m_mouseState.rgbButtons[index] & 0x80 && 
            !(m_prevMouseState.rgbButtons[index] & 0x80);
    }
    else if (state & ACTION_STATE_PRESSING)
    {
        bStatus |= m_mouseState.rgbButtons[index] & 0x80 &&
            m_prevMouseState.rgbButtons[index] & 0x80;
    }
    else if (state & ACTION_STATE_RELEASED)
    {
        bStatus |= !(m_mouseState.rgbButtons[index] & 0x80) &&
            m_prevMouseState.rgbButtons[index] & 0x80;
    }
    else if (state & ACTION_STATE_NONE)
    {
        bStatus |= !(m_mouseState.rgbButtons[index] & 0x80) &&
            !(m_prevMouseState.rgbButtons[index] & 0x80);
    }

    return bStatus;
}
