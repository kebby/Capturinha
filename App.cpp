#include "App.h"

#include <windows.h>

#include <atlbase.h>
#include <atlapp.h>
#include <atlframe.h>
#include <atlctrls.h>
#include <atldlgs.h>
#include <atlctrlw.h>
#include <atltheme.h>
#include <atlmisc.h>

#include "resource.h"
#include "system.h"
#include "graphics.h"

#include "screencapture.h"
#include "audiocapture.h"

CAppModule _Module;


CaptureConfig Config = 
{
    .Filename = "c:\\temp\\capture",
}; 

IScreenCapture* Capture = 0;

constexpr int WM_SETCAPTURE = WM_USER + 1;

//-------------------------------------------------------------------
//-------------------------------------------------------------------

constexpr float aCenter = 0.5;
constexpr float aLeft = 0;
constexpr float aRight = 1;
constexpr float aTop = 0;
constexpr float aBottom = 1;
constexpr float aligned = -1;

static RECT Rect (const RECT &ref, float refAnchorX, float refAnchorY, int width, int height, float anchorX = aligned, float anchorY = aligned, int offsX = 0, int offsY = 0)
{
    float ax = Lerp<float>(refAnchorX, (float)ref.left, (float)ref.right);
    float ay = Lerp<float>(refAnchorY, (float)ref.top, (float)ref.bottom);

    if (anchorX <= aligned) anchorX = refAnchorX;
    if (anchorY <= aligned) anchorY = refAnchorY;

    int left = (int)roundf(ax - width * anchorX + offsX);
    int top = (int)roundf(ay - height * anchorY + offsY);
    return RECT{ .left = left, .top = top, .right = left + width, .bottom = top + height };
}

//-------------------------------------------------------------------
//-------------------------------------------------------------------

class SetupForm : public CWindowImpl<SetupForm>
{
public:

    CButton startCapture;
    CComboBox videoOut;
    CButton recordWhenFS;

    DECLARE_WND_CLASS("SetupForm");

    BEGIN_MSG_MAP(SetupForm)
        MESSAGE_HANDLER(WM_ERASEBKGND, OnEraseBackground)
        COMMAND_ID_HANDLER(BN_CLICKED, OnClick)
        //CHAIN_MSG_MAP(CWindowImpl<SetupForm>)
        MESSAGE_HANDLER(WM_CREATE, OnCreate)
    END_MSG_MAP()

    template<class T> void Child(T& child, const RECT& r, const char* text = "", const DWORD style = 0)
    {
        RECT r2 = r;
        child.Create(m_hWnd, r2, text, WS_CHILD | WS_VISIBLE | style, ID_BUTTON);
        child.SetFont(AtlGetStockFont(DEFAULT_GUI_FONT));
    }

    LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
    {
        CFont font = AtlGetStockFont(DEFAULT_GUI_FONT);
        SetFont(font);

        CRect r, cr;
        CStatic label;
        Array<String> strings;
        GetClientRect(&cr);
        cr.InflateRect(-10, -10);
        CRect line = Rect(cr, 0, 0, cr.Width(), 20);

        r = Rect(line, 0, 0, 100, line.Height(), 0, 0, 0, 4);
        Child(label, r, "Capture screen");

        r = Rect(line, aLeft, aTop, 300, line.Height(), aLeft, aTop, 100);
        Child(videoOut, r, "", CBS_DROPDOWNLIST | CBS_HASSTRINGS);        
        GetVideoOutputs(strings);
        for (auto out : strings)
            videoOut.AddString(out);
        videoOut.SetCurSel(0);

        line.OffsetRect(0, 25);

        r = Rect(line, aLeft, aTop, 300, line.Height(), aLeft, aTop, 100);
        Child(recordWhenFS, r, "Only record when fullscreen", BS_CHECKBOX);

        //CRect rlabel = Rect(line, 0, 0, 100, line.Height());
        //Child(label, rlabel, "Capture screen");

        r = Rect(cr, aRight, aBottom, 130, 25, aRight, aBottom);
        startCapture.Create(m_hWnd, r, "Start", WS_CHILD | WS_VISIBLE, ID_BUTTON);
        startCapture.SetFont(font);


        return 0;
    }

