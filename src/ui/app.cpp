#include "app.h"
#ifdef __WXMSW__
    #include <psapi.h>
    #include <debugapi.h>
#endif

using namespace Wisteria;

//----------------------------------------------------------
Wisteria::UI::BaseApp::BaseApp()
    {
    // call this to tell the library to call our OnFatalException()
    wxHandleFatalExceptions();
    }

//----------------------------------------------------------
void Wisteria::UI::BaseApp::OnFatalException()
    {
    GenerateReport(wxDebugReport::Context_Exception);
    }

//----------------------------------------------------------
bool Wisteria::UI::BaseApp::OnInit()
    {
    // prepare profile report (only if compiled with profiling)
    m_profileReportPath = wxString(wxStandardPaths::Get().GetTempDir() +
                                   wxFileName::GetPathSeparator() + GetAppName() +
                                   L" Profile.dat");
    SET_PROFILER_REPORT_PATH(m_profileReportPath.mb_str());
    DUMP_PROFILER_REPORT(); // flush out data in temp file from previous run

    // logs will be written to file now, delete the old logging system
    m_logBuffer = new LogFile;
    delete wxLog::SetActiveTarget(m_logBuffer);

    // log some system information
    wxDateTime buildDate;
    buildDate.ParseDate(__DATE__);
    wxLogMessage(L"Log File Location: %s", m_logBuffer->GetLogFilePath());
    wxLogMessage(L"%s %s (build %s)", GetAppName(), GetAppSubName(), buildDate.Format(L"%G.%m.%d"));
    wxLogMessage(L"App Location: %s", wxStandardPaths::Get().GetExecutablePath());
    wxLogMessage(wxVERSION_STRING);
    wxLogMessage(L"OS: %s", wxGetOsDescription());
#ifdef __WXGTK__
    wxLogMessage(L"Linux Info: %s", wxPlatformInfo::Get().GetLinuxDistributionInfo().Description);
    wxLogMessage(L"Desktop Environment: %s", wxPlatformInfo::Get().GetDesktopEnvironment());
#endif
    wxLogMessage(L"CPU Architecture: %s", wxGetCpuArchitectureName());
    wxLogMessage(L"CPU Count: %d", wxThread::GetCPUCount());
    if (wxGraphicsRenderer::GetDefaultRenderer())
        {
        wxLogMessage(L"Graphics Renderer: %s",
            wxGraphicsRenderer::GetDefaultRenderer()->GetName());
        }
#ifdef __WXMSW__
    if (wxGraphicsRenderer::GetDirect2DRenderer())
        { wxLogMessage(L"Direct2D Rendering Available: will attempt to use Direct2D"); }
    wxLogMessage(L"Available Physical Memory: %.02fGbs.",
        safe_divide<double>(wxGetFreeMemory().ToLong(), 1024*1024*1024));
#endif
    wxLogMessage(L"Default System Font: %s, %d pt.",
        wxSystemSettings::GetFont(wxSYS_DEFAULT_GUI_FONT).GetFaceName(),
        wxSystemSettings::GetFont(wxSYS_DEFAULT_GUI_FONT).GetPointSize());
    wxLogMessage(L"Screen Size: %d wide, %d tall",
        wxSystemSettings::GetMetric(wxSYS_SCREEN_X),
        wxSystemSettings::GetMetric(wxSYS_SCREEN_Y));
    wxLogMessage(L"System Theme: %s", wxSystemSettings::GetAppearance().GetName().empty() ?
        L"[unnamed]" : wxSystemSettings::GetAppearance().GetName());
    // subroutine to log the system colors
    const auto logSystemColor = [](const wxSystemColour color, const wxString& description)
        {
        if (const auto sysColor = wxSystemSettings::GetColour(color); sysColor.IsOk())
            {
            wxLogVerbose(L"%s: %s %s", description, sysColor.GetAsString(wxC2S_HTML_SYNTAX),
                wxTheColourDatabase->FindName(sysColor.GetRGB()).MakeCapitalized());
            }
        };

    logSystemColor(wxSYS_COLOUR_WINDOW, _DT(L"Window Color"));
    logSystemColor(wxSYS_COLOUR_MENU, _DT(L"Menu Color"));
    logSystemColor(wxSYS_COLOUR_WINDOWFRAME, _DT(L"Window Frame Color"));
    logSystemColor(wxSYS_COLOUR_BTNFACE, _DT(L"Dialog/Controls Color"));
    logSystemColor(wxSYS_COLOUR_HIGHLIGHT, _DT(L"Highlighted Item Color"));
    logSystemColor(wxSYS_COLOUR_WINDOWTEXT, _DT(L"Window Text Color"));
    logSystemColor(wxSYS_COLOUR_MENUTEXT, _DT(L"Menu Text Color"));
    logSystemColor(wxSYS_COLOUR_HIGHLIGHTTEXT, _DT(L"Highlighted Text Color"));
    logSystemColor(wxSYS_COLOUR_GRAYTEXT, _DT(L"Grayed Text Color"));
    logSystemColor(wxSYS_COLOUR_HOTLIGHT, _DT(L"Hyperlink Color"));

    // fix color mapping on Windows
    wxSystemOptions::SetOption(_DT(L"msw.remap"), 0);

    // set the locale (for number formatting, etc.) and load any translations
    wxUILocale::UseDefault();

    wxLogMessage(L"System Language: %s", wxUILocale::GetCurrent().GetName());

    wxInitAllImageHandlers();
    wxPropertyGrid::RegisterAdditionalEditors();
    wxFileSystem::AddHandler(new wxZipFSHandler);
    wxFileSystem::AddHandler(new wxMemoryFSHandler);

    //load the XRC handlers
    wxXmlResource::Get()->AddHandler(new wxBitmapXmlHandler);
    wxXmlResource::Get()->AddHandler(new wxIconXmlHandler);
    wxXmlResource::Get()->AddHandler(new wxMenuXmlHandler);
    wxXmlResource::Get()->AddHandler(new wxMenuBarXmlHandler);

    //Create the document manager
    SetDocManager(new Wisteria::UI::DocManager);

    wxDialog::EnableLayoutAdaptation(true);

    return true;
    }

