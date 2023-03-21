///////////////////////////////////////////////////////////////////////////////
// Name:        filelistdlg.cpp
// Author:      Blake Madden
// Copyright:   (c) 2005-2022 Blake Madden
// License:     3-Clause BSD license
// SPDX-License-Identifier: BSD-3-Clause
///////////////////////////////////////////////////////////////////////////////

#include "filelistdlg.h"

void FileListDlg::CreateControls()
    {
    wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);

    // infobar
    m_infoBar = new wxInfoBar(this);
    mainSizer->Add(m_infoBar, wxSizerFlags().Expand());

    wxSizerFlags szFlags(wxSizerFlags().Expand().Border(wxDirection::wxALL,
        wxSizerFlags::GetDefaultBorder()));

    wxBoxSizer* controlsSizer = new wxBoxSizer(wxHORIZONTAL);
    mainSizer->Add(controlsSizer, wxSizerFlags(1).Expand().
        Border(wxDirection::wxALL, wxSizerFlags::GetDefaultBorder()));

    // file list and toolbar
    wxBoxSizer* fileListSizer = new wxBoxSizer(wxVERTICAL);
    controlsSizer->Add(fileListSizer, wxSizerFlags(2).Expand().
        Border(wxDirection::wxALL, wxSizerFlags::GetDefaultBorder()));

    auto buttonsSizer = new wxGridSizer(4, wxSize(wxSizerFlags::GetDefaultBorder(),
        wxSizerFlags::GetDefaultBorder()));

    wxButton* button = new wxButton(this, wxID_OPEN, _("&Open File(s)..."));
    button->SetBitmap(wxArtProvider::GetBitmap(wxART_FILE_OPEN, wxART_BUTTON,
        FromDIP(wxSize(16, 16))));
    buttonsSizer->Add(button, wxSizerFlags().Align(wxALIGN_LEFT).Expand());

#ifdef __WXMSW__
    button = new wxButton(this, ID_FOLDER_OPEN, _("Open &Folder(s)..."));
    button->SetBitmap(wxArtProvider::GetBitmap(wxART_FOLDER_OPEN, wxART_BUTTON,
        FromDIP(wxSize(16, 16))));
    buttonsSizer->Add(button, wxSizerFlags().Align(wxALIGN_LEFT).Expand());
#endif

    button = new wxButton(this, wxID_DELETE, _("&Delete File(s)"));
    button->SetBitmap(wxArtProvider::GetBitmap(wxART_DELETE, wxART_BUTTON,
        FromDIP(wxSize(16, 16))));
    buttonsSizer->Add(button, wxSizerFlags().Align(wxALIGN_LEFT).Expand());

    button = new wxButton(this, wxID_REFRESH, _("&Refresh List"));
    button->SetBitmap(wxArtProvider::GetBitmap(wxART_REDO, wxART_BUTTON,
        FromDIP(wxSize(16, 16))));
    buttonsSizer->Add(button, wxSizerFlags().Align(wxALIGN_LEFT).Expand());

    fileListSizer->Add(buttonsSizer);

    m_listCtrl = new ListCtrlEx(this, wxID_ANY, wxDefaultPosition, FromDIP(wxSize(500, 400)),
                                wxLC_REPORT|wxLC_VIRTUAL);
    m_listCtrl->EnableGridLines();
    m_listCtrl->EnableAlternateRowColours(false);
    m_listCtrl->InsertColumn(0, _("File"));
    m_listCtrl->SetFileColumn(0);
    m_listCtrl->InsertColumn(1, _("Folder"));
    m_listCtrl->SetFolderColumn(1);
    m_listCtrl->InsertColumn(2, _("Group"));
    m_listCtrl->SetSortable(true);
    m_listCtrl->EnableFileDeletion();
    m_listCtrl->SetVirtualDataProvider(m_fileData);
    m_listCtrl->SetVirtualDataSize(1, 3);
    fileListSizer->Add(m_listCtrl, wxSizerFlags(1).Expand());

    // file information
    wxBoxSizer* fileInfoSizer = new wxBoxSizer(wxVERTICAL);
    m_thumbnail = new Wisteria::UI::Thumbnail(this, wxNullBitmap);
    fileInfoSizer->Add(m_thumbnail, wxSizerFlags(1).Expand().
        Border(wxDirection::wxALL, wxSizerFlags::GetDefaultBorder()));
    
    m_label = new wxStaticText(this, wxID_ANY, L"\n\n\n");
    fileInfoSizer->Add(m_label, szFlags);

    controlsSizer->Add(fileInfoSizer, wxSizerFlags(1).Expand().
        Border(wxDirection::wxALL, wxSizerFlags::GetDefaultBorder()));

    mainSizer->Add(CreateSeparatedButtonSizer(wxCLOSE), szFlags);

    SetSizerAndFit(mainSizer);

    BindEvents();
    }

