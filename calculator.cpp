#include <windows.h>
#include <dwmapi.h>
#include <cmath>
#include <string>
#include <sstream>
#include <cstdlib>

#pragma comment(lib, "dwmapi.lib")

using namespace std;

// Dark mode detection
bool isDarkMode = false;
COLORREF bgColor, textColor, buttonBgColor, buttonTextColor;
HBRUSH hBgBrush = NULL;
HBRUSH hButtonBrush = NULL;

bool IsWindowsDarkMode()
{
    HKEY hKey;
    LONG result = RegOpenKeyEx(HKEY_CURRENT_USER, 
        "Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", 
        0, KEY_READ, &hKey);
    
    if (result != ERROR_SUCCESS)
        return false;

    DWORD value = 1;
    DWORD size = sizeof(value);
    result = RegQueryValueEx(hKey, "AppsUseLightTheme", NULL, NULL, 
        (LPBYTE)&value, &size);
    RegCloseKey(hKey);

    return (result == ERROR_SUCCESS && value == 0);
}

void SetColors()
{
    isDarkMode = IsWindowsDarkMode();
    
    if (hBgBrush) DeleteObject(hBgBrush);
    if (hButtonBrush) DeleteObject(hButtonBrush);
    
    if (isDarkMode)
    {
        bgColor = RGB(32, 32, 32);
        textColor = RGB(255, 255, 255);
        buttonBgColor = RGB(45, 45, 45);
        buttonTextColor = RGB(255, 255, 255);
    }
    else
    {
        bgColor = RGB(240, 240, 240);
        textColor = RGB(0, 0, 0);
        buttonBgColor = RGB(225, 225, 225);
        buttonTextColor = RGB(0, 0, 0);
    }
    
    hBgBrush = CreateSolidBrush(bgColor);
    hButtonBrush = CreateSolidBrush(buttonBgColor);
}

void ApplyDarkModeToTitleBar(HWND hwnd)
{
    // DWMWA_USE_IMMERSIVE_DARK_MODE = 20
    BOOL darkMode = isDarkMode ? TRUE : FALSE;
    DwmSetWindowAttribute(hwnd, 20, &darkMode, sizeof(darkMode));
}

string to_string_custom(double value)
{
    stringstream ss;
    ss << value;
    return ss.str();
}

double stod_custom(const string& str)
{
    return atof(str.c_str());
}

HWND hwndDisplay;
string currentInput = "";
double currentValue = 0;
string operation = "";
bool shouldResetDisplay = false;

