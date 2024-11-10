/*
    Tunnel Vision - tunview.cpp
    Copyright (C) 2005 Cockos Incorporated

    Tunnel Vision is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    Tunnel Vision is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Tunnel Vision; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

// Added functionality for adjustable opacity using a slider control.

#include <windows.h>
#include <multimon.h>
#include "resource.h"
#undef GetSystemMetrics

char g_configfile[1024];
#define VERSION "0.03"
#define SECNAME "tunview"

#define ABOUT_INFO "Tunnel Vision v" VERSION "\r\nCopyright (C) 2005, Cockos Incorporated."

HINSTANCE g_hInstance;
#define WM_SYSTRAY              WM_USER + 0x200
BOOL systray_add(HWND hwnd, UINT uID, HICON hIcon, LPSTR lpszTip);
BOOL systray_del(HWND hwnd, UINT uID);

int g_enable = 0;
int g_rsize = 10; // radius in percent of screen
int g_rshape = 2;
int g_bgcolor = 0;
int g_use_img = 0;
int g_img_mode = 2;
char g_img_path[2048];

// Added variables for opacity
int g_opacity = 128; // Default opacity (0-255)

HWND g_hwnd;
int m_last_x = -1, m_last_y = -1;

void DoSelectColor(HWND hwnd, int* a)
{
    static COLORREF custcolors[16];
    CHOOSECOLOR cs;
    cs.lStructSize = sizeof(cs);
    cs.hwndOwner = hwnd;
    cs.hInstance = 0;
    cs.rgbResult = *a;
    cs.lpCustColors = custcolors;
    cs.Flags = CC_RGBINIT | CC_FULLOPEN;
    if (ChooseColor(&cs))
    {
        *a = cs.rgbResult;
    }
}

void DoDrawColoredButton(DRAWITEMSTRUCT* di, COLORREF color)
{
    char wt[123];
    HPEN hPen, hOldPen;
    HBRUSH hBrush, hOldBrush;
    hPen = (HPEN)GetStockObject(BLACK_PEN);
    LOGBRUSH lb = { BS_SOLID, color, 0 };
    hBrush = CreateBrushIndirect(&lb);
    hOldPen = (HPEN)SelectObject(di->hDC, hPen);
    hOldBrush = (HBRUSH)SelectObject(di->hDC, hBrush);

    Rectangle(di->hDC, di->rcItem.left, di->rcItem.top, di->rcItem.right, di->rcItem.bottom);

    GetWindowText(di->hwndItem, wt, sizeof(wt));
    SetBkColor(di->hDC, color);
    SetTextColor(di->hDC, ~color & 0xffffff);
    DrawText(di->hDC, wt, -1, &di->rcItem, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    DeleteObject(hBrush);
    SelectObject(di->hDC, hOldPen);
    SelectObject(di->hDC, hOldBrush);
}

HBITMAP g_bg_bitmap;
HWND config_window;

BOOL WINAPI ConfigProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    static HWND hOpacitySlider; // Added for opacity slider
    switch (uMsg)
    {
    case WM_INITDIALOG:
        config_window = hwndDlg;
        SetDlgItemInt(hwndDlg, IDC_SIZEPERCENT, g_rsize, FALSE);
        SetDlgItemText(hwndDlg, IDC_INFO, ABOUT_INFO);
        if (g_rshape == 0) CheckDlgButton(hwndDlg, IDC_RADIO1, BST_CHECKED);
        else if (g_rshape == 1) CheckDlgButton(hwndDlg, IDC_RADIO2, BST_CHECKED);
        else if (g_rshape == 2) CheckDlgButton(hwndDlg, IDC_RADIO3, BST_CHECKED);
        if (g_use_img)
            CheckDlgButton(hwndDlg, IDC_CHECK1, BST_CHECKED);

        if (g_img_mode == 0)
            CheckDlgButton(hwndDlg, IDC_RADIO5, BST_CHECKED);
        else if (g_img_mode == 1)
            CheckDlgButton(hwndDlg, IDC_RADIO6, BST_CHECKED);
        else if (g_img_mode == 2)
            CheckDlgButton(hwndDlg, IDC_RADIO7, BST_CHECKED);
        else
            CheckDlgButton(hwndDlg, IDC_RADIO8, BST_CHECKED);

        SetDlgItemText(hwndDlg, IDC_EDIT2, g_img_path);

        // Initialize opacity slider
        hOpacitySlider = GetDlgItem(hwndDlg, IDC_OPACITY_SLIDER);
        SendMessage(hOpacitySlider, TBM_SETRANGE, TRUE, MAKELONG(0, 255));
        SendMessage(hOpacitySlider, TBM_SETPOS, TRUE, g_opacity);
        SetDlgItemInt(hwndDlg, IDC_OPACITY_LABEL, g_opacity, FALSE);

        return 0;
    case WM_DESTROY:
        config_window = 0;
        return 0;
    case WM_HSCROLL:
        if ((HWND)lParam == hOpacitySlider)
        {
            g_opacity = SendMessage(hOpacitySlider, TBM_GETPOS, 0, 0);
            SetDlgItemInt(hwndDlg, IDC_OPACITY_LABEL, g_opacity, FALSE);
            // Save opacity to config
            char buf[64];
            wsprintf(buf, "%d", g_opacity);
            WritePrivateProfileString(SECNAME, "opacity", buf, g_configfile);
            InvalidateRect(g_hwnd, NULL, TRUE);
        }
        return 0;
    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDOK:
        case IDCANCEL:
            // Save config
            EndDialog(hwndDlg, 0);
            return 0;
        case IDC_CHECK1:
            g_use_img = !!IsDlgButtonChecked(hwndDlg, IDC_CHECK1);
            WritePrivateProfileString(SECNAME, "use_img", g_use_img ? "1" : "0", g_configfile);
            if (!g_use_img)
            {
                if (g_bg_bitmap) DeleteObject(g_bg_bitmap);
                g_bg_bitmap = 0;
            }
            InvalidateRect(g_hwnd, NULL, TRUE);
            return 0;
        case IDC_BUTTON1:
        {
            char temp[2048];
            char buf1[2048];
            OPENFILENAME l = { sizeof(l),0 };
            strcpy(buf1, g_img_path);
            temp[0] = 0;
            l.lpstrInitialDir = buf1;
            l.hwndOwner = hwndDlg;
            l.lpstrFilter = "Bitmap files\0*.BMP\0All files\0*.*\0";
            l.lpstrFile = temp;
            l.nMaxFile = 2048 - 1;
            l.lpstrTitle = "Select bitmap";
            l.lpstrDefExt = "BMP";
            l.Flags = OFN_HIDEREADONLY | OFN_EXPLORER;
            if (GetOpenFileName(&l))
            {
                lstrcpyn(g_img_path, temp, sizeof(g_img_path));
                SetDlgItemText(hwndDlg, IDC_EDIT2, g_img_path);
                PostMessage(hwndDlg, WM_COMMAND, MAKEWPARAM(IDC_EDIT2, EN_CHANGE), 0);
            }
        }
        return 0;
        case IDC_EDIT2:
            if (HIWORD(wParam) == EN_CHANGE)
            {
                GetDlgItemText(hwndDlg, IDC_EDIT2, g_img_path, sizeof(g_img_path));
                WritePrivateProfileString(SECNAME, "img_path", g_img_path, g_configfile);

                if (g_bg_bitmap) DeleteObject(g_bg_bitmap);
                g_bg_bitmap = 0;

                if (g_use_img)
                    InvalidateRect(g_hwnd, NULL, TRUE);
            }
            return 0;
        case IDC_RADIO5:
        case IDC_RADIO6:
        case IDC_RADIO7:
        case IDC_RADIO8:
        {
            if (IsDlgButtonChecked(hwndDlg, IDC_RADIO5)) g_img_mode = 0;
            else if (IsDlgButtonChecked(hwndDlg, IDC_RADIO6)) g_img_mode = 1;
            else if (IsDlgButtonChecked(hwndDlg, IDC_RADIO7)) g_img_mode = 2;
            else g_img_mode = 3;

            char buf[64];
            wsprintf(buf, "%d", g_img_mode);
            WritePrivateProfileString(SECNAME, "img_mode", buf, g_configfile);

            InvalidateRect(g_hwnd, NULL, TRUE);
        }
        return 0;
        case IDC_RADIO1:
        case IDC_RADIO2:
        case IDC_RADIO3:
        {
            if (IsDlgButtonChecked(hwndDlg, IDC_RADIO1)) g_rshape = 0;
            else if (IsDlgButtonChecked(hwndDlg, IDC_RADIO2)) g_rshape = 1;
            else g_rshape = 2;

            char buf[64];
            wsprintf(buf, "%d", g_rshape);
            WritePrivateProfileString(SECNAME, "rshape", buf, g_configfile);
            m_last_x = -1;
            m_last_y = -1;
        }
        return 0;
        case IDC_SIZEPERCENT:
            if (HIWORD(wParam) == EN_CHANGE)
            {
                BOOL t;
                int a = GetDlgItemInt(hwndDlg, IDC_SIZEPERCENT, &t, FALSE);
                if (t && a > 0 && a < 100)
                {
                    g_rsize = a;
                    char buf[64];
                    wsprintf(buf, "%d", g_rsize);
                    WritePrivateProfileString(SECNAME, "rsize", buf, g_configfile);
                    m_last_x = -1;
                    m_last_y = -1;
                }
            }
            return 0;
        case IDC_COLOR:
            DoSelectColor(hwndDlg, &g_bgcolor);
            InvalidateRect(GetDlgItem(hwndDlg, LOWORD(wParam)), NULL, FALSE);
            InvalidateRect(g_hwnd, NULL, TRUE);
            {
                char buf[64];
                wsprintf(buf, "%d", g_bgcolor);
                WritePrivateProfileString(SECNAME, "bgcolor", buf, g_configfile);
            }
            return 0;
        }
        return 0;
    case WM_DRAWITEM:
    {
        DRAWITEMSTRUCT* di = (DRAWITEMSTRUCT*)lParam;
        switch (di->CtlID)
        {
        case IDC_COLOR:
            DoDrawColoredButton(di, g_bgcolor);
            break;
        }
    }
    return 0;
    }
    return 0;
}

void show_config(HWND hwnd)
{
    if (config_window && IsWindow(config_window))
    {
        SetForegroundWindow(config_window);
        return;
    }
    DialogBox(g_hInstance, MAKEINTRESOURCE(IDD_DIALOG1), hwnd, ConfigProc);
    config_window = 0;
}

static BOOL CALLBACK monitorEnumProc(
    HMONITOR hMonitor,  // handle to display monitor
    HDC hdcMonitor,     // handle to monitor-appropriate device context
    LPRECT lprcMonitor, // pointer to monitor intersection rectangle
    LPARAM dwData       // data passed from EnumDisplayMonitors
)
{
    RECT* r = (RECT*)dwData;
    if (lprcMonitor->left < r->left) r->left = lprcMonitor->left;
    if (lprcMonitor->top < r->top) r->top = lprcMonitor->top;
    if (lprcMonitor->right > r->right) r->right = lprcMonitor->right;
    if (lprcMonitor->bottom > r->bottom) r->bottom = lprcMonitor->bottom;
    return TRUE;
}

static LRESULT CALLBACK m_WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    static int m_last_bg_brush_color = -1;
    static HBRUSH m_last_bg_brush;
    switch (uMsg)
    {
    case WM_CREATE:
        g_hwnd = hwnd;
        SetWindowLong(hwnd, GWL_STYLE, GetWindowLong(hwnd, GWL_STYLE) & ~(WS_CAPTION));

        GetPrivateProfileString(SECNAME, "img_path", "", g_img_path, sizeof(g_img_path), g_configfile);
        g_use_img = GetPrivateProfileInt(SECNAME, "use_img", g_use_img, g_configfile);

        g_bgcolor = GetPrivateProfileInt(SECNAME, "bgcolor", g_bgcolor, g_configfile);
        g_rsize = GetPrivateProfileInt(SECNAME, "rsize", g_rsize, g_configfile);
        g_rshape = GetPrivateProfileInt(SECNAME, "rshape", g_rshape, g_configfile);
        g_img_mode = GetPrivateProfileInt(SECNAME, "img_mode", g_img_mode, g_configfile);
        g_opacity = GetPrivateProfileInt(SECNAME, "opacity", g_opacity, g_configfile); // Load opacity

        if (g_opacity < 0) g_opacity = 0;
        else if (g_opacity > 255) g_opacity = 255;

        if (g_rsize < 1) g_rsize = 1;
        else if (g_rsize > 99) g_rsize = 99;

        if ((g_enable = GetPrivateProfileInt(SECNAME, "enabled", 1, g_configfile)))
            ShowWindow(hwnd, SW_SHOW);

        systray_add(hwnd, 0, (HICON)GetClassLong(hwnd, GCL_HICON), "Cockos Tunnel Vision");
        SetTimer(hwnd, 1, 10, NULL);
    case WM_DISPLAYCHANGE:

    {
        RECT r = { 0,0,GetSystemMetrics(SM_CXSCREEN),GetSystemMetrics(SM_CYSCREEN) };

        HINSTANCE h = LoadLibrary("user32.dll");
        if (h)
        {
            BOOL(WINAPI* Edm)(HDC hdc, LPCRECT lprcClip, MONITORENUMPROC lpfnEnum, LPARAM dwData) = (BOOL(WINAPI*)(HDC, LPCRECT, MONITORENUMPROC, LPARAM))GetProcAddress(h, "EnumDisplayMonitors");
            if (Edm)  Edm(NULL, NULL, monitorEnumProc, (LPARAM)&r);
            FreeLibrary(h);
        }
        if (!config_window) SetWindowPos(hwnd, HWND_TOPMOST, r.left, r.top, r.right - r.left, r.bottom - r.top, 0);
    }

    return 0;
    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case ID_SYSTRAYPOPUP_QUITTUNNELVISION:
            DestroyWindow(hwnd);
            return 0;
        case ID_SYSTRAYPOPUP_TUNNELVISIONENABLED:
            g_enable = !g_enable;
            if (g_enable) ShowWindow(hwnd, SW_SHOWNA);
            else ShowWindow(hwnd, SW_HIDE);
            WritePrivateProfileString(SECNAME, "enabled", g_enable ? "1" : "0", g_configfile);
            return 0;
        case ID_SYSTRAYPOPUP_CONFIGURETUNNELVISION:
            show_config(hwnd);
            return 0;
        }
        return 0;
    case WM_ENDSESSION:
        PostQuitMessage(0);
        return 0;
    case WM_DESTROY:
        systray_del(hwnd, 0);
        if (m_last_bg_brush) DeleteObject(m_last_bg_brush);
        m_last_bg_brush = 0;
        PostQuitMessage(0);
        if (g_bg_bitmap) DeleteObject(g_bg_bitmap);
        g_bg_bitmap = 0;
        return 0;
    case WM_SYSTRAY:
        switch (LOWORD(lParam))
        {
        case WM_LBUTTONDBLCLK:
            // do configuration
            show_config(hwnd);
            return 0;
        case WM_RBUTTONUP:
        {
            SetForegroundWindow(hwnd);
            POINT pt;
            GetCursorPos(&pt);
            HMENU hm = LoadMenu(g_hInstance, MAKEINTRESOURCE(IDR_MENU1));
            HMENU popup = GetSubMenu(hm, 0);

            CheckMenuItem(popup, ID_SYSTRAYPOPUP_TUNNELVISIONENABLED, MF_BYCOMMAND | (g_enable ? MF_CHECKED : 0));
            TrackPopupMenuEx(popup, TPM_LEFTALIGN | TPM_LEFTBUTTON, pt.x, pt.y, hwnd, NULL);

            DestroyMenu(hm);
        }

        return 0;
        }
        return 0;
    case WM_TIMER:
        if (wParam == 1)
        {
            POINT l;
            GetCursorPos(&l);
            // check for movements
            if (g_enable && (l.x != m_last_x || l.y != m_last_y))
            {
                m_last_x = l.x;
                m_last_y = l.y;
                // adjust window region now
                RECT r;
                GetWindowRect(hwnd, &r);
                int width = r.right - r.left;
                int height = r.bottom - r.top;
                l.x -= r.left;
                l.y -= r.top;

                // Create a 32-bit DIB section for per-pixel alpha
                BITMAPV4HEADER bi = { 0 };
                bi.bV4Size = sizeof(BITMAPV4HEADER);
                bi.bV4Width = width;
                bi.bV4Height = -height; // Top-down DIB
                bi.bV4Planes = 1;
                bi.bV4BitCount = 32;
                bi.bV4V4Compression = BI_BITFIELDS;
                bi.bV4RedMask = 0x00FF0000;
                bi.bV4GreenMask = 0x0000FF00;
                bi.bV4BlueMask = 0x000000FF;
                bi.bV4AlphaMask = 0xFF000000;

                void* pvBits = NULL;
                HDC hdcScreen = GetDC(NULL);
                HDC hdcMem = CreateCompatibleDC(hdcScreen);
                HBITMAP hBitmap = CreateDIBSection(hdcMem, (BITMAPINFO*)&bi, DIB_RGB_COLORS, &pvBits, NULL, 0);
                HGDIOBJ hOldBitmap = SelectObject(hdcMem, hBitmap);

                // Fill the background with the desired opacity
                for (int y = 0; y < height; y++)
                {
                    DWORD* pixel = (DWORD*)((BYTE*)pvBits + y * width * 4);
                    for (int x = 0; x < width; x++)
                    {
                        // Calculate distance from the cursor
                        int dx = x - l.x;
                        int dy = y - l.y;
                        BOOL inside = FALSE;
                        if (g_rshape == 0) // Proportional rectangle
                        {
                            int xdim = (width * g_rsize) / 200;
                            int ydim = (height * g_rsize) / 200;
                            inside = (abs(dx) <= xdim && abs(dy) <= ydim);
                        }
                        else if (g_rshape == 1) // Square
                        {
                            int wdim = (min(width, height) * g_rsize) / 200;
                            inside = (abs(dx) <= wdim && abs(dy) <= wdim);
                        }
                        else // Circle
                        {
                            int wdim = (min(width, height) * g_rsize) / 200;
                            inside = (dx * dx + dy * dy <= wdim * wdim);
                        }

                        if (inside)
                        {
                            // Fully transparent
                            pixel[x] = 0x00000000;
                        }
                        else
                        {
                            // Background color with opacity
                            pixel[x] = (g_opacity << 24) | (g_bgcolor & 0x00FFFFFF);
                        }
                    }
                }

                POINT ptDst = { r.left, r.top };
                SIZE sizeWnd = { width, height };
                POINT ptSrc = { 0, 0 };
                BLENDFUNCTION blend = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };

                UpdateLayeredWindow(hwnd, hdcScreen, &ptDst, &sizeWnd, hdcMem, &ptSrc, 0, &blend, ULW_ALPHA);

                // Clean up
                SelectObject(hdcMem, hOldBitmap);
                DeleteObject(hBitmap);
                DeleteDC(hdcMem);
                ReleaseDC(NULL, hdcScreen);
            }
            static int a;
            if (!config_window && g_enable && a-- < 0) { a = 10; SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE); }
        }
        return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int WINAPI WinMain(
    HINSTANCE hInstance,  // handle to current instance
    HINSTANCE hPrevInstance,  // handle to previous instance
    LPSTR lpCmdLine,      // pointer to command line
    int nCmdShow          // show state of window
)
{
    {
        GetModuleFileName(hInstance, g_configfile, sizeof(g_configfile));
        char* p = g_configfile;
        while (*p) p++;
        while (p >= g_configfile && *p != '.' && *p != '\\') p--;
        strcpy(++p, "ini");
    }
    WNDCLASS wc = { 0, };

    wc.style = 0;
    wc.lpfnWndProc = m_WndProc;
    g_hInstance = wc.hInstance = hInstance;
    wc.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON1));
    wc.hbrBackground = NULL;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = "CockosTunView";
    RegisterClass(&wc);

    HWND h = CreateWindowEx(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED, wc.lpszClassName, "CockosTunView", 0,
        0, 0, 10, 10,
        NULL, NULL, hInstance, NULL);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}

BOOL systray_add(HWND hwnd, UINT uID, HICON hIcon, LPSTR lpszTip)
{
    NOTIFYICONDATA tnid;
    tnid.cbSize = sizeof(NOTIFYICONDATA);
    tnid.hWnd = hwnd;
    tnid.uID = uID;
    tnid.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE;
    tnid.uCallbackMessage = WM_SYSTRAY;
    tnid.hIcon = hIcon;
    lstrcpyn(tnid.szTip, lpszTip, sizeof(tnid.szTip) - 1);
    return (Shell_NotifyIcon(NIM_ADD, &tnid));
}

BOOL systray_del(HWND hwnd, UINT uID) {
    NOTIFYICONDATA tnid;
    tnid.cbSize = sizeof(NOTIFYICONDATA);
    tnid.hWnd = hwnd;
    tnid.uID = uID;
    return(Shell_NotifyIcon(NIM_DELETE, &tnid));
}
