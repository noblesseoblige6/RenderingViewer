#pragma once

class InputManager
{
public:
    enum ACTION_STATE
    {
        ACTION_STATE_NONE = 0,
        ACTION_STATE_PRESSED,
        ACTION_STATE_PRESSING,
        ACTION_STATE_RELEASED,
    };

    enum MOUSE_BUTTON
    {
        MOUSE_BUTTON_LEFT = 0,
        MOUSE_BUTTON_RIGHT,
        MOUSE_BUTTON_CENTER
    };

public:
    InputManager( HWND hWnd, HINSTANCE hInst );
    ~InputManager();

    void Release();

    void ProcessKeyboard();
    void ProcessMouse();

    const DIMOUSESTATE& GetMouseState() const { return m_mouseState; }
    const DIMOUSESTATE& GetPrevMouseState() const { return m_prevMouseState; }

    bool CheckMouseButton( MOUSE_BUTTON button, ACTION_STATE state ) const;
    bool IsPressed( MOUSE_BUTTON button ) const { return CheckMouseButton( button, ACTION_STATE_PRESSED ); };
    bool IsPressing( MOUSE_BUTTON button ) const { return CheckMouseButton(button, ACTION_STATE_PRESSING ); };
    bool IsReleased( MOUSE_BUTTON button ) const { return CheckMouseButton( button, ACTION_STATE_RELEASED ); };

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

