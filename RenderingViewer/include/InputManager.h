#pragma once

class InputManager
{
public:
    InputManager( HWND hWnd, HINSTANCE hInst );
    ~InputManager();

    void Release();

    void ProcessKeyboard();
    void ProcessMouse();

protected:
    bool InitDirectInput( HINSTANCE hInst );

    bool InitKeyboard( HWND hWnd, HINSTANCE hInst );
    bool InitMouse( HWND hWnd, HINSTANCE hInst );

private:
    LPDIRECTINPUT8  m_pDInput;

    LPDIRECTINPUTDEVICE8  m_pDIKeyboard;
    BYTE m_prevKeyboardBuffer[256];
    BYTE m_keyboardBuffer[256];
    
    LPDIRECTINPUTDEVICE8  m_pDIMouse;
    DIMOUSESTATE m_prevMouseState;
    DIMOUSESTATE m_mouseState;
};

