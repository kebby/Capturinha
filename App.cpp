//
// Copyright (C) Tammo Hinrichs 2021. All rights reserved.
// Licensed under the MIT License. See LICENSE.md file for full license information
//

#define WIN32_LEAN_AND_MEAN
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

CaptureConfig Config = {};

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

static int DPI = 96;

inline int WithDpi(int s) { return s>=0 ? s * DPI / 96 : s; }
inline int WithoutDpi(int s) { return s >= 0 ? s * 96 / DPI : s; }

inline RECT WithDpi(const RECT& r)
{
    return RECT{ .left = WithDpi(r.left), .top = WithDpi(r.top), .right = WithDpi(r.right), .bottom = WithDpi(r.bottom) };
}


inline RECT WithoutDpi(const RECT& r)
{
    return RECT{ .left = WithoutDpi(r.left), .top = WithoutDpi(r.top), .right = WithoutDpi(r.right), .bottom = WithoutDpi(r.bottom) };

}
static RECT Rect (const RECT &ref, float refAnchorX, float refAnchorY, int width, int height, float anchorX = aligned, float anchorY = aligned, int offsX = 0, int offsY = 0)
{
    float ax = Lerp<float>(refAnchorX, (float)ref.left, (float)ref.right);
    float ay = Lerp<float>(refAnchorY, (float)ref.top, (float)ref.bottom);

    if (anchorX <= aligned) anchorX = refAnchorX;
    if (anchorY <= aligned) anchorY = refAnchorY;

    int left = (int)roundf(ax - width * anchorX + offsX);
    int top = (int)roundf(ay - height * anchorY + offsY);
    return WithDpi(RECT{ .left = left, .top = top, .right = left + width, .bottom = top + height });
}

//-------------------------------------------------------------------
//-------------------------------------------------------------------

class SetupForm : public CWindowImpl<SetupForm>, CIdleHandler
{
public:

    CFont font;

    CaptureConfig lastConfig;

    CButton startCapture;
    CComboBox videoOut;
    CButton recordWhenFS;
    CButton upscale;
    CEdit upscaleTo;
    CStatic upscaleToLabel;
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
    CToolTipCtrl tooltips;

    DECLARE_WND_CLASS_EX("SetupForm", 0, COLOR_MENU);

    BEGIN_MSG_MAP(SetupForm)
        COMMAND_ID_HANDLER(BN_CLICKED, OnClick)
        MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
        MESSAGE_HANDLER(WM_CREATE, OnCreate)
    END_MSG_MAP()

    template<class T> void Child(T& child, const RECT& r, const char* text = "", const DWORD style = 0)
    {
        RECT r2 = r;
        child.Create(m_hWnd, r2, text, WS_CHILD | WS_VISIBLE | style, 0);
        child.SetFont(font);
    }

    void Dropdown(CComboBox& child, const RECT& r, Array<String> &strings)
    {
        RECT r2 = r;
        child.Create(m_hWnd, r2, NULL, WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | CBS_HASSTRINGS, 0);
        child.SetFont(font);
        for (auto out : strings)
            child.AddString(out);
        child.SetCurSel(0);
    }

    CComboBox videoCodec;

    static const int labelwidth = 85;

    LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
    {
        font.CreateFontA(WithDpi(16), 0, 0, 0, FW_DONTCARE, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, "Segoe UI");
        SetFont(font);

        CRect r, cr;
        CStatic label;
        Array<String> strings;
        GetClientRect(&cr);
        cr = WithoutDpi(cr);
        cr.InflateRect(-10,-10);
        CRect line = WithoutDpi(Rect(cr, 0, 0, cr.Width(), 20));

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

        //------------- Recording options
        r = Rect(line, aLeft, aTop, 300, line.Height(), aLeft, aTop, labelwidth);
        Child(recordWhenFS, r, "Only record when fullscreen", WS_TABSTOP | BS_AUTOCHECKBOX);

        line.OffsetRect(0, 20);

        r = Rect(line, aLeft, aTop, 170, line.Height(), aLeft, aTop, labelwidth);
        Child(upscale, r, "Oldschool Upscale to at least", WS_TABSTOP | BS_AUTOCHECKBOX);

        r = Rect(line, aLeft, aTop, 40, line.Height(), aLeft, aTop, 260);
        Child(upscaleTo, r, "", ES_RIGHT | ES_NUMBER | WS_BORDER);

        r = Rect(line, aLeft, aTop, 100, line.Height(), aLeft, aTop, 305, 1);
        Child(upscaleToLabel, r, "lines");


        line.OffsetRect(0, 25);

        //-------------- Codec profile
        line.OffsetRect(0, 10);

        CStatic label2;
        r = Rect(line, 0, 0, labelwidth, line.Height(), 0, 0, 0, 4);
        Child(label2, r, "Video codec");

        Array<String> codecs = 
        { 
            "H.264 Main profile",
            "H.264 High profile",
            "H.264 4:4:4 High profile",
            "HEVC Main profile",
            "HEVC Main10 profile",
            "HEVC 4:4:4 Main profile",
            "HEVC 4:4:4 Main10 profile",
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

        r = Rect(line, aLeft, aTop, 100, line.Height(), aLeft, aTop, 240, 4);
        Child(rateParamLabel, r, "");

        r = Rect(line, aLeft, aTop, 60, line.Height(), aLeft, aTop, 325);       
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

        r = Rect(line, aLeft, aTop, 60, line.Height(), aLeft, aTop, 325);
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

        r = Rect(line, aLeft, aTop, 100, line.Height(), aLeft, aTop, 240, 4);
        CStatic label8;
        Child(label8, r, "Bit rate (kbits/s)");

        r = Rect(line, aLeft, aTop, 60, line.Height(), aLeft, aTop, 325);
        Child(audioRate, r, "", ES_RIGHT | ES_NUMBER | WS_BORDER);

        line.OffsetRect(0, 25);

        //--------------- directory
        line.OffsetRect(0, 15);

        r = Rect(line, 0, 0, labelwidth, line.Height(), 0, 0, 0, 4);
        CStatic label9;
        Child(label9, r, "Output folder");

        r = Rect(line, aLeft, aTop, 265, line.Height(), aLeft, aTop, labelwidth);
        Child(directory, r, "c:\\temp", ES_LEFT | WS_BORDER);

        r = Rect(line, aLeft, aTop, 30, line.Height(), aLeft, aTop, 355);
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

        r = Rect(line, aLeft, aTop, 60, line.Height(), aLeft, aTop, 325);
        Array<String> containerStrs = { "mp4", "mov", "mkv" };
        Dropdown(container, r, containerStrs);


        line.OffsetRect(0, 25);

        //--------------- options, start

        r = Rect(cr, aLeft, aBottom, 200, 25, aLeft, aBottom);
        Child(blinkScrlLock, r, "Flash Scroll Lock when recording", WS_TABSTOP | BS_AUTOCHECKBOX);

        r = Rect(cr, aRight, aBottom, 130, 25, aRight, aBottom);
        startCapture.Create(m_hWnd, r, "Start", WS_TABSTOP | WS_CHILD | WS_VISIBLE, 0);
        startCapture.SetFont(font);
       
        tooltips.Create(*this);        
        TOOLINFO toolInfo = {
            .cbSize = sizeof(toolInfo),
            .uFlags = TTF_IDISHWND | TTF_SUBCLASS,
            .hwnd = *this,
            .uId = (UINT_PTR)(HWND)startCapture,
            .lpszText = (LPTSTR)"Win-F9",
        };
        tooltips.AddTool(&toolInfo);

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

    // Inherited via CIdleHandler
    BOOL OnIdle() override
    {
        ConfigFromControls();
        ConfigToControls(false);
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
        Config.Upscale = !!upscale.GetCheck();
        Config.UpscaleTo = Clamp(GetInt(upscaleTo), 720, 4320);
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

    void ConfigToControls(bool force)
    {
        // set control states from config
        if (force)
        {
            videoOut.SetCurSel(Config.OutputIndex);
            recordWhenFS.SetCheck(Config.RecordOnlyFullscreen);
            upscale.SetCheck(Config.Upscale);
            upscaleTo.SetWindowTextA(String::PrintF("%d", Config.UpscaleTo));
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
        if (force || lastConfig.Upscale != Config.Upscale)
        {
            upscaleTo.EnableWindow(Config.Upscale);
        }

        if (force || lastConfig.CodecCfg.Profile != Config.CodecCfg.Profile)
        {
            rateControl.EnableWindow(true);
            rateParam.EnableWindow(true);
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
                    Config.CodecCfg.BitrateParameter = 24;
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
};

//-------------------------------------------------------------------
//-------------------------------------------------------------------

class StatsForm : public CWindowImpl<StatsForm>, CIdleHandler
{
public:

    CFont font, smallFont, bigFont;

    CStatic statusText;
    CButton stopCapture;
    CToolTipCtrl tooltips;

    double maxRate = 0;
    int stat = -1;
    int lastStat = -1;

    DECLARE_WND_CLASS_EX("StatsForm",0, COLOR_MENU);

    BEGIN_MSG_MAP(StatsForm)
        MESSAGE_HANDLER(WM_PAINT, OnPaint)
        COMMAND_ID_HANDLER(BN_CLICKED, OnClick)
        MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
        MESSAGE_HANDLER(WM_CREATE, OnCreate)
        MESSAGE_HANDLER(WM_ERASEBKGND, OnEraseBackground)
    END_MSG_MAP()

    template<class T> void Child(T& child, const RECT& r, const char* text = "", const DWORD style = 0)
    {
        RECT r2 = r;
        child.Create(m_hWnd, r2, text, WS_CHILD | WS_VISIBLE | style, 0);
        child.SetFont(font);
    }

    LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
    {        
        font.CreateFontA(WithDpi(16), 0, 0, 0, FW_DONTCARE, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, "Segoe UI");
        smallFont.CreateFontA(WithDpi(11), 0, 0, 0, FW_DONTCARE, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, "Segoe UI");
        bigFont.CreateFontA(WithDpi(24), 0, 0, 0, FW_DONTCARE, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, "Bahnschrift");

        CRect r, cr;
        GetClientRect(&cr);
        cr = WithoutDpi(cr);
        cr.InflateRect(-10, -10);

        r = Rect(cr, aLeft, aBottom, 230, 25, aLeft, aBottom);
        Child(statusText, r, "");
        statusText.SetFont(bigFont);

        r = Rect(cr, aRight, aBottom, 130, 25, aRight, aBottom);
        Child(stopCapture, r, "Stop");

        tooltips.Create(*this);
        TOOLINFO toolInfo = {
            .cbSize = sizeof(toolInfo),
            .uFlags = TTF_IDISHWND | TTF_SUBCLASS,
            .hwnd = *this,
            .uId = (UINT_PTR)(HWND)stopCapture,
            .lpszText = (LPTSTR)"Win-F9",
        };
        tooltips.AddTool(&toolInfo);



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

    LRESULT OnEraseBackground(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
    {
        return 1;
    }

    static COLORREF CRef(const Vec3& color)
    {
        return (int(255 * color.x)) | (int(255 * color.y ) << 8) | (int(255 * color.z ) << 16);
    }

    static float VUToScreen(float vu)
    {
        return powf(vu, 0.3f);
    }

    static float DecibelToLinear(float dB) { return powf(10, dB / 20); };

    void PaintVU(CDC& dc, const RECT& rect, const CaptureStats& stats)
    {
        CPen pen;
        pen.CreatePen(PS_SOLID, 1, 0xc0c0c0);
        CPen pen2;
        pen2.CreatePen(PS_SOLID, 2, 0xd0d0d0);
        CPen pen3;
        pen3.CreatePen(PS_SOLID, 1, 0xe0e0e0);

        dc.SelectPen(pen);
        dc.SelectStockBrush(NULL_BRUSH);
        dc.Rectangle(&rect);
        CRect area = rect;
        area.InflateRect(-1, -1);

        dc.SelectFont(smallFont);
        dc.SetTextColor(0xa0a0a0);
        dc.SetBkMode(TRANSPARENT);

        int d10 = WithDpi(10);
        int d20 = WithDpi(20);

        for (int db = 1; db < 100; db++)
        {
            if (db > 50 && db % 10) continue;
            if (db > 20 && db % 2) continue;
            float v = VUToScreen(DecibelToLinear((float)-db));                   
            int x = area.left + int(v * area.Width() + 1);
                       
            if ((db >= 30 && db <= 60 && !(db % 10)) || (db<30 && !(db % 6)))
            {
                CRect textRect(x-d10, area.bottom, x+d10, area.bottom+d10);
                dc.DrawTextA(String::PrintF("-%d",db), -1, &textRect, DT_CENTER);
                dc.SelectPen(pen2);
            }
            else
                dc.SelectPen(pen3);
            dc.MoveTo(x, area.top);
            dc.LineTo(x, area.bottom);
        }
        CRect textRect2(area.left, area.bottom, area.left+WithDpi(d20), area.bottom + d10);
        dc.DrawTextA("dBFS", -1, &textRect2, DT_LEFT);
        CRect textRect3(area.right-d20, area.bottom, area.right, area.bottom + d10);
        dc.DrawTextA("0", -1, &textRect3, DT_RIGHT);


        CBrush peak;
        peak.CreateSolidBrush(CRef(Vec3(1, 0.3f, 0.3f)));
        dc.SelectStockPen(NULL_PEN);
        dc.SelectBrush(peak);

        int nch;
        for (nch = 0; stats.VU[nch] >= 0; nch++) {}
       
        for (int ch = 0; ch < nch; ch++)
        {
            float v = VUToScreen(Clamp(stats.VU[ch], 0.0f, 1.0f));
            int t = area.top + ch * area.Height() / nch;
            int b = area.top + (ch + 1) * area.Height() / nch +1;
            int l = area.left;
            int r = area.left + int(v * area.Width()+1);
            Vec3 ca(0, 0.5, 0);
            Vec3 cb = Lerp(v, ca, Vec3(1, 0.5, 0));
            
            dc.GradientFillRect(*CRect(l, t, r, b), CRef(ca), CRef(cb), TRUE);

            int px = area.left + int(VUToScreen(stats.VUPeak[ch]) * area.Width() + 1);
            dc.Rectangle(px - 1, t, px + 1, b);
        }

    }

    void PaintGraph(CDC& dc, const RECT &rect, const Vec3& color, const char* label, const char* unitFmt, size_t nPoints, double max, double avg, Func<double(int)> getPoint)
    {
        CPen pen;
        pen.CreatePen(PS_SOLID, 1, 0xc0c0c0);

        dc.SelectPen(pen);
        dc.SelectStockBrush(NULL_BRUSH);
        dc.Rectangle(&rect);

        CRect grapharea = rect;
        grapharea.InflateRect(-1, -1);

        // the points for the graph...
        int np = Min(grapharea.Width() + 1, (int)nPoints);
        int offs = (int)nPoints - np;
        double gh = (double)grapharea.Height() + 1;
        Array<POINT> points(POINT{ .x = grapharea.left, .y = grapharea.bottom });
        for (int i = 0; i < np; i++)
        {
            POINT point{ .x = grapharea.left + i , .y = grapharea.bottom - LONG(getPoint(i+offs)*gh/max) };
            points.PushTail(point);
        }
        points.PushTail(POINT{ .x = grapharea.left + np - 1, .y = grapharea.bottom });

        CBrush green;
        Vec3 grcol = Lerp(0.75f, color, Vec3(1));
        green.CreateSolidBrush(CRef(grcol));

        dc.SelectStockPen(NULL_PEN);
        dc.SelectBrush(green);

        dc.Polygon(&points[0], np + 2);

        CPen darkgreen;
        
        darkgreen.CreatePen(PS_SOLID, 1, CRef(color));

        dc.SelectPen(darkgreen);
        dc.SelectStockBrush(NULL_BRUSH);
        dc.MoveTo(points[1]);
        for (int i = 2; i < np - 1; i++)
            dc.LineTo(points[i]);

        if (avg >= 0)
        {
            int y = grapharea.bottom - LONG(avg * gh / max);
            dc.MoveTo(grapharea.left, y);
            dc.LineTo(grapharea.right, y);
        }

        int d5 = WithDpi(5);
        int d10 = WithDpi(10);
        int d100 = WithDpi(100);

        CRect textRect(grapharea.left + d5, grapharea.top + 1, grapharea.left + d5 + d100, grapharea.top + 1 + d10);
        dc.SelectFont(smallFont);
        dc.SetTextColor(CRef(color * 0.5f));
        dc.SetBkMode(TRANSPARENT);
        dc.DrawTextA(label, -1, &textRect, DT_LEFT);

        CRect textRect2(grapharea.right - d5 - 2*d100, grapharea.top + 1, grapharea.right - d5, grapharea.top + 1 + d10);        
        dc.DrawTextA(String::PrintF(unitFmt, max), -1, &textRect2, DT_RIGHT);
    }

    void PaintText(CDC &dc, const char* left, const char* right, CRect& rect, int leftw)
    {
        dc.SelectFont(font);
        dc.SetTextColor(0x000000);
        dc.SetBkMode(TRANSPARENT);

        int d20 = WithDpi(20);
        leftw = WithDpi(leftw);

        CRect r1 = WithDpi(rect); r1.right = r1.left + leftw; r1.bottom = r1.top + d20;
        CRect r2 = WithDpi(rect); ; r2.left = r2.left + leftw; r2.bottom = r2.top + d20;
        dc.DrawTextA(left, -1, &r1, DT_LEFT);
        dc.DrawTextA(right, -1, &r2, DT_RIGHT|DT_PATH_ELLIPSIS);

        rect.OffsetRect(0, 20);
    }

    LRESULT OnPaint(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/)
    {
        RECT cr;
        GetClientRect(&cr);      
        cr = WithoutDpi(cr);
        auto w = cr.right - cr.left - 20;
        auto h = cr.bottom - cr.top - 60;
        PAINTSTRUCT ps;
        CDC maindc = BeginPaint(&ps);
        CDC dc;
        dc.CreateCompatibleDC(maindc);
        CBitmap bitmap;
        bitmap.CreateCompatibleBitmap(maindc, WithDpi(w), WithDpi(h));

        dc.SelectBitmap(bitmap);

        CRect area(0, 0, w, h);

        // clear
        dc.SelectStockPen(NULL_PEN);
        dc.SelectStockBrush(WHITE_BRUSH);
        CRect clr = WithDpi(area);
        dc.Rectangle(&clr);

        area.InflateRect(-10, -10);
      
        if (Capture)
        {
            auto stats = Capture->GetStats();
            stat = stats.Recording ? 1 : 0;

            // FPS graph    
            CRect graph(area.left, area.top, area.right, area.top + 62);
            PaintGraph(dc, WithDpi(graph), Vec3(0, 0.5, 0), "FPS", "%.2f", stats.Frames.Count(), stats.FPS, -1, [&](int i)
            {
                return stats.Frames[i].FPS;
            });
           
            while (stats.MaxBitrate < (maxRate-5000))
                maxRate = maxRate - 5000;
            while (stats.MaxBitrate > maxRate)
                maxRate = maxRate + 5000;

            // Bitrate graph
            graph.OffsetRect(0, 70);
            PaintGraph(dc, WithDpi(graph), Vec3(0.0, 0, 0.5), "Bit rate", "%.0f kbits/s", stats.Frames.Count(), maxRate, stats.AvgBitrate, [&](int i)
            {
                return stats.Frames[i].Bitrate;
            });

            // VU meter
            CRect vumeter(area.left, graph.bottom + 10, area.right, graph.bottom + 10 + 26);
            if (Config.CaptureAudio)
                PaintVU(dc, WithDpi(vumeter), stats);

            // info
            CRect line(area.left, vumeter.bottom + 20 + 40, area.right, area.bottom);
            int lw = 80;
            PaintText(dc, "Current file", stats.Filename, line, lw);

            PaintText(dc, "Resolution", String::PrintF("%dx%d @ %.4g fps", stats.SizeX, stats.SizeY, stats.FPS), line, lw);

            int s = (int)stats.Time;
            int m = s / 60; s = s % 60;
            int h = m / 60; m = m % 60;
            PaintText(dc, "Length", String::PrintF("%d:%02d:%02d",h,m,s), line, lw);

            PaintText(dc, "Bitrate", String::PrintF("avg %d, max %d kbits/s", (int)stats.AvgBitrate,(int)stats.MaxBitrate), line, lw);
        }

        int d10 = WithDpi(10);
        maindc.BitBlt(d10, d10, WithDpi(w), WithDpi(h), dc, 0, 0, SRCCOPY);
        EndPaint(&ps);
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

    // Inherited via CIdleHandler
    virtual BOOL OnIdle() override
    {
        if (stat != lastStat)
        {
            if (stat)
                statusText.SetWindowTextA("🔴 RECORDING");
            else
                statusText.SetWindowTextA("⏸ Ready");
            lastStat = stat;
        }
        Invalidate();

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

    DECLARE_FRAME_WND_CLASS_EX(NULL, IDR_MAINFRAME, 0, COLOR_MENU)

    BEGIN_UPDATE_UI_MAP(MainFrame)
    END_UPDATE_UI_MAP()

    BEGIN_MSG_MAP(MainFrame)
        MESSAGE_HANDLER(WM_SETCAPTURE, OnSetCapture)
        MESSAGE_HANDLER(WM_HOTKEY, OnHotKey)
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

        setupForm.Create(m_hWnd, cr, "", WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN, 0);
        statsForm.Create(m_hWnd, cr, "", WS_CHILD | WS_CLIPSIBLINGS | WS_CLIPCHILDREN, 0);

        return 0;
    }

    LRESULT OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
    {
        Delete(Capture);
        bHandled = TRUE;
        return 1;
    }

    LRESULT OnHotKey(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled)
    {
        switch (wParam)
        {
        case 1:
            return OnSetCapture(0, Capture ? 0 : 1, 0, bHandled);
        }
        return 0;
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

    wchar_t *videosPath = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Videos, 0, 0, &videosPath)))
        Config.Directory = videosPath;

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

    DPI = GetDpiForSystem();
    RECT winRect = { .left = CW_USEDEFAULT , .top = CW_USEDEFAULT, .right = CW_USEDEFAULT+WithDpi(420), .bottom = CW_USEDEFAULT+WithDpi(420) };

    if (wndMain.CreateEx(0, &winRect, WS_DLGFRAME|WS_SYSMENU|WS_MINIMIZEBOX) == NULL)
    {
        ATLTRACE(_T("Main window creation failed!\n"));
        return 0;
    }

    wndMain.ShowWindow(nCmdShow);

    auto hr = RegisterHotKey(wndMain, 1, MOD_WIN | MOD_NOREPEAT, VK_F9);

    int nRet = theLoop.Run();

    _Module.RemoveMessageLoop();

    return 0;
}

extern const char* AppName;

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/, LPSTR lpstrCmdLine, int nCmdShow)
{
    HRESULT hRes = ::CoInitializeEx(NULL, COINIT_MULTITHREADED);  
    ATLASSERT(SUCCEEDED(hRes));

    static char appName[2048];
    LoadString(ModuleHelper::GetResourceInstance(), IDR_MAINFRAME, appName, 2048);
    AppName = appName;

    // check for FFmpeg presence
    HMODULE dll = LoadLibrary("avcodec-58.dll");
    if (!dll)
    {
        char directory[MAX_PATH + 1];
        GetCurrentDirectory(MAX_PATH + 1, directory);
        Fatal("The FFmpeg DLLs are missing\n\nPlease download an FFmpeg 4.x build (64 bit, shared version), and place the DLLs from the bin folder into %s.", directory);
    }

    // check for CUDA presence
    dll = LoadLibrary("nvcuda.dll");
    if (!dll)
    {
        char directory[MAX_PATH + 1];
        GetCurrentDirectory(MAX_PATH + 1, directory);
        Fatal("CUDA must be installed - Capturinha currently only works on NVIDIA GPUs, sorry for that.");
    }

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