const int BUTTON_WIDTH = 60;
const int BUTTON_HEIGHT = 40;
const int DISPLAY_HEIGHT = 60;

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void CreateButtons(HWND hwnd);
void UpdateDisplay();
void OnButtonClick(const string& value);
void PerformOperation(const string& op);
void Calculate();
void RefreshWindow(HWND hwnd);

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow)
{
    SetColors();
    
    const char CLASS_NAME[] = "CalculatorClass";

    WNDCLASS wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = CreateSolidBrush(bgColor);

    RegisterClass(&wc);

    HWND hwnd = CreateWindowEx(
        0,
        CLASS_NAME,
        "Calculator",
        WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT,
        340, 420,
        NULL,
        NULL,
        hInstance,
        NULL
    );

    if (hwnd == NULL) return 0;

    ShowWindow(hwnd, nCmdShow);

    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CREATE:
        hwndDisplay = CreateWindow(
            "STATIC",
            "0",
            WS_VISIBLE | WS_CHILD | SS_RIGHT,
            5, 10, 320, DISPLAY_HEIGHT,
            hwnd, NULL, NULL, NULL
        );
        {
            SetBkColor(GetDC(hwndDisplay), bgColor);
            HFONT hFont = CreateFont(32, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, "Arial");
            SendMessage(hwndDisplay, WM_SETFONT, (WPARAM)hFont, TRUE);
        }
        CreateButtons(hwnd);
        ApplyDarkModeToTitleBar(hwnd);
        return 0;

    case WM_SETTINGCHANGE:
    {
        SetColors();
        ApplyDarkModeToTitleBar(hwnd);
        RefreshWindow(hwnd);
        RedrawWindow(hwnd, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
        return 0;
    }

    case WM_ERASEBKGND:
    {
        HDC hdc = (HDC)wParam;
        RECT rc;
        GetClientRect(hwnd, &rc);
        FillRect(hdc, &rc, hBgBrush);
        return TRUE;
    }

    case WM_CTLCOLORBTN:
    {
        HDC hdcStatic = (HDC)wParam;
        SetTextColor(hdcStatic, buttonTextColor);
        SetBkColor(hdcStatic, buttonBgColor);
        return (LRESULT)hButtonBrush;
    }

    case WM_CTLCOLORSTATIC:
    {
        HDC hdcStatic = (HDC)wParam;
        SetTextColor(hdcStatic, textColor);
        SetBkColor(hdcStatic, bgColor);
        return (LRESULT)hBgBrush;
    }

    case WM_CTLCOLORDLG:
        return (LRESULT)hBgBrush;

    case WM_DRAWITEM:
    {
        LPDRAWITEMSTRUCT pDIS = (LPDRAWITEMSTRUCT)lParam;
        
        RECT rcButton = pDIS->rcItem;
        FillRect(pDIS->hDC, &rcButton, hButtonBrush);
        
        SetBkMode(pDIS->hDC, TRANSPARENT);
        SetTextColor(pDIS->hDC, buttonTextColor);
        
        if (pDIS->itemState & ODS_FOCUS)
        {
            DrawFocusRect(pDIS->hDC, &rcButton);
        }
        
        if (pDIS->itemState & ODS_SELECTED)
        {
            HBRUSH hSelectedBrush = CreateSolidBrush(RGB(60, 60, 60));
            FillRect(pDIS->hDC, &rcButton, hSelectedBrush);
            DeleteObject(hSelectedBrush);
        }
        
        char btnText[256] = {0};
        GetWindowText(pDIS->hwndItem, btnText, sizeof(btnText));
        
        HFONT hFont = CreateFont(14, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, "Arial");
        HFONT hOldFont = (HFONT)SelectObject(pDIS->hDC, hFont);
        
        DrawText(pDIS->hDC, btnText, -1, &rcButton, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        
        SelectObject(pDIS->hDC, hOldFont);
        DeleteObject(hFont);
        
        FrameRect(pDIS->hDC, &rcButton, (HBRUSH)GetStockObject(GRAY_BRUSH));
        
        return TRUE;
    }

    case WM_COMMAND:
    {
        int id = LOWORD(wParam);
        if (id >= 100 && id <= 109) // Number buttons 0-9
            OnButtonClick(to_string_custom(id - 100));
        else if (id == 110) OnButtonClick(".");
        else if (id == 111) PerformOperation("+");
        else if (id == 112) PerformOperation("-");
        else if (id == 113) PerformOperation("*");
        else if (id == 114) PerformOperation("/");
        else if (id == 115) Calculate();
        else if (id == 116) { currentInput = ""; currentValue = 0; operation = ""; UpdateDisplay(); }
        else if (id == 117)
        {
            if (!currentInput.empty())
                currentInput.erase(currentInput.length() - 1);
            UpdateDisplay();
        }
        else if (id == 118)
        {
            if (!currentInput.empty())
                currentValue = stod_custom(currentInput) / 100.0;
            else if (currentValue != 0)
                currentValue /= 100.0;
            currentInput = to_string_custom(currentValue);
            size_t pos = currentInput.find_last_not_of('0');
            if (pos != string::npos && currentInput[pos] != '.')
                currentInput.erase(pos + 1);
            UpdateDisplay();
        }
        else if (id == 119)
        {
            double val = 0;
            if (!currentInput.empty())
                val = stod_custom(currentInput);
            else
                val = currentValue;
            val = sqrt(val);
            currentInput = to_string_custom(val);
            currentValue = val;
            UpdateDisplay();
        }
        return 0;
    }

    case WM_KEYDOWN:
    {
        switch (wParam)
        {
        case VK_RETURN:  // Enter = equals
            Calculate();
            break;
        case VK_BACK:    // Backspace
            if (!currentInput.empty())
                currentInput.erase(currentInput.length() - 1);
            UpdateDisplay();
            break;
        case VK_DELETE:  // Delete = Clear
            currentInput = "";
            currentValue = 0;
            operation = "";
            UpdateDisplay();
            break;
        case VK_ESCAPE:  // Escape = Clear
            currentInput = "";
            currentValue = 0;
            operation = "";
            UpdateDisplay();
            break;
        case VK_OEM_PLUS:   // + (plus/equals key)
            PerformOperation("+");
            break;
        case VK_OEM_MINUS:  // - (minus key)
            PerformOperation("-");
            break;
        case VK_OEM_2:      // / (slash)
            PerformOperation("/");
            break;
        case 0x38:  // * (shift+8, but we check for VK_MULTIPLY)
            break;
        }
        return 0;
    }

    case WM_CHAR:
    {
        switch (wParam)
        {
        case '0': case '1': case '2': case '3': case '4':
        case '5': case '6': case '7': case '8': case '9':
            OnButtonClick(string(1, (char)wParam));
            break;
        case '.':
            OnButtonClick(".");
            break;
        case '+':
            PerformOperation("+");
            break;
        case '-':
            PerformOperation("-");
            break;
        case '/':
            PerformOperation("/");
            break;
        case '*':
            PerformOperation("*");
            break;
        case '=':
            Calculate();
            break;
        }
        return 0;
    }

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void CreateButtons(HWND hwnd)
{
    int startX = 10;
    int startY = 80;
    int spacing = 5;
    int btnW = 55;
    int btnH = 45;

    const char* buttons[] = {
        "7", "8", "9", "/", "Back",
        "4", "5", "6", "*", "%",
        "1", "2", "3", "-", "Sqrt",
        "0", ".", "=", "+", "C"
    };

    int ids[] = {
        107, 108, 109, 114, 117,
        104, 105, 106, 113, 118,
        101, 102, 103, 112, 119,
        100, 110, 115, 111, 116
    };

    int buttonIndex = 0;
    for (int row = 0; row < 4; row++)
    {
        for (int col = 0; col < 5; col++)
        {
            int x = startX + col * (btnW + spacing);
            int y = startY + row * (btnH + spacing);

            HWND btnHandle = CreateWindow(
                "BUTTON",
                buttons[buttonIndex],
                WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
                x, y, btnW, btnH,
                hwnd, (HMENU)(long)ids[buttonIndex], NULL, NULL
            );
            
            HFONT hFont = CreateFont(14, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, "Arial");
            SendMessage(btnHandle, WM_SETFONT, (WPARAM)hFont, TRUE);
            
            buttonIndex++;
        }
    }
}

void UpdateDisplay()
{
    if (hwndDisplay)
    {
        string display = currentInput.empty() ? to_string_custom((int)currentValue) : currentInput;
        SetWindowText(hwndDisplay, display.c_str());
    }
}

void OnButtonClick(const string& value)
{
    if (shouldResetDisplay)
    {
        currentInput = "";
        shouldResetDisplay = false;
    }

    if (value == ".")
    {
        if (currentInput.find(".") == string::npos)
            currentInput += value;
    }
    else
    {
        currentInput += value;
    }
    UpdateDisplay();
}

void PerformOperation(const string& op)
{
    if (!currentInput.empty())
    {
        currentValue = stod_custom(currentInput);
        currentInput = "";
    }
    operation = op;
    shouldResetDisplay = true;
}

void RefreshWindow(HWND hwnd)
{
    HWND child = GetWindow(hwnd, GW_CHILD);
    while (child != NULL)
    {
        InvalidateRect(child, NULL, TRUE);
        UpdateWindow(child);
        child = GetWindow(child, GW_HWNDNEXT);
    }
    InvalidateRect(hwnd, NULL, TRUE);
    UpdateWindow(hwnd);
}

void Calculate()
{
    if (currentInput.empty() || operation.empty())
        return;

    double secondValue = stod_custom(currentInput);
    double result = 0;

    if (operation == "+") result = currentValue + secondValue;
    else if (operation == "-") result = currentValue - secondValue;
    else if (operation == "*") result = currentValue * secondValue;
    else if (operation == "/")
    {
        if (secondValue != 0)
            result = currentValue / secondValue;
        else
        {
            SetWindowText(hwndDisplay, "Error");
            currentInput = "";
            return;
        }
    }

    currentValue = result;
    currentInput = to_string_custom(result);
    operation = "";
    shouldResetDisplay = true;
    UpdateDisplay();
}
