#include <windows.h>

#include <atlbase.h>
#include <atlapp.h>
#include <atlframe.h>
#include <atlctrls.h>
#include <atldlgs.h>
#include <atlctrlw.h>
#include <atltheme.h>
#include <atlmisc.h>
#include <atlstr.h>

#include "resource.h"
#include "system.h"
#include "graphics.h"

#include "screencapture.h"
#include "audiocapture.h"

CAppModule _Module;


CaptureConfig Config = 
{
    .Directory = "c:\\temp",
    .NamePrefix = "capture",
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

class SetupForm : public CWindowImpl<SetupForm>, CIdleHandler
{
public:

    CaptureConfig lastConfig;

    CButton startCapture;
    CComboBox videoOut;
    CButton recordWhenFS;
    CComboBox rateControl;
    CStatic rateParamLabel;
    CEdit rateParam;
    CComboBox frameLayout;
    CEdit gopSize;
    CButton captureAudio;
    CComboBox audioOut;
    CComboBox audioCodec;
    CEdit audioRate;
    CEdit directory;
    CButton dirButton;
    CEdit prefix;
    CComboBox container;
    CButton blinkScrlLock;

    DECLARE_WND_CLASS("SetupForm");

    BEGIN_MSG_MAP(SetupForm)
        MESSAGE_HANDLER(WM_ERASEBKGND, OnEraseBackground)
        COMMAND_ID_HANDLER(BN_CLICKED, OnClick)
        MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
        //CHAIN_MSG_MAP(CWindowImpl<SetupForm>)
        MESSAGE_HANDLER(WM_CREATE, OnCreate)
    END_MSG_MAP()

    template<class T> void Child(T& child, const RECT& r, const char* text = "", const DWORD style = 0)
    {
        RECT r2 = r;
        child.Create(m_hWnd, r2, text, WS_CHILD | WS_VISIBLE | style, ID_BUTTON);
        child.SetFont(AtlGetStockFont(DEFAULT_GUI_FONT));
    }

    template<class T> void Dropdown(T& child, const RECT& r, Array<String> &strings)
    {
        RECT r2 = r;
        child.Create(m_hWnd, r2, NULL, WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | CBS_HASSTRINGS, ID_BUTTON);
        child.SetFont(AtlGetStockFont(DEFAULT_GUI_FONT));
        for (auto out : strings)
            child.AddString(out);
        child.SetCurSel(0);
    }

    CComboBox videoCodec;

    static const int labelwidth = 80;

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

        //------------- OutputIndex
        r = Rect(line, 0, 0, labelwidth, line.Height(), 0, 0, 0, 4);
        Child(label, r, "Capture screen");

        r = Rect(line, aLeft, aTop, 300, line.Height(), aLeft, aTop, labelwidth);
        Child(videoOut, r, "", WS_TABSTOP | CBS_DROPDOWNLIST | CBS_HASSTRINGS);        
        GetVideoOutputs(strings);
        for (auto out : strings)
            videoOut.AddString(out);
        videoOut.SetCurSel(0);

        line.OffsetRect(0, 25);

        //------------- RecordOnlyFullscreen
        r = Rect(line, aLeft, aTop, 300, line.Height(), aLeft, aTop, labelwidth);
        Child(recordWhenFS, r, "Only record when fullscreen", WS_TABSTOP | BS_AUTOCHECKBOX);

        line.OffsetRect(0, 25);
       
        //-------------- Codec profile
        CStatic label2;
        r = Rect(line, 0, 0, labelwidth, line.Height(), 0, 0, 0, 4);
        Child(label2, r, "Video codec");

        Array<String> codecs = 
        { 
            "H.264 Main profile",
            "H.264 High profile",
            "H.264 4:4:4 High profile",
            "H.264 lossless",
            "HEVC Main profile",
            "HEVC Main10 profile",
        };
        r = Rect(line, aLeft, aTop, 300, line.Height(), aLeft, aTop, labelwidth);
        Dropdown(videoCodec, r, codecs);
        line.OffsetRect(0, 25);

        //--------------- rate control
        r = Rect(line, 0, 0, labelwidth, line.Height(), 0, 0, 0, 4);
        CStatic label3;
        Child(label3, r, "Rate control");

        Array<String> rateStrs = { "CBR", "Const QP" };
        r = Rect(line, aLeft, aTop, 100, line.Height(), aLeft, aTop, labelwidth);
        Dropdown(rateControl, r, rateStrs );

        r = Rect(line, aLeft, aTop, 80, line.Height(), aLeft, aTop, 240, 4);
        Child(rateParamLabel, r, "");

        r = Rect(line, aLeft, aTop, 60, line.Height(), aLeft, aTop, 320);       
        Child(rateParam, r, "", ES_RIGHT | ES_NUMBER | WS_BORDER);

        line.OffsetRect(0, 25);

        //--------------- video options
        r = Rect(line, 0, 0, labelwidth, line.Height(), 0, 0, 0, 4);
        CStatic label5;
        Child(label5, r, "Frame layout");

        Array<String> layoutStrs = { "I only", "I+P", /* "I+B+P", "I+B+B+P" */};
        r = Rect(line, aLeft, aTop, 100, line.Height(), aLeft, aTop, labelwidth);
        Dropdown(frameLayout, r, layoutStrs);
        frameLayout.SetCurSel(1);

        r = Rect(line, aLeft, aTop, 80, line.Height(), aLeft, aTop, 240, 4);
        CStatic label4;
        Child(label4, r, "GOP length");

        r = Rect(line, aLeft, aTop, 60, line.Height(), aLeft, aTop, 320);
        Child(gopSize, r, "", ES_RIGHT | ES_NUMBER | WS_BORDER);

        line.OffsetRect(0, 25);

        //--------------- audio
        line.OffsetRect(0, 15);

        r = Rect(line, aLeft, aTop, 300, line.Height(), aLeft, aTop, labelwidth);
        Child(captureAudio, r, "Capture audio", WS_TABSTOP | BS_AUTOCHECKBOX);

        line.OffsetRect(0, 25);

        r = Rect(line, 0, 0, labelwidth, line.Height(), 0, 0, 0, 4);
        CStatic label6;
        Child(label6, r, "Audio output");

        r = Rect(line, aLeft, aTop, 300, line.Height(), aLeft, aTop, labelwidth);
        Child(audioOut, r, "", WS_TABSTOP | CBS_DROPDOWNLIST | CBS_HASSTRINGS);
        GetAudioDevices(strings);
        for (auto out : strings)
            audioOut.AddString(out);
        audioOut.SetCurSel(0);

        line.OffsetRect(0, 25);

        //--------------- audio codec         
        r = Rect(line, 0, 0, labelwidth, line.Height(), 0, 0, 0, 4);
        CStatic label7;
        Child(label7, r, "Audio Codec");

        Array<String> acodecStrs = { "PCM, 16bit", "PCM, float", "MP3", "AAC" };
        r = Rect(line, aLeft, aTop, 100, line.Height(), aLeft, aTop, labelwidth);
        Dropdown(audioCodec, r, acodecStrs);

        r = Rect(line, aLeft, aTop, 80, line.Height(), aLeft, aTop, 240, 4);
        CStatic label8;
        Child(label8, r, "Bit rate (kbits/s)");

        r = Rect(line, aLeft, aTop, 60, line.Height(), aLeft, aTop, 320);
        Child(audioRate, r, "", ES_RIGHT | ES_NUMBER | WS_BORDER);

        line.OffsetRect(0, 25);

        //--------------- directory
        line.OffsetRect(0, 15);

        r = Rect(line, 0, 0, labelwidth, line.Height(), 0, 0, 0, 4);
        CStatic label9;
        Child(label9, r, "Output folder");

        r = Rect(line, aLeft, aTop, 265, line.Height(), aLeft, aTop, labelwidth);
        Child(directory, r, "c:\\temp", ES_LEFT | WS_BORDER);

        r = Rect(line, aLeft, aTop, 30, line.Height(), aLeft, aTop, 350);
        Child(dirButton, r, "...", BS_PUSHBUTTON);

        line.OffsetRect(0, 25);

        //--------------- prefix / container

        r = Rect(line, 0, 0, labelwidth, line.Height(), 0, 0, 0, 4);
        CStatic label10;
        Child(label10, r, "Name prefix");
       
        r = Rect(line, aLeft, aTop, 150, line.Height(), aLeft, aTop, labelwidth);
        Child(prefix, r, "capture", ES_LEFT | WS_BORDER);

        r = Rect(line, aLeft, aTop, 80, line.Height(), aLeft, aTop, 240, 4);
        CStatic label11;
        Child(label11, r, "Container");

        r = Rect(line, aLeft, aTop, 60, line.Height(), aLeft, aTop, 320);
        Array<String> containerStrs = { "avi", "mp4", "mov", "mkv" };
        Dropdown(container, r, containerStrs);


        line.OffsetRect(0, 25);

        //--------------- options, start

        r = Rect(cr, aLeft, aBottom, 200, 25, aLeft, aBottom);
        Child(blinkScrlLock, r, "Flash Scroll Lock when recording", WS_TABSTOP | BS_AUTOCHECKBOX);

        r = Rect(cr, aRight, aBottom, 130, 25, aRight, aBottom);
        startCapture.Create(m_hWnd, r, "Start", WS_TABSTOP | WS_CHILD | WS_VISIBLE, ID_BUTTON);
        startCapture.SetFont(font);

        lastConfig = Config;
        ConfigToControls(true);

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
        if (hwnd == startCapture)
        {
            WriteFileUTF8(Json::Serialize(Config, true), "config.json");
            SendMessage(GetParent(), WM_SETCAPTURE, 1, 0);
            return 1;
        }
        if (hwnd == dirButton)
        {
            CFolderDialog dlg(m_hWnd, "Select the capture destination folder");
            dlg.SetInitialFolder("c:\\temp", true);
            if (dlg.DoModal(m_hWnd) == IDOK)
            {
                directory.SetWindowTextA(dlg.GetFolderPath());
            }

        }

        return 0;
    }

    static int GetInt(const CWindow& wnd)
    {
        char rp[10];
        wnd.GetWindowTextA(rp, 10);
        return atoi(rp);
    }

    static String GetString(const CWindow& wnd)
    {
        char buffer[1024];
        wnd.GetWindowTextA(buffer, 1024);
        return buffer;
    }

    void ConfigFromControls()
    {
        Config.OutputIndex = videoOut.GetCurSel();
        Config.RecordOnlyFullscreen = !!recordWhenFS.GetCheck();
        Config.CodecCfg.Profile = (CodecProfile)videoCodec.GetCurSel();
        Config.CodecCfg.UseBitrateControl = (BitrateControl)rateControl.GetCurSel();
        Config.CodecCfg.FrameCfg = (FrameConfig)frameLayout.GetCurSel();

        int rpi = GetInt(rateParam);
        switch (Config.CodecCfg.UseBitrateControl)
        {
        case BitrateControl::CBR:
            Config.CodecCfg.BitrateParameter = Clamp(rpi, 200, 500000);
            break;
        case BitrateControl::CONSTQP:
            Config.CodecCfg.BitrateParameter = Clamp(rpi, 1, 52);
            break;
        }

        Config.CodecCfg.GopSize = Clamp(GetInt(gopSize), 1, 10000);
        if (Config.CodecCfg.FrameCfg == FrameConfig::I)
            Config.CodecCfg.GopSize = 1;

        Config.CaptureAudio = !!captureAudio.GetCheck();
        Config.AudioOutputIndex = audioOut.GetCurSel();
        Config.UseAudioCodec = (AudioCodec)audioCodec.GetCurSel();
        Config.AudioBitrate = Clamp(GetInt(audioRate), 32, 320);

        Config.Directory = GetString(directory);
        Config.NamePrefix = GetString(prefix);
        Config.UseContainer = (Container)container.GetCurSel();
        Config.BlinkScrollLock = !!blinkScrlLock.GetCheck();
    }

    // Inherited via CIdleHandler
    virtual BOOL OnIdle() override
    {
        ConfigFromControls();
        ConfigToControls(false);
        return 0;
    }

    void ConfigToControls(bool force)
    {
        // set control states from config
        if (force)
        {
            videoOut.SetCurSel(Config.OutputIndex);
            recordWhenFS.SetCheck(Config.RecordOnlyFullscreen);
            videoCodec.SetCurSel((int)Config.CodecCfg.Profile);
            rateControl.SetCurSel((int)Config.CodecCfg.UseBitrateControl);
            rateParam.SetWindowTextA(String::PrintF("%d", Config.CodecCfg.BitrateParameter));
            frameLayout.SetCurSel((int)Config.CodecCfg.FrameCfg);
            gopSize.SetWindowTextA(String::PrintF("%d", Config.CodecCfg.GopSize));
            captureAudio.SetCheck(Config.CaptureAudio);
            audioOut.SetCurSel(Config.AudioOutputIndex);
            audioCodec.SetCurSel((int)Config.UseAudioCodec);
            audioRate.SetWindowTextA(String::PrintF("%d", Config.AudioBitrate));
            directory.SetWindowTextA(Config.Directory);
            prefix.SetWindowTextA(Config.NamePrefix);
            container.SetCurSel((int)Config.UseContainer);
            blinkScrlLock.SetCheck(Config.BlinkScrollLock);
        }

        // enable/disable options etc

        if (force || lastConfig.CodecCfg.Profile != Config.CodecCfg.Profile)
        {
            if (Config.CodecCfg.Profile == CodecProfile::H264_LOSSLESS)
            {
                rateControl.EnableWindow(false);
                rateParam.EnableWindow(false);
                Config.CodecCfg.UseBitrateControl = BitrateControl::CONSTQP;
                Config.CodecCfg.BitrateParameter = 1;
            }
            else
            {
                rateControl.EnableWindow(true);
                rateParam.EnableWindow(true);
            }
        }

        if (force || lastConfig.CodecCfg.UseBitrateControl != Config.CodecCfg.UseBitrateControl)
        {
            if (Config.CodecCfg.UseBitrateControl == BitrateControl::CBR)
            {
                rateParamLabel.SetWindowTextA("Bit rate (kbits/s)");
                if (lastConfig.CodecCfg.UseBitrateControl != Config.CodecCfg.UseBitrateControl)
                    Config.CodecCfg.BitrateParameter = 20000;
            }
            else if (Config.CodecCfg.UseBitrateControl == BitrateControl::CONSTQP)
            {
                rateParamLabel.SetWindowTextA("Constant QP");
                if (lastConfig.CodecCfg.UseBitrateControl != Config.CodecCfg.UseBitrateControl)
                    Config.CodecCfg.BitrateParameter = 20;
            }
            rateParam.SetWindowTextA(String::PrintF("%d", Config.CodecCfg.BitrateParameter));
        }

       
        if (force || lastConfig.CodecCfg.FrameCfg != Config.CodecCfg.FrameCfg)
        {
            gopSize.EnableWindow(Config.CodecCfg.FrameCfg != FrameConfig::I);
        }

        if (force || lastConfig.CaptureAudio != Config.CaptureAudio)
        {
            audioOut.EnableWindow(Config.CaptureAudio);
            audioCodec.EnableWindow(Config.CaptureAudio);
        }

        if (force || lastConfig.UseAudioCodec != Config.UseAudioCodec || lastConfig.CaptureAudio != Config.CaptureAudio)
        {
            audioRate.EnableWindow(Config.CaptureAudio && Config.UseAudioCodec >= AudioCodec::MP3);
        }

        lastConfig = Config;
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

struct DlgMessageFiler : CMessageFilter
{
    BOOL PreTranslateMessage(MSG* pMsg) override
    {
        if (IsDialogMessage(hWnd, pMsg))
            return 1;

        return 0;
    }

    HWND hWnd;
};

int Run(LPTSTR /*lpstrCmdLine*/ = NULL, int nCmdShow = SW_SHOWDEFAULT)
{  
    CMessageLoop theLoop;
    _Module.AddMessageLoop(&theLoop);

    if (FileExists("config.json"))
    {
        String json = ReadFileUTF8("config.json");
        Array<String> errors;
        if (json.Length()>0 && !Json::Deserialize(json, Config, errors))
        {
            String allerrors = String::Join(errors, "\n");
            Fatal(String("Could not read config.json: \n\n") + allerrors);
        }
    }

    MainFrame wndMain;

    RECT winRect = { .left = CW_USEDEFAULT , .top = CW_USEDEFAULT, .right = CW_USEDEFAULT+420, .bottom = CW_USEDEFAULT+380 };
    if (wndMain.CreateEx(0, &winRect, WS_DLGFRAME|WS_SYSMENU|WS_MINIMIZEBOX) == NULL)
    {
        ATLTRACE(_T("Main window creation failed!\n"));
        return 0;
    }

    DlgMessageFiler msgFilter;
    msgFilter.hWnd = wndMain;

    theLoop.AddMessageFilter(&msgFilter);

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