    //LRESULT OnClick(WPARAM wParam, LPNMHDR nmhdr, BOOL& bHandled)
    //LRESULT OnClick(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
    LRESULT OnClick(UINT w1, UINT w2, HWND hwnd, BOOL& bHandled)
    {
        if (hwnd == startCapture)
        {
            Config.OutputIndex = videoOut.GetCurSel();
            Config.RecordOnlyFullscreen = !!recordWhenFS.GetCheck();
            SendMessage(GetParent(), WM_SETCAPTURE, 1, 0);
            return 1;
        }
        if (hwnd == recordWhenFS) recordWhenFS.SetCheck(recordWhenFS.GetCheck() ? 0 : 1);

        return 0;
    }


    LRESULT OnEraseBackground(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled)
    {
        return 0;
        /*
        CDC dc = (HDC)wParam;
        CBrush brush;
        brush.CreateSolidBrush(0x00f0f0f0);

        RECT cr;
        this->GetClientRect(&cr);
        dc.FillRect(&cr, brush);

        return 1;
        */
    }

};

//-------------------------------------------------------------------
//-------------------------------------------------------------------

class StatsForm : public CWindowImpl<StatsForm>, CIdleHandler
{
public:

    CStatic statusText;
    CButton stopCapture;

    DECLARE_WND_CLASS("StatsForm");

    BEGIN_MSG_MAP(StatsForm)
        MESSAGE_HANDLER(WM_ERASEBKGND, OnEraseBackground)
        COMMAND_ID_HANDLER(BN_CLICKED, OnClick)
        MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
        MESSAGE_HANDLER(WM_CREATE, OnCreate)
    END_MSG_MAP()

    template<class T> void Child(T& child, const RECT& r, const char* text = "", const DWORD style = 0)
    {
        RECT r2 = r;
        child.Create(m_hWnd, r2, text, WS_CHILD | WS_VISIBLE | style, ID_BUTTON);
        child.SetFont(AtlGetStockFont(DEFAULT_GUI_FONT));
    }

    LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
    {
        CFont font = AtlGetStockFont(DEFAULT_GUI_FONT);
        SetFont(font);

        CRect r, cr;
        CStatic label;
        Array<String> strings;
        GetClientRect(&cr);
        cr.InflateRect(-10, -10);
        CRect line = Rect(cr, 0, 0, cr.Width(), 20);

       
        Child(statusText, line, "Status text");


        r = Rect(cr, aRight, aBottom, 130, 25, aRight, aBottom);
        Child(stopCapture, r, "Stop");

        CMessageLoop* pLoop = _Module.GetMessageLoop();
        ATLASSERT(pLoop != NULL);
        //pLoop->AddMessageFilter(this);
        pLoop->AddIdleHandler(this);

        return 0;
    }

    LRESULT OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
    {
        CMessageLoop* pLoop = _Module.GetMessageLoop();
        ATLASSERT(pLoop != NULL);
        //pLoop->RemoveMessageFilter(this);
        pLoop->RemoveIdleHandler(this);

        SendMessage(GetParent(), WM_SETCAPTURE, 0, 0);
        bHandled = FALSE;
        return 1;
    }

    //LRESULT OnClick(WPARAM wParam, LPNMHDR nmhdr, BOOL& bHandled)
    //LRESULT OnClick(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
    LRESULT OnClick(UINT w1, UINT w2, HWND hwnd, BOOL& bHandled)
    {
        if (hwnd == stopCapture)
        {
            SendMessage(GetParent(), WM_SETCAPTURE, 0, 0);
            return 1;
        }

        return 0;
    }


    LRESULT OnEraseBackground(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled)
    {
        return 0;
        /*
        CDC dc = (HDC)wParam;
        CBrush brush;
        brush.CreateSolidBrush(0x00f0f0f0);

        RECT cr;
        this->GetClientRect(&cr);
        dc.FillRect(&cr, brush);

        return 1;
        */
    }


    // Inherited via CIdleHandler
    virtual BOOL OnIdle() override
    {
        if (!Capture) return 0;

        auto stats = Capture->GetStats();
        auto text = String::PrintF("recd %5d frames, dupl %5d frames, %5.2f FPS, skew %6.2f ms // fs %d\r", stats.FramesCaptured + stats.FramesDuplicated, stats.FramesDuplicated, stats.FPS, stats.AVSkew * 1000, IsFullscreen());

        statusText.SetWindowTextA(text);

        return 0;
    }

};

//-------------------------------------------------------------------
//-------------------------------------------------------------------

class MainFrame : public CFrameWindowImpl<MainFrame>, public CUpdateUI<MainFrame>  
{
public:
     
    SetupForm setupForm;
    StatsForm statsForm;