//----------------------------------------
void FileListDlg::BindEvents()
    {
    // item selection
    Bind(wxEVT_LIST_ITEM_SELECTED,
        [this](wxListEvent& selected)
            {
            const wxString selectedFile = m_listCtrl->GetItemFilePath(selected.GetIndex());
            // file may have been deleted by the user while this dialog is open,
            // make sure it's actually still there.
            if (wxFile::Exists(selectedFile))
                {
                m_thumbnail->LoadImage(selectedFile);

                wxFileName fn(selectedFile);
                wxDateTime accessedDt, modifiedDt, createdDt;
                fn.GetTimes(&accessedDt, &modifiedDt, &createdDt);
                const wxString fileInfo =
                    wxString::Format(_("Name: %s\nSize: %s\nCreated: %s %s\nModified: %s %s"),
                    wxFileName(m_listCtrl->GetSelectedText()).GetFullName(),
                    fn.GetHumanReadableSize(),
                    createdDt.FormatDate(), createdDt.FormatTime(),
                    modifiedDt.FormatDate(), modifiedDt.FormatTime());
                m_label->SetLabel(fileInfo);
                }
            else
                { m_listCtrl->DeleteItem(selected.GetIndex()); }
            },
        wxID_ANY);

    // open files
    Bind(wxEVT_BUTTON,
        [this]([[maybe_unused]] wxCommandEvent&)
            {
            // make sure user isn't accidently opening too many files at once
            if (m_listCtrl->GetSelectedItemCount() > 10 &&
                wxMessageBox(wxString::Format(
                    _("Do you wish to open the selected %d files?"),
                    m_listCtrl->GetSelectedItemCount()),
                        _("Open Files"), wxYES_NO|wxICON_WARNING) == wxNO)
                { return; }
            long item{ wxNOT_FOUND };
            for (;;)
                {
                item = m_listCtrl->GetNextItem(item, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
                if (item == wxNOT_FOUND)
                    { break; }
                if (wxFile::Exists(m_listCtrl->GetItemFilePath(item)) )
                    { wxLaunchDefaultApplication(m_listCtrl->GetItemFilePath(item)); }
                }
            },
        wxID_OPEN);

#ifdef __WXMSW__
    // open folders
    Bind(wxEVT_BUTTON,
        [this]([[maybe_unused]] wxCommandEvent&)
            {
            // make sure user isn't accidently opening too many folders at once
            if (m_listCtrl->GetSelectedItemCount() > 10 &&
                wxMessageBox(
                    wxString::Format(_("Do you wish to open the selected %d folders?"),
                        m_listCtrl->GetSelectedItemCount()),
                    _("Open Files"), wxYES_NO|wxICON_WARNING) == wxNO)
                { return; }
            long item{ wxNOT_FOUND };
            for (;;)
                {
                item = m_listCtrl->GetNextItem(item, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
                if (item == wxNOT_FOUND)
                    { break; }
                wxFileName fn(m_listCtrl->GetItemFilePath(item));
                if (wxDir::Exists(fn.GetPath()))
                    { ShellExecute(NULL, wxT("open"), fn.GetPath(), NULL, NULL, SW_SHOWNORMAL); }
                }
            },
        ID_FOLDER_OPEN);
#endif

    // delete files
    Bind(wxEVT_BUTTON,
        [this]([[maybe_unused]] wxCommandEvent&)
            {
            if (m_promptOnDelete)
                {
                wxRichMessageDialog msg(this,
                    _("Do you wish to delete the selected file(s)?"),
                    _("Delete File"), wxYES_NO | wxICON_WARNING);
                msg.ShowCheckBox(_("Do not show this again"));
                if (msg.ShowModal() == wxID_NO)
                    { return; }
                // 'Yes' to delete, see if they don't want to see this prompt again
                else if (msg.IsCheckBoxChecked())
                    { m_promptOnDelete = false; }
                }
             m_listCtrl->DeleteSelectedItems();
            },
        wxID_DELETE);

    // file list refresh
    Bind(wxEVT_BUTTON,
        [this]([[maybe_unused]] wxCommandEvent&)
            {
            SetCursor(*wxHOURGLASS_CURSOR);
            wxWindowUpdateLocker lock(m_listCtrl);
            if (m_listCtrl->GetItemCount() > 0)
                {
                for (auto i = m_listCtrl->GetItemCount()-1; i >= 0; --i)
                    {
                    const wxString selectedFile = m_listCtrl->GetItemFilePath(i);
                    if (!wxFile::Exists(selectedFile))
                        { m_listCtrl->DeleteItem(i); }
                    }
                }
            SetCursor(wxNullCursor);
            },
        wxID_REFRESH);
    }