//----------------------------------------------------------
int Wisteria::UI::BaseApp::OnExit()
    {
    wxLogDebug(__WXFUNCTION__);
    SaveFileHistoryMenu();
    wxDELETE(m_docManager);

#ifdef __WXMSW__
    #if wxDEBUG_LEVEL >= 2
        // dump max memory usage
        // https://docs.microsoft.com/en-us/windows/win32/psapi/collecting-memory-usage-information-for-a-process?redirectedfrom=MSDN
        PROCESS_MEMORY_COUNTERS memCounter;
        ::ZeroMemory(&memCounter, sizeof(PROCESS_MEMORY_COUNTERS));
        if (::GetProcessMemoryInfo(::GetCurrentProcess(), &memCounter, sizeof(memCounter)))
            {
            const wxString memMsg = wxString::Format(L"Peak Memory Usage: %.02fGbs.",
                safe_divide<double>(memCounter.PeakWorkingSetSize, 1024*1024*1024));
            wxLogDebug(memMsg);
            OutputDebugString(memMsg);
            }
    #endif
#endif
    return wxApp::OnExit();
    }

//----------------------------------------------------------
void Wisteria::UI::BaseApp::GenerateReport(wxDebugReport::Context ctx)
    {
    wxDebugReportCompress* report = new wxDebugReportCompress;

    // add all standard files: currently this means just a minidump and an
    // XML file with system info and stack trace
    report->AddAll(ctx);

    const wxDateTime dt = wxDateTime::Now();
    report->AddText(L"Timestamp.log", dt.FormatISODate() + wxT(' ') +
                    dt.FormatISOTime(), _(L"Timestamp of this report"));

    report->AddFile(m_logBuffer->GetLogFilePath(), _(L"Log Report"));
    wxString settingsPath = wxStandardPaths::Get().GetUserDataDir() +
                            wxFileName::GetPathSeparator() + L"Settings.xml";
    if (!wxFile::Exists(settingsPath))
        {
        settingsPath = wxStandardPaths::Get().GetUserDataDir() +
                       wxFileName::GetPathSeparator() + GetAppName() +
                       wxFileName::GetPathSeparator() + L"Settings.xml"; }
    report->AddFile(settingsPath, _(L"Settings File"));

    if (wxDebugReportPreviewStd().Show(*report) )
        {
        report->Process();
        wxString newReportPath = wxStandardPaths::Get().GetDocumentsDir() +
                                 wxFileName::GetPathSeparator() + GetAppName() +
                                 L" Crash Report.zip";
        wxCopyFile(report->GetCompressedFileName(), newReportPath, true);
        wxMessageBox(wxString::Format(_(L"An error report has been saved to:\n\"%s\".\n\n"
            "Please email this file to %s to have this issue reviewed. "
            "Thank you for your patience."), newReportPath, m_supportEmail), 
            _(L"Error Report"), wxOK|wxICON_INFORMATION);
    #ifdef __WXMSW__
        ShellExecute(NULL, _DT(L"open"), wxStandardPaths::Get().GetDocumentsDir(),
                     NULL, NULL, SW_SHOWNORMAL);
    #endif
        }

    delete report;
    }

//----------------------------------------------------------
void Wisteria::UI::BaseApp::SaveFileHistoryMenu()
    {
    wxConfig config(GetAppName() + _DT(L"MRU"), GetVendorName());
    config.SetPath(_DT(L"Recent File List", DTExplanation::SystemEntry, L"This goes in the registry"));
    GetDocManager()->FileHistorySave(config);
    }

//----------------------------------------------------------
void Wisteria::UI::BaseApp::LoadFileHistoryMenu()
    {
    if (GetMainFrame()->GetMenuBar() && GetMainFrame()->GetMenuBar()->GetMenuCount() )
        {
        GetDocManager()->FileHistoryUseMenu(GetMainFrame()->GetMenuBar()->GetMenu(0));
        }
    //Load the file history
    wxConfig config(GetAppName() + _DT(L"MRU"), GetVendorName());
    config.SetPath(_DT(L"Recent File List", DTExplanation::SystemEntry));
    GetDocManager()->FileHistoryLoad(config);
    }

//----------------------------------------------------------
void Wisteria::UI::BaseApp::ClearFileHistoryMenu()
    {
    while (GetDocManager()->GetHistoryFilesCount())
        { GetDocManager()->GetFileHistory()->RemoveFileFromHistory(0); }
    }