    DECLARE_FRAME_WND_CLASS_EX(NULL, IDR_MAINFRAME, 0, COLOR_BTNFACE)

    BEGIN_UPDATE_UI_MAP(MainFrame)
    END_UPDATE_UI_MAP()

    BEGIN_MSG_MAP(MainFrame)
        MESSAGE_HANDLER(WM_SETCAPTURE, OnSetCapture)
        CHAIN_MSG_MAP(CFrameWindowImpl<MainFrame>)
        MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
        CHAIN_MSG_MAP(CUpdateUI<MainFrame>)
        MESSAGE_HANDLER(WM_CREATE, OnCreate)
    END_MSG_MAP()

    // Handler prototypes (uncomment arguments if needed):
    //	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
    //	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
    //	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)

    LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
    {
        CFont font = AtlGetStockFont(DEFAULT_GUI_FONT);
        SetFont(font);

        CRect cr;
        GetClientRect(&cr);

        setupForm.Create(m_hWnd, cr, "", WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN, ID_BUTTON);      
        statsForm.Create(m_hWnd, cr, "", WS_CHILD | WS_CLIPSIBLINGS | WS_CLIPCHILDREN, ID_BUTTON);

        // register object for message filtering and idle updates
        CMessageLoop* pLoop = _Module.GetMessageLoop();
        ATLASSERT(pLoop != NULL);
        //pLoop->AddMessageFilter(this);
        //pLoop->AddIdleHandler(this);

        return 0;
    }

    LRESULT OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
    {
        // unregister message filtering and idle updates
        CMessageLoop* pLoop = _Module.GetMessageLoop();
        ATLASSERT(pLoop != NULL);
        //pLoop->RemoveMessageFilter(this);
        //pLoop->RemoveIdleHandler(this);

        

        bHandled = FALSE;
        return 1;
    }


    LRESULT OnSetCapture(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled)
    {
        if (wParam)
        {
            ASSERT(!Capture);
            Capture = CreateScreenCapture(Config);
            setupForm.ShowWindow(SW_HIDE);
            statsForm.ShowWindow(SW_SHOW);
            statsForm.SetTimer(1, 30);
            UpdateWindow();
        }
        else
        {
            Delete(Capture);
            setupForm.ShowWindow(SW_SHOW);
            statsForm.KillTimer(1);
            statsForm.ShowWindow(SW_HIDE);            
            UpdateWindow();
        }
        return 1;
    }
};

//-------------------------------------------------------------------
//-------------------------------------------------------------------

int Run(LPTSTR /*lpstrCmdLine*/ = NULL, int nCmdShow = SW_SHOWDEFAULT)
{
    CMessageLoop theLoop;
    _Module.AddMessageLoop(&theLoop);

    MainFrame wndMain;

    RECT winRect = { .left = CW_USEDEFAULT , .top = CW_USEDEFAULT, .right = CW_USEDEFAULT+440, .bottom = CW_USEDEFAULT+200 };
    if (wndMain.CreateEx(0, &winRect, WS_DLGFRAME|WS_SYSMENU) == NULL)
    {
        ATLTRACE(_T("Main window creation failed!\n"));
        return 0;
    }

    wndMain.ShowWindow(nCmdShow);

    int nRet = theLoop.Run();

    _Module.RemoveMessageLoop();
    return nRet;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/, LPSTR lpstrCmdLine, int nCmdShow)
{
    HRESULT hRes = ::CoInitializeEx(NULL, COINIT_MULTITHREADED);
    // If you are running on NT 4.0 or higher you can use the following call instead to 
    // make the EXE free threaded. This means that calls come in on a random RPC thread.
    //	HRESULT hRes = ::CoInitializeEx(NULL, COINIT_MULTITHREADED);
    ATLASSERT(SUCCEEDED(hRes));

    GfxInit();
    InitAudioCapture();

    // this resolves ATL window thunking problem when Microsoft Layer for Unicode (MSLU) is used
    ::DefWindowProc(NULL, 0, 0, 0L);

    AtlInitCommonControls(ICC_COOL_CLASSES | ICC_BAR_CLASSES | ICC_STANDARD_CLASSES);	// add flags to support other controls

    hRes = _Module.Init(NULL, hInstance);
    ATLASSERT(SUCCEEDED(hRes));

    int nRet = Run(lpstrCmdLine, nCmdShow);

    _Module.Term();
    ::CoUninitialize();

    return nRet;
}