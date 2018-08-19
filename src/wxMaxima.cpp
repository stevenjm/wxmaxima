// -*- mode: c++; c-file-style: "linux"; c-basic-offset: 2; indent-tabs-mode: nil -*-
//
//  Copyright (C) 2004-2015 Andrej Vodopivec <andrej.vodopivec@gmail.com>
//            (C) 2008-2009 Ziga Lenarcic <zigalenarcic@users.sourceforge.net>
//            (C) 2011-2011 cw.ahbong <cw.ahbong@gmail.com>
//            (C) 2012-2013 Doug Ilijev <doug.ilijev@gmail.com>
//            (C) 2014-2018 Gunter Königsmann <wxMaxima@physikbuch.de>
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 2 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
//  SPDX-License-Identifier: GPL-2.0+

/*!\file
  This file defines the contents of the class wxMaxima that contains most of the program's logic.

  The worksheet is defined in the class MathCtrl instead and
  everything surrounding it in wxMaximaFrame.
 */

#include <wx/notifmsg.h>

#if defined __WXMSW__
#include <wchar.h>
#endif
#include "wxMaxima.h"
#include "ImgCell.h"
#include "DrawWiz.h"
#include "SubstituteWiz.h"
#include "IntegrateWiz.h"
#include "LimitWiz.h"
#include "Plot2dWiz.h"
#include "SeriesWiz.h"
#include "SumWiz.h"
#include "Plot3dWiz.h"
#include "ConfigDialogue.h"
#include "Gen1Wiz.h"
#include "Gen2Wiz.h"
#include "Gen3Wiz.h"
#include "Gen4Wiz.h"
#include "Gen5Wiz.h"
#include "BC2Wiz.h"
#include "MatWiz.h"
#include "SystemWiz.h"
#include "MathPrintout.h"
#include "TipOfTheDay.h"
#include "EditorCell.h"
#include "SlideShowCell.h"
#include "PlotFormatWiz.h"
#include "ActualValuesStorageWiz.h"
#include "MaxSizeChooser.h"
#include "ListSortWiz.h"
#include "wxMaximaIcon.h"

#include <wx/colordlg.h>
#include <wx/clipbrd.h>
#include <wx/filedlg.h>
#include <wx/utils.h>
#include <wx/uri.h>
#include <wx/msgdlg.h>
#include <wx/textfile.h>
#include <wx/tokenzr.h>
#include <wx/mimetype.h>
#include <wx/dynlib.h>
#include <wx/dir.h>
#include <wx/filename.h>
#include <wx/artprov.h>
#include <wx/aboutdlg.h>
#include <wx/utils.h>
#include <wx/mstream.h>

#include <wx/zipstrm.h>
#include <wx/wfstream.h>
#include <wx/txtstrm.h>
#include <wx/sckstrm.h>
#include <wx/fs_mem.h>
#include <wx/persist/toplevel.h>

#include <wx/url.h>
#include <wx/sstream.h>
#include <list>

#if defined __WXMAC__
#define MACPREFIX "wxMaxima.app/Contents/Resources/"
#endif

void wxMaxima::ConfigChanged()
{
  wxConfig *config = (wxConfig *) wxConfig::Get();
  int showLength = 0;

  config->Read(wxT("showLength"), &showLength);

  switch (showLength)
  {
    case 0:
      m_maxOutputCellsPerCommand = 600;
      break;
    case 1:
      m_maxOutputCellsPerCommand = 1200;
      break;
    case 2:
      m_maxOutputCellsPerCommand = 5000;
      break;
    case 3:
      m_maxOutputCellsPerCommand = -1;
      break;
  }
  m_console->RecalculateForce();
  m_console->RequestRedraw();

    bool wxcd = true;
#if defined (__WXMSW__)
  config->Read(wxT("wxcd"),&wxcd);
#endif

  if (wxcd)
  {
    m_configCommands += wxT(":lisp-quiet (setq $wxchangedir t)\n");
  }
  else
  {
    m_configCommands += wxT(":lisp-quiet (setq $wxchangedir nil)\n");
  }

#if defined (__WXMAC__)
  bool usepngCairo = false;
#else
  bool usepngCairo=true;
#endif
  config->Read(wxT("usepngCairo"), &usepngCairo);
  if (usepngCairo)
    m_configCommands += wxT(":lisp-quiet (setq $wxplot_pngcairo t)\n");
  else
    m_configCommands += wxT(":lisp-quiet (setq $wxplot_pngcairo nil)\n");

  m_configCommands += wxT(":lisp-quiet (setq $wxsubscripts ") +
             m_console->m_configuration->GetAutosubscript_string() +
             wxT(")\n");

  // A few variables for additional debug info in wxbuild_info();
  m_configCommands += wxString::Format(wxT(":lisp-quiet (setq wxUserConfDir \"%s\")\n"),
                                       EscapeForLisp(m_console->m_configuration->m_dirStructure.UserConfDir()));
  m_configCommands += wxString::Format(wxT(":lisp-quiet (setq wxHelpDir \"%s\")\n"),
                              EscapeForLisp(m_console->m_configuration->m_dirStructure.HelpDir()));
  m_configCommands += wxString::Format(wxT(":lisp-quiet (setq wxMaximaLispLocation \"%s\")\n"),
                              EscapeForLisp(m_console->m_configuration->m_dirStructure.MaximaLispLocation()));

  int defaultPlotWidth = 600;
  config->Read(wxT("defaultPlotWidth"), &defaultPlotWidth);
  int defaultPlotHeight = 400;
  config->Read(wxT("defaultPlotHeight"), &defaultPlotHeight);
  m_configCommands += wxString::Format(wxT(":lisp-quiet (setq $wxplot_size '((mlist simp) %i %i))\n"),
                              defaultPlotWidth,
                              defaultPlotHeight);

  if (m_console->m_currentFile != wxEmptyString)
  {
    wxString filename(m_console->m_currentFile);

    SetCWD(filename);
  }
}

wxMaxima *MyApp::m_frame;

wxMaxima::wxMaxima(wxWindow *parent, int id, const wxString title, const wxString configFile,
                   const wxPoint pos, const wxSize size) :
  wxMaximaFrame(parent, id, title, configFile, pos, size)
{
  m_gnuplotProcess = NULL;
  m_openInitialFileError = false;
  m_nestedLoadCommands = 0;
  m_maximaJiffies_old = 0;
  m_cpuTotalJiffies_old = 0;

  m_updateControls = true;
  m_commandIndex = -1;
  m_isActive = true;
  wxASSERT(m_outputPromptRegEx.Compile(wxT("<lbl>.*</lbl>")));
  wxConfig *config = (wxConfig *) wxConfig::Get();
  m_unsuccessfulConnectionAttempts = 0;
  m_outputCellsFromCurrentCommand = 0;
  m_CWD = wxEmptyString;
  m_port = 4010;
  m_pid = -1;
  wxASSERT(m_gnuplotErrorRegex.Compile(wxT("\".*\\.gnuplot\", line [0-9][0-9]*: ")));
  m_hasEvaluatedCells = false;
  m_process = NULL;
  m_maximaStdout = NULL;
  m_maximaStderr = NULL;
  m_ready = false;
  m_inLispMode = false;
  m_first = true;
  m_isRunning = false;
  m_dispReadOut = false;
  m_promptPrefix = wxT("<PROMPT-P/>");
  m_promptSuffix = wxT("<PROMPT-S/>");

  m_symbolsPrefix = wxT("<wxxml-symbols>");
  m_symbolsSuffix = wxT("</wxxml-symbols>");
  m_variablesPrefix = wxT("<variables>");
  m_variablesSuffix = wxT("</variables>");
  m_firstPrompt = wxT("(%i1) ");

  m_client = NULL;
  m_server = NULL;

  config->Read(wxT("lastPath"), &m_lastPath);
  m_lastPrompt = wxEmptyString;

  m_closing = false;
  m_openFile = wxEmptyString;
  m_fileSaved = true;
  m_printData = NULL;

  m_htmlHelpInitialized = false;
  m_chmhelpFile = wxEmptyString;

  m_isConnected = false;
  m_isRunning = false;

  wxFileSystem::AddHandler(new wxMemoryFSHandler); // for saving wxmx

  UpdateRecentDocuments();

  m_console->m_findDialog = NULL;
  m_oldFindString = wxEmptyString;
  m_oldFindFlags = 0;
  m_console->m_currentFile = wxEmptyString;
  int findFlags = wxFR_DOWN | wxFR_MATCHCASE;
  wxConfig::Get()->Read(wxT("findFlags"), &findFlags);
  m_findData.SetFlags(findFlags);
  m_console->SetFocus();
  m_console->m_keyboardInactiveTimer.SetOwner(this, KEYBOARD_INACTIVITY_TIMER_ID);
  m_maximaStdoutPollTimer.SetOwner(this, MAXIMA_STDOUT_POLL_ID);

  m_autoSaveTimer.SetOwner(this, AUTO_SAVE_TIMER_ID);

#if wxUSE_DRAG_AND_DROP
  m_console->SetDropTarget(new MyDropTarget(this));
#endif

  StatusMaximaBusy(disconnected);

  /// RegEx for function definitions
  wxASSERT(m_funRegEx.Compile(wxT("^ *([[:alnum:]%_]+) *\\(([[:alnum:]%_,[[.].] ]*)\\) *:=")));
  // RegEx for variable definitions
  wxASSERT(m_varRegEx.Compile(wxT("^ *([[:alnum:]%_]+) *:")));
  // RegEx for blank statement removal
  wxASSERT(m_blankStatementRegEx.Compile(wxT("(^;)|((^|;)(((\\/\\*.*\\*\\/)?([[:space:]]*))+;)+)")));

  m_statusBar->GetNetworkStatusElement()->Connect(wxEVT_LEFT_DCLICK,
                                                  wxCommandEventHandler(wxMaxima::NetworkDClick),
                                                  NULL, this);
  m_clientStream = NULL;
  m_clientTextStream = NULL;

}

wxMaxima::~wxMaxima()
{
  KillMaxima();
  wxDELETE(m_printData);
  m_printData = NULL;
}


#if wxUSE_DRAG_AND_DROP

bool MyDropTarget::OnDropFiles(wxCoord WXUNUSED(x), wxCoord WXUNUSED(y), const wxArrayString &files)
{

  if (files.GetCount() != 1)
    return true;

  if (wxGetKeyState(WXK_SHIFT))
  {
    m_wxmax->m_console->InsertText(files[0]);
    return true;
  }

  if (files[0].Right(4) == wxT(".wxm") ||
      files[0].Right(5) == wxT(".wxmx"))
  {
    if (m_wxmax->m_console->GetTree() != NULL &&
        !m_wxmax->DocumentSaved())
    {
      int close = m_wxmax->SaveDocumentP();

      if (close == wxID_CANCEL)
        return false;

      if (close == wxID_YES)
      {
        if (!m_wxmax->SaveFile())
          return false;
      }
    }

    m_wxmax->OpenFile(files[0]);
    return true;
  }

  if (files[0].Right(4) == wxT(".png") ||
      files[0].Right(5) == wxT(".jpeg") ||
      files[0].Right(4) == wxT(".jpg"))
  {
    m_wxmax->LoadImage(files[0]);
    return true;
  }

  m_wxmax->m_console->InsertText(files[0]);
  return true;
}

#endif

//!--------------------------------------------------------------------------------
//  Startup
//--------------------------------------------------------------------------------
void wxMaxima::InitSession()
{
  bool server = false;
  int defaultPort = 4010;

  wxConfig::Get()->Read(wxT("defaultPort"), &defaultPort);
  m_port = defaultPort;

  while (!(server = StartServer()))
  {
    m_port++;
    if (m_port > defaultPort + 50)
    {
      wxMessageBox(_("wxMaxima could not start the server.\n\n"
                             "Please check you have network support\n"
                             "enabled and try again!"),
                   _("Fatal error"),
                   wxOK | wxICON_ERROR);
      break;
    }
  }

  if (!server)
    SetStatusText(_("Starting server failed"));
  else if (!StartMaxima())
    SetStatusText(_("Starting Maxima process failed"));

  Refresh();
  m_console->SetFocus();
  if (m_console->m_configuration->AutoSaveInterval() > 0)
    m_autoSaveTimer.StartOnce(m_console->m_configuration->AutoSaveInterval());
}

void wxMaxima::FirstOutput()
{
  m_lastPrompt = wxT("(%i1) ");

  /// READ FUNCTIONS FOR AUTOCOMPLETION
  m_console->LoadSymbols();

  m_console->SetFocus();
}

///--------------------------------------------------------------------------------
///  Appending stuff to output
///--------------------------------------------------------------------------------

/*! ConsoleAppend adds a new line s of type to the console window.
 *
 * It will call
 * DoConsoleAppend if s is in xml and DoRawCosoleAppend if s is not in xml.
 */
TextCell *wxMaxima::ConsoleAppend(wxString s, int type, wxString userLabel)
{
  TextCell *lastLine = NULL;
  // If we want to append an error message to the worksheet and there is no cell
  // that can contain it we need to create such a cell.
  if (m_console->GetTree() == NULL)
    m_console->InsertGroupCells(
            new GroupCell(&(m_console->m_configuration), GC_TYPE_CODE, &m_console->m_cellPointers, wxEmptyString));

  m_dispReadOut = false;
  s.Replace(m_promptSuffix, wxEmptyString);

  // If the string we have to append is empty we return immediately.
  wxString t(s);
  t.Trim();
  t.Trim(false);
  if (t.Length() == 0)
  {
    return NULL;
  }

  if (m_maxOutputCellsPerCommand > 0)
  {
    // If we already have output more lines than we are allowed to we a inform the user
    // about this and return.
    if (m_outputCellsFromCurrentCommand++ == m_maxOutputCellsPerCommand)
    {
      DoRawConsoleAppend(
              _("... [suppressed additional lines since the output is longer than allowed in the configuration] "),
              MC_TYPE_ERROR);
      return NULL;
    };


    // If we already have output more lines than we are allowed to and we already
    // have informed the user about this we return immediately
    if (m_outputCellsFromCurrentCommand > m_maxOutputCellsPerCommand)
      return NULL;
  }

  if ((type != MC_TYPE_ERROR) && (type != MC_TYPE_WARNING))
    StatusMaximaBusy(parsing);

  if (type == MC_TYPE_DEFAULT)
  {
    // Show a busy cursor whilst interpreting and layouting potentially long data from maxima.
    wxBusyCursor crs;

    while (s.Length() > 0)
    {
      int start = s.Find(wxT("<mth"));

      if (start == wxNOT_FOUND)
      {
        t = s;
        t.Trim();
        t.Trim(false);
        if (t.Length())
          lastLine = DoRawConsoleAppend(s, MC_TYPE_DEFAULT);
        s = wxEmptyString;
      }
      else
      {

        // If the string does begin with a <mth> we add the
        // part of the string that precedes the <mth> to the console
        // first.
        wxString pre = s.SubString(0, start - 1);
        wxString pre1(pre);
        pre1.Trim();
        pre1.Trim(false);
        if (pre1.Length())
          DoRawConsoleAppend(pre, MC_TYPE_DEFAULT);

        // If the math tag ends inside this string we add the whole tag.
        int end = s.Find(wxT("</mth>"));
        if (end == wxNOT_FOUND)
          end = s.Length();
        else
          end += 5;
        wxString rest = s.SubString(start, end);

        DoConsoleAppend(wxT("<span>") + rest +
                        wxT("</span>"), type, false, true, userLabel);
        s = s.SubString(end + 1, s.Length());
      }
//      wxSafeYield();
    }
  }

  else if (type == MC_TYPE_PROMPT)
  {
    m_lastPrompt = s;

    if (s.StartsWith(wxT("MAXIMA> ")))
    {
      s = s.Right(8);
    }
    else
      s = s + wxT(" ");

    DoConsoleAppend(wxT("<span>") + s + wxT("</span>"), type, true, true, userLabel);
  }

  else if (type == MC_TYPE_ERROR)
  {
    lastLine = DoRawConsoleAppend(s, MC_TYPE_ERROR);
    GroupCell *tmp = m_console->GetWorkingGroup(true);

    if (tmp == NULL)
    {
    if (m_console->GetActiveCell())
      tmp = dynamic_cast<GroupCell *>(m_console->GetActiveCell()->GetGroup());
    }

    if(tmp != NULL)
    {
      m_console->m_cellPointers.m_errorList.Add(tmp);
      tmp->GetEditable()->SetErrorIndex(m_commandIndex - 1);
    }
  }
  else if (type == MC_TYPE_WARNING)
  {
    lastLine = DoRawConsoleAppend(s, MC_TYPE_WARNING);
  }
  else
    DoConsoleAppend(wxT("<span>") + s + wxT("</span>"), type, false);

  return lastLine;
}

void wxMaxima::DoConsoleAppend(wxString s, int type, bool newLine,
                               bool bigSkip, wxString userLabel)
{
  MathCell *cell;

  if (s.IsEmpty())
    return;

  s.Replace(wxT("\n"), wxT(" "), true);

  MathParser mParser(&m_console->m_configuration, &m_console->m_cellPointers);
  mParser.SetUserLabel(userLabel);
  cell = mParser.ParseLine(s, type);

  wxASSERT_MSG(cell != NULL, _("There was an error in generated XML!\n\n"
                                       "Please report this as a bug."));
  if (cell == NULL)
  {
    return;
  }

  cell->SetSkip(bigSkip);
  m_console->InsertLine(cell, newLine || cell->BreakLineHere());
}

TextCell *wxMaxima::DoRawConsoleAppend(wxString s, int type)
{

  TextCell *cell = NULL;
  // If we want to append an error message to the worksheet and there is no cell
  // that can contain it we need to create such a cell.
  if (m_console->GetTree() == NULL)
    m_console->InsertGroupCells(
            new GroupCell(&(m_console->m_configuration), GC_TYPE_CODE, &m_console->m_cellPointers, wxEmptyString));

  if (s.IsEmpty())
    return NULL;

  bool scrollToCaret = (!m_console->FollowEvaluation() && m_console->CaretVisibleIs());

  if (type == MC_TYPE_MAIN_PROMPT)
  {
    cell = new TextCell(m_console->GetTree(), &(m_console->m_configuration), &m_console->m_cellPointers, s);
    cell->SetType(type);
    m_console->InsertLine(cell, true);
  }

  else
  {

    TextCell *incompleteTextCell =
      dynamic_cast<TextCell *>(m_console->m_cellPointers.m_currentTextCell);

    if(incompleteTextCell != NULL)
    {
      int pos = s.Find("\n");
      wxString newVal = incompleteTextCell->GetValue();
      if(pos != wxNOT_FOUND)
      {
        newVal += s.Left(pos);
        s = s.Right(s.Length() - pos - 1);
      }
      else
      {
        newVal += s;
        s = wxEmptyString;
      }

      incompleteTextCell->SetValue(newVal);
      if(s == wxEmptyString)
      {
        dynamic_cast<GroupCell *>(incompleteTextCell->GetGroup())->ResetSize();
        dynamic_cast<GroupCell *>(incompleteTextCell->GetGroup())->Recalculate();
        return incompleteTextCell;
      }
    }

    wxStringTokenizer tokens(s, wxT("\n"));
    int count = 0;
    MathCell *tmp = NULL, *lst = NULL;
    while (tokens.HasMoreTokens())
    {
      cell = new TextCell(m_console->GetTree(), &(m_console->m_configuration),
                           &m_console->m_cellPointers,
                           tokens.GetNextToken());

      cell->SetType(type);

      if (tokens.HasMoreTokens())
        cell->SetSkip(false);

      if (lst == NULL)
        tmp = lst = cell;
      else
      {
        lst->AppendCell(cell);
        cell->ForceBreakLine(true);
        lst = cell;
      }

      count++;
    }
    m_console->InsertLine(tmp, true);
  }

  if (scrollToCaret) m_console->ScrollToCaret();
  return cell;
}

/*! Remove empty statements
 *
 * We need to remove any statement which would be considered empty
 * and thus cause an error. Comments within non-empty expressions seem to
 * be fine.
 *
 * What we need to remove is any statements which are any amount of whitespace
 * and any amount of comments, in any order, ended by a semicolon,
 * and nothing else.
 *
 * The most that should be left over is a single empty statement, ";".
 *
 * @param s The command string from which to remove comment expressions.
 */
void wxMaxima::StripComments(wxString &s)
{
  if (s.StartsWith(wxT(":lisp\n")) || s.StartsWith(wxT(":lisp ")))
  {
    int start = 0;
    int commentStart = 0;
    while ((commentStart = s.find(wxT(';'), start)) != wxNOT_FOUND)
    {
      int commentEnd = s.find(wxT('\n'), commentStart);
      if (commentEnd == wxNOT_FOUND)
        commentEnd = s.length();
      s = s.SubString(0, commentStart - 1) + s.SubString(commentEnd, s.length());
    }
  }
  else
    m_blankStatementRegEx.Replace(&s, wxT(";"));
}

void wxMaxima::SendMaxima(wxString s, bool addToHistory, bool checkForUnmatchedStrings)
{
  if (m_xmlInspector)
    m_xmlInspector->Add_ToMaxima(s);

  // Normally we catch parenthesis errors before adding cells to the
  // evaluation queue. But if the error is introduced only after the
  // cell is placed in the evaluation queue we need to catch it here.
  int index;
  wxString parenthesisError = GetUnmatchedParenthesisState(s,index);
  if ((parenthesisError == wxEmptyString) || (!checkForUnmatchedStrings))
  {
    s = m_console->UnicodeToMaxima(s);

    // If there is no working group and we still are trying to send something
    // we are trying to change maxima's settings from the background and might never
    // get an answer that changes the status again.
    if (m_console->GetWorkingGroup())
      StatusMaximaBusy(calculating);
    else
      StatusMaximaBusy(waiting);

    m_dispReadOut = false;

    /// Add this command to History
    if (addToHistory)
      AddToHistory(s);

    StripComments(s);

    if (s.StartsWith(wxT(":lisp ")) || s.StartsWith(wxT(":lisp\n")))
      s.Replace(wxT("\n"), wxT(" "));

    s.Trim(true);
    s.Append(wxT("\n"));

    /// Check for function/variable definitions
    wxStringTokenizer commands(s, wxT(";$"));
    while (commands.HasMoreTokens())
    {
      wxString line = commands.GetNextToken();
      if (m_varRegEx.Matches(line))
        m_console->AddSymbol(m_varRegEx.GetMatch(line, 1));

      if (m_funRegEx.Matches(line))
      {
        wxString funName = m_funRegEx.GetMatch(line, 1);
        m_console->AddSymbol(funName);

        /// Create a template from the input
        wxString args = m_funRegEx.GetMatch(line, 2);
        wxStringTokenizer argTokens(args, wxT(","));
        funName << wxT("(");
        int count = 0;
        while (argTokens.HasMoreTokens())
        {
          if (count > 0)
            funName << wxT(",");
          wxString a = argTokens.GetNextToken().Trim().Trim(false);
          if (a != wxEmptyString)
          {
            if (a[0] == '[')
              funName << wxT("[<") << a.SubString(1, a.Length() - 2) << wxT(">]");
            else
              funName << wxT("<") << a << wxT(">");
            count++;
          }
        }
        funName << wxT(")");
        m_console->AddSymbol(funName, AutoComplete::tmplte);
      }
    }

    if (m_client)
    {
      wxScopedCharBuffer const data_raw = s.utf8_str();
      m_client->Write(data_raw.data(), data_raw.length());
      m_statusBar->NetworkStatus(StatusBar::transmit);
    }
  }
  else
  {
    ConsoleAppend(_("Refusing to send cell to maxima: ") +
                  parenthesisError + wxT("\n"),
                  MC_TYPE_ERROR);
    m_console->m_cellPointers.SetWorkingGroup(NULL);
  }
  if(!m_maximaStdoutPollTimer.IsRunning())
      m_statusBar->SetMaximaCPUPercentage(-1);
  m_maximaStdoutPollTimer.StartOnce(MAXIMAPOLLMSECS);
}

///--------------------------------------------------------------------------------
///  Socket stuff
///--------------------------------------------------------------------------------

void wxMaxima::ClientEvent(wxSocketEvent &event)
{
  switch (event.GetSocketEvent())
  {

  case wxSOCKET_INPUT:
  {
    // Read out stderr: We will do that in the background on a regular basis, anyway.
    // But if we do it manually now, too, the probability that things are presented
    // to the user in chronological order increases a bit.
    ReadStdErr();

    // It is theoretically possible that the client has exited after sending us
    // data and before we had been able to process it.
    if (m_client == NULL)
      return;

    if(!m_client->IsData())
      return;

    m_statusBar->NetworkStatus(StatusBar::receive);

    // The memory we store new chars we receive from maxima in
    wxString newChars;

    // Read all new lines of text we received.
    wxString line;
    bool eof = false;
    while(m_client->IsData())
      {
        line = m_clientTextStream->ReadLine();
        newChars += line;
        if ((m_client->IsData()) && (!(eof = m_clientStream->Eof())))
          newChars += wxT("\n");
      }

    if(newChars == wxEmptyString)
      return;

    if (IsPaneDisplayed(menu_pane_xmlInspector))
      m_xmlInspector->Add_FromMaxima(newChars);

    // This way we can avoid searching the whole string for a
    // ending tag if we have received only a few bytes of the
    // data between 2 tags
    if(m_currentOutput != wxEmptyString)
      m_currentOutputEnd = m_currentOutput.Right(MIN(30,m_currentOutput.Length())) + newChars;
    else
      m_currentOutputEnd = wxEmptyString;

    m_currentOutput += newChars;

    if (!m_dispReadOut &&
        (m_currentOutput != wxT("\n")) &&
        (m_currentOutput != wxT("<wxxml-symbols></wxxml-symbols>")))
    {
      StatusMaximaBusy(transferring);
      m_dispReadOut = true;
    }

    size_t length_old = -1;

    while (length_old != m_currentOutput.Length())
    {
      if (m_currentOutput.StartsWith("\n<"))
        m_currentOutput = m_currentOutput.Right(m_currentOutput.Length() - 1);

      length_old = m_currentOutput.Length();


      // First read the prompt that tells us that maxima awaits the next command:
      // If that is the case ReadPrompt() sends the next command to maxima and
      // maxima can work while we interpret its output.
      GroupCell *oldActiveCell = m_console->GetWorkingGroup();
      ReadPrompt(m_currentOutput);
      GroupCell *newActiveCell = m_console->GetWorkingGroup();

      // Temporarily switch to the WorkingGroup the output we don't have interpreted yet
      // was for
      if(newActiveCell != oldActiveCell)
        m_console->m_cellPointers.SetWorkingGroup(oldActiveCell);
      // Handle the <mth> tag that contains math output and sometimes text.
      ReadMath(m_currentOutput);

      // The following function calls each extract and remove one type of XML tag
      // information from the beginning of the data string we got - but only do so
      // after the closing tag has been transferred, as well.
      ReadLoadSymbols(m_currentOutput);

      // Let's see if maxima informs us about the values of variables
      ReadVariables(m_currentOutput);

      // Handle the XML tag that contains Status bar updates
      ReadStatusBar(m_currentOutput);

      // Handle text that isn't wrapped in a known tag
      if (!m_first)
        // Handle text that isn't XML output: Mostly Error messages or warnings.
        ReadMiscText(m_currentOutput);
      else
        // This function determines the port maxima is running on from  the text
        // maxima outputs at startup. This piece of text is afterwards discarded.
        ReadFirstPrompt(m_currentOutput);

      // Switch to the WorkingGroup the next bunch of data is for.
      if(newActiveCell != oldActiveCell)
        m_console->m_cellPointers.SetWorkingGroup(newActiveCell);
    }
    break;
    }
  default:
    break;
  }
}

/*!
 * ServerEvent is triggered when maxima connects to the socket server.
 */
void wxMaxima::ServerEvent(wxSocketEvent &event)
{
  switch (event.GetSocketEvent())
  {

    case wxSOCKET_CONNECTION :
    {
      wxLogMessage(_("Connected."));
      if (m_isConnected)
      {
        wxSocketBase *tmp = m_server->Accept(false);
        tmp->Close();
        return;
      }
      if(m_process == NULL)
        return;

      m_statusBar->NetworkStatus(StatusBar::idle);
      m_console->QuestionAnswered();
      m_currentOutput = wxEmptyString;
      m_isConnected = true;
      m_client = m_server->Accept(false);
      m_client->SetEventHandler(*this, socket_client_id);
      m_client->SetNotify(wxSOCKET_INPUT_FLAG);
       m_client->SetFlags(wxSOCKET_NOWAIT);
      m_client->SetTimeout(2);
      m_client->Notify(true);
      m_clientStream = new wxSocketInputStream(*m_client);
      m_clientTextStream = new wxTextInputStream(*m_clientStream, wxT('\n'),
                                                 wxConvAuto(wxFONTENCODING_UTF8));
      SetupVariables();

      // Start the evaluation. If the evaluation queue isn't empty, that is.
      TryEvaluateNextInQueue();
    }
    break;

  default:
      break;
  }
}

bool wxMaxima::StartServer()
{
  m_newStatusText = wxString::Format(_("Starting server on port %d"), m_port);

  wxIPV4address addr;

#ifndef __WXMAC__
  addr.LocalHost();
#else
  addr.AnyAddress();
#endif

  addr.Service(m_port);

  m_server = new wxSocketServer(addr);
  if (!m_server->Ok())
  {
    m_server->Destroy();
    m_server = NULL;
    m_isRunning = false;
    m_isConnected = false;
    m_newStatusText = _("Starting server failed");
    m_statusBar->NetworkStatus(StatusBar::error);
    return false;
  }
  m_newStatusText = _("Server started");
  m_server->SetEventHandler(*this, socket_server_id);
  m_server->SetNotify(wxSOCKET_CONNECTION_FLAG);
  m_server->Notify(true);

  m_isConnected = false;
  m_isRunning = true;
  return m_isRunning;
}

///--------------------------------------------------------------------------------
///  Maxima process stuff
///--------------------------------------------------------------------------------

bool wxMaxima::StartMaxima(bool force)
{
  m_nestedLoadCommands = 0;
  // We only need to start or restart maxima if we aren't connected to a maxima
  // that till now never has done anything and therefore is in perfect working
  // order.
  if ((m_process == NULL) || (m_hasEvaluatedCells) || force)
  {
    m_closing = true;
    if(m_process != NULL)
      KillMaxima();
    m_maximaStdoutPollTimer.StartOnce(MAXIMAPOLLMSECS);

    wxString command = GetCommand();

    if (command.Length() > 0)
    {

      command.Append(wxString::Format(wxT(" -s %d "), m_port));

#if defined __WXMAC__
      wxSetEnv(wxT("DISPLAY"), wxT(":0.0"));
#endif

#if defined __WXMSW__
      // Tell maxima we want to be able to kill it on Ctrl+G by sending it a signal
      wxSetEnv(wxT("MAXIMA_SIGNALS_THREAD"), wxT("1"));
#endif
      m_process = new wxProcess(this, maxima_process_id);
      m_process->Redirect();
      m_process->SetPriority(wxPRIORITY_MAX);
      m_first = true;
      m_pid = -1;
      wxLogMessage(wxString::Format(_("Running maxima as: %s"), command));
      if (wxExecute(command, wxEXEC_ASYNC, m_process) < 0)
      {
        StatusMaximaBusy(process_wont_start);
        m_newStatusText = _("Cannot start the maxima binary");
        wxLogMessage(m_newStatusText);
        m_process = NULL;
        m_maximaStdout = NULL;
        m_maximaStderr = NULL;
        m_statusBar->NetworkStatus(StatusBar::offline);
        return false;
      }
      m_maximaStdout = m_process->GetInputStream();
      m_maximaStderr = m_process->GetErrorStream();
      m_lastPrompt = wxT("(%i1) ");
      StatusMaximaBusy(wait_for_start);
    }
    else
    {
      m_statusBar->NetworkStatus(StatusBar::offline);
      wxLogMessage(_("Cannot find a maxima binary and no binary chosen in the config dialogue."));
      return false;
    }
  }
  m_console->m_cellPointers.m_errorList.Clear();

  // Initialize the performance counter.
  GetMaximaCPUPercentage();
  return true;
}


void wxMaxima::Interrupt(wxCommandEvent& WXUNUSED(event))
{
    if(m_console != NULL)
    m_console->CloseAutoCompletePopup();

  if (m_pid < 0)
  {
    GetMenuBar()->Enable(menu_interrupt_id, false);
    return;
  }

#if defined (__WXMSW__)
  if(m_pid > 0)
  {
    // The following lines are adapted from maxima's winkill which William Schelter has
    // written and which has been improved by David Billinghurst and
    // Andrej Vodopivec.
    //
    // Winkill tries to find a shared memory region maxima provides we can set signals
    // in that maxima can listen to.
    //
    // For maxima's end of this means of communication see
    // interfaces/xmaxima/win32/win_signals.lisp
    // and interfaces/xmaxima/win32/winkill_lib.c in maxima's tree.
    HANDLE sharedMemoryHandle = 0;
    LPVOID sharedMemoryAddress = 0;
    wchar_t sharedMemoryName[51];
    sharedMemoryName[50] = 0;

    // wxMaxima doesn't want to get interrupt signals.
    // SetConsoleCtrlHandler(NULL, true);

    /* First try to send the signal to gcl. */
    wxString sharedMemoryName1 = wxString::Format("gcl-%d", m_pid);
    wcsncpy(sharedMemoryName, sharedMemoryName1.wchar_str(), 50);
    sharedMemoryHandle = OpenFileMapping(FILE_MAP_WRITE,     /*  Read/write permission.   */
                                         FALSE,              /*  Do not inherit the name  */
                                         sharedMemoryName); /*  of the mapping object.   */

    /* If gcl is not running, send to maxima. */
    wxString sharedMemoryName2 = wxString::Format("maxima-%d", m_pid);
    if (sharedMemoryHandle == NULL) {
      wcsncpy(sharedMemoryName, sharedMemoryName2.wchar_str(), 50);
      sharedMemoryHandle = OpenFileMapping(FILE_MAP_WRITE,     /*  Read/write permission.   */
                                           FALSE,              /*  Do not inherit the name  */
                                           sharedMemoryName); /*  of the mapping object.   */
    }

    if (sharedMemoryHandle == NULL)
    {
      wxLogMessage(_("The Maxima process doesn't offer a shared memory segment we can send an interrupt signal to."));

      // No shared memory location we can send break signals to => send a
      // console interrupt.
      // Before we do that we stop our program from closing on receiving a Ctrl+C
      // from the console.
      SetConsoleCtrlHandler(NULL,TRUE);

      // We could send a CTRL_BREAK_EVENT instead of a CTRL_C_EVENT that
      // isn't handled in the 2010 clisp release (see:
      // https://sourceforge.net/p/clisp/bugs/735/)
      // ...but CTRL_BREAK_EVENT seems to crash clisp, see
      // https://sourceforge.net/p/clisp/bugs/736/
      //
      // And we need to send the CTRL_BREAK_EVENT to our own console, which
      // has the group ID 0, see
      // https://docs.microsoft.com/en-us/windows/console/generateconsolectrlevent
      if (GenerateConsoleCtrlEvent(CTRL_C_EVENT, 0) == 0)
      {
        LPTSTR errorText = NULL;

        FormatMessage(
          FORMAT_MESSAGE_FROM_SYSTEM
          |FORMAT_MESSAGE_ALLOCATE_BUFFER
          |FORMAT_MESSAGE_IGNORE_INSERTS,
          NULL,GetLastError(),MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
          errorText,0,NULL);

        wxString errorMessage;
        if (!errorText)
          errorMessage = _("Could not send an interrupt signal to maxima.");
        else
        {
          errorMessage = wxString::Format(_("Interrupting maxima: %s"),
                                          errorText);
          LocalFree(errorText);
        }

        SetStatusText(errorMessage, 0);
        wxLogMessage(errorMessage);
        return;
      }
    }
    else
    {
      sharedMemoryAddress = MapViewOfFile(sharedMemoryHandle, /* Handle to mapping object.  */
                                          FILE_MAP_WRITE,      /* Read/write permission.  */
                                          0,                   /* Max.  object size.  */
                                          0,                   /* Size of hFile.  */
                                          0);                  /* Map entire file.  */

      if (sharedMemoryAddress == NULL)
      {
        wxLogMessage(_("Could not map view of the file needed in order to "
                       "send an interrupt signal to maxima."));
        return;
      }

      // Set the bit for the SIGINT handler
      int value = (1 << (wxSIGINT));
      int *sharedMemoryContents = (int *)(sharedMemoryAddress);
      if (sharedMemoryAddress)
      {
        *sharedMemoryContents = *sharedMemoryContents | value;
        wxLogMessage(_("Sending an interrupt signal to Maxima."));
        UnmapViewOfFile(sharedMemoryAddress);
      }
      if (sharedMemoryHandle)
        CloseHandle(sharedMemoryHandle);
      sharedMemoryAddress = NULL;
      sharedMemoryHandle = NULL;
    }
  }
  else
  {
    if(m_process)
    {
      // We need to send the CTRL_BREAK_EVENT to the process group, not
      // to the lisp.
      long pid = m_process->GetPid();
      if (!GenerateConsoleCtrlEvent(CTRL_C_EVENT, pid))
      {
        wxLogMessage(_("Could not send an interrupt signal to maxima."));
        return;
      }
      else
        wxLogMessage(_("Sending an interactive Interrupt signal (Ctrl+C) to Maxima."));
    }
  }
#else
  wxLogMessage(_("Sending Maxima a SIGINT signal."));
  wxProcess::Kill(m_pid, wxSIGINT);
#endif
}

void wxMaxima::KillMaxima()
{
  if((m_pid > 0) && (m_client == NULL))
    return;

  wxLogMessage(_("Killing Maxima."));
  m_nestedLoadCommands = 0;
  m_configCommands = wxEmptyString;
  // The new maxima process will be in its initial condition => mark it as such.
  m_hasEvaluatedCells = false;

  m_console->m_cellPointers.SetWorkingGroup(NULL);
  m_console->m_evaluationQueue.Clear();
  EvaluationQueueLength(0);

  // We start checking for maximas output again as soon as we send some data to the program.
  m_statusBar->SetMaximaCPUPercentage(0);
  m_CWD = wxEmptyString;
  m_console->QuestionAnswered();
  m_currentOutput = wxEmptyString;
  // If we did close maxima by hand we already might have a new process
  // and therefore invalidate the wrong process in this step
  if (m_process)
    m_process->Detach();
  m_process = NULL;
  m_maximaStdout = NULL;
  m_maximaStderr = NULL;

  if (m_pid <= 0)
  {
    if(m_client)
    {
      if (m_inLispMode)
        SendMaxima(wxT("($quit)"));
      else
        SendMaxima(wxT("quit();"));
      return;
    }
  }
  else
    wxProcess::Kill(m_pid, wxSIGKILL, wxKILL_CHILDREN);

  wxDELETE(m_clientTextStream);m_clientTextStream = NULL;
  wxDELETE(m_clientStream); m_clientStream = NULL;

  if (m_client)
    m_client->Close();
  m_client = NULL;
  m_isConnected = false;

  // As we might have killed maxima before it was able to clean up its
  // temp files
  // we try to do so manually now:
  if(m_maximaTempDir != wxEmptyString)
  {
    wxLogNull logNull;
    wxRemoveFile(m_maximaTempDir + wxT("/maxout") + wxString::Format("%li.gnuplot",m_pid));
    wxRemoveFile(m_maximaTempDir + wxT("/data") + wxString::Format("%li.gnuplot",m_pid));
    wxRemoveFile(m_maximaTempDir + wxT("/maxout") + wxString::Format("%li.xmaxima",m_pid));
    wxRemoveFile(m_maximaTempDir + wxT("/maxout_") + wxString::Format("%li.gnuplot",m_pid));
    wxRemoveFile(m_maximaTempDir + wxT("/data_") + wxString::Format("%li.gnuplot",m_pid));
    wxRemoveFile(m_maximaTempDir + wxT("/maxout_") + wxString::Format("%li.xmaxima",m_pid));
  }
  m_pid = -1;
}

void wxMaxima::OnGnuplotClose(wxProcessEvent& WXUNUSED(event))
{
  m_gnuplotProcess = NULL;
  wxLogMessage(_("Gnuplot has closed."));
}

void wxMaxima::OnProcessEvent(wxProcessEvent& WXUNUSED(event))
{
  wxLogMessage(_("Maxima has terminated."));
  m_statusBar->NetworkStatus(StatusBar::offline);
  if (!m_closing)
  {
    m_newStatusText = _("Maxima process terminated.");

    // Let's see if maxima has told us why this did happen.
    ReadStdErr();

    // if m_closing==true we might already have a new process
    // and therefore the following lines would probably mark
    // the wrong process as "deleted".
    m_process = NULL;
    m_maximaStdout = NULL;
    m_maximaStderr = NULL;
    m_maximaVersion = wxEmptyString;
    m_lispVersion = wxEmptyString;

    ConsoleAppend(wxT("\nMaxima exited...\n"),
                  MC_TYPE_ERROR);

    m_isConnected = false;
    if (m_unsuccessfulConnectionAttempts > 10)
      ConsoleAppend(wxT("Restart Maxima with 'Maxima->Restart Maxima'.\n"),
                    MC_TYPE_ERROR);
    else
    {
      ConsoleAppend(wxT("Trying to restart Maxima.\n"),
                    MC_TYPE_ERROR);
      // Perhaps we shouldn't restart maxima again if it outputs a prompt and
      // crashes immediately after => Each prompt is deemed as but one hint
      // for a working maxima while each crash counts twice.
      m_unsuccessfulConnectionAttempts += 2;
      StartMaxima(true);
    }
    m_console->m_evaluationQueue.Clear();
  }
  StatusMaximaBusy(disconnected);
}

void wxMaxima::CleanUp()
{
  if (m_isConnected)
    KillMaxima();

  if (m_isRunning)
  {
    if (m_server)
      m_server->Destroy();
    m_server = NULL;
  }
  if(m_process)
    m_process->Detach();
  m_process = NULL;

}

///--------------------------------------------------------------------------------
///  Dealing with stuff read from the socket
///--------------------------------------------------------------------------------

void wxMaxima::ReadFirstPrompt(wxString &data)
{
  int end;
  if((end = m_currentOutput.Find(m_firstPrompt)) == wxNOT_FOUND)
    return;

//  m_console->m_cellPointers.m_currentTextCell = NULL;

  int start = 0;
  start = data.Find(wxT("Maxima "));
  if (start == wxNOT_FOUND)
    start = 0;
  FirstOutput();

  // Wait for a line maxima informs us about it's process id in.
  int s = data.Find(wxT("pid=")) + 4;
  int t = s + data.SubString(s, data.Length()).Find(wxT("\n")) - 1;

  // Read this pid
  if (s < t)
    data.SubString(s, t).ToLong(&m_pid);

  if (m_pid > 0)
    GetMenuBar()->Enable(menu_interrupt_id, true);

  m_first = false;
  m_inLispMode = false;
  StatusMaximaBusy(waiting);
  m_closing = false; // when restarting maxima this is temporarily true

  wxString prompt_compact = data.Left(start + end + m_firstPrompt.Length() - 1);
  prompt_compact.Replace(wxT("\n"),wxT("\x21b2"));


  wxLogMessage(wxString::Format(_("Received maxima's first prompt: %s"),
                                prompt_compact));

  // Remove the first prompt from Maxima's answer.
  data = data.Right(data.Length() - end - m_firstPrompt.Length());

  if (m_console->m_evaluationQueue.Empty())
  {
    // Inform the user that the evaluation queue is empty.
    EvaluationQueueLength(0);
    if ((m_console->m_configuration->GetOpenHCaret()) && (m_console->GetActiveCell() == NULL))
      m_console->OpenNextOrCreateCell();
  }
  else
  {
    // Needed in order to make batch mode start
    if(m_evalOnStartup)
      TryEvaluateNextInQueue();
  }
}

int wxMaxima::GetMiscTextEnd(const wxString &data)
{
  // These tests are redundant with later tests. But they are faster.
  if(data.StartsWith("<mth>"))
    return 0;
  if(data.StartsWith("<lbl>"))
    return 0;
  if(data.StartsWith("<statusbar>"))
    return 0;
  if(data.StartsWith(m_promptPrefix))
    return 0;
  if(data.StartsWith(m_symbolsPrefix))
    return 0;
  if(data.StartsWith(m_variablesPrefix))
    return 0;

  int mthpos = data.Find("<mth>");
  int lblpos = data.Find("<lbl>");
  int statpos = data.Find("<statusbar>");
  int prmptpos = data.Find(m_promptPrefix);
  int symbolspos = data.Find(m_symbolsPrefix);
  int variablespos = data.Find(m_variablesPrefix);

  int tagPos = data.Length();
  if ((mthpos != wxNOT_FOUND) && (mthpos < tagPos))
    tagPos = mthpos;
  if ((tagPos == wxNOT_FOUND) || ((lblpos != wxNOT_FOUND) && (lblpos < tagPos)))
    tagPos = lblpos;
  if ((tagPos == wxNOT_FOUND) || ((statpos != wxNOT_FOUND) && (statpos < tagPos)))
    tagPos = statpos;
  if ((tagPos == wxNOT_FOUND) || ((prmptpos != wxNOT_FOUND) && (prmptpos < tagPos)))
    tagPos = prmptpos;
  if ((tagPos == wxNOT_FOUND) || ((prmptpos != wxNOT_FOUND) && (prmptpos < tagPos)))
    tagPos = prmptpos;
  if ((tagPos == wxNOT_FOUND) || ((symbolspos != wxNOT_FOUND) && (symbolspos < tagPos)))
    tagPos = symbolspos;
  if ((tagPos == wxNOT_FOUND) || ((variablespos != wxNOT_FOUND) && (variablespos < tagPos)))
    tagPos = variablespos;
  return tagPos;
}

void wxMaxima::ReadMiscText(wxString &data)
{
  if (data.IsEmpty())
    return;

  // Extract all text that isn't a xml tag known to us.
  int miscTextLen = GetMiscTextEnd(data);
  if(miscTextLen <= 0)
  {
    if(data != wxEmptyString)
      m_console->m_cellPointers.m_currentTextCell = NULL;
    return;
  }

  wxString miscText = data.Left(miscTextLen);
  data = data.Right(data.Length() - miscTextLen);

  // Stupid DOS and MAC line endings. The first of these commands won't work
  // if the "\r" is the last char of a packet containing a part of a very long
  // string. But running a search-and-replace
  miscText.Replace("\r\n","\n");
  miscText.Replace("\r","\n");

  if(miscText.StartsWith("\n"))
    m_console->m_cellPointers.m_currentTextCell = NULL;

  // A version of the text where each line begins with non-whitespace and whitespace
  // characters are merged.
  wxString mergedWhitespace = wxT("\n");
  bool whitespace = true;
  for ( wxString::iterator it = miscText.begin(); it!=miscText.end(); ++it)
  {
    if((*it == wxT(' ')) || (*it == wxT('\t')))
    {
      // Merge non-newline whitespace to a space.
      if(!whitespace)
        mergedWhitespace += wxT(' ');
    }
    else
      mergedWhitespace += *it;

    if((*it == wxT(' ')) || (*it == wxT('\t')) || (*it == wxT('\n')))
      whitespace = true;
    else
      whitespace = false;
  }

  bool error   = false;
  bool warning = false;
  if (
    (mergedWhitespace.Contains(wxT("\n-- an error."))) ||
    (mergedWhitespace.Contains(wxT(":incorrect syntax:"))) ||
    (mergedWhitespace.Contains(wxT("\nincorrect syntax"))) ||
    (mergedWhitespace.Contains(wxT("\nMaxima encountered a Lisp error"))) ||
    (mergedWhitespace.Contains(wxT("\nkillcontext: no such context"))) ||
    (mergedWhitespace.Contains(wxT("\ndbl:MAXIMA>>"))) ||  // a gcl error message
    (mergedWhitespace.Contains(wxT("\nTo enable the Lisp debugger set *debugger-hook* to nil."))) // a scbl error message
    )
    error = true;

  if ((mergedWhitespace.StartsWith(wxT("Warning:"))) ||
      (mergedWhitespace.StartsWith(wxT("warning:"))) ||
      (mergedWhitespace.StartsWith(wxT("WARNING:"))) ||
      (mergedWhitespace.Contains(wxT("\nWarning:"))) ||
      (mergedWhitespace.Contains(wxT("\nWARNING:"))) ||
      (mergedWhitespace.Contains(wxT("\nwarning:"))) ||
      (mergedWhitespace.Contains(wxT(": Warning:"))) ||
      (mergedWhitespace.Contains(wxT(": warning:")))
    )
    warning = true;
  else
  {
    // Gnuplot errors differ from gnuplot warnings by not containing a "warning:"
    if (m_gnuplotErrorRegex.Matches(mergedWhitespace))
      error = true;
  }

  // Add all text lines to the console
  wxStringTokenizer lines(miscText, wxT("\n"));
  while (lines.HasMoreTokens())
  {
    // extract a string from the Data lines
    wxString textline = lines.GetNextToken();
    wxString trimmedLine = textline;

    trimmedLine.Trim(true);
    trimmedLine.Trim(false);

    if((textline != wxEmptyString)&&(textline != wxT("\n")))
    {
      if(error)
      {
        m_console->m_cellPointers.m_currentTextCell = ConsoleAppend(textline, MC_TYPE_ERROR);
        AbortOnError();
      }
      else
      {
        if(warning)
          m_console->m_cellPointers.m_currentTextCell = ConsoleAppend(textline, MC_TYPE_WARNING);
        else
          m_console->m_cellPointers.m_currentTextCell = ConsoleAppend(textline, MC_TYPE_DEFAULT);
      }
    }
    if(lines.HasMoreTokens())
      m_console->m_cellPointers.m_currentTextCell = NULL;
  }
  if(miscText.EndsWith("\n"))
    m_console->m_cellPointers.m_currentTextCell = NULL;

  if(data != wxEmptyString)
    m_console->m_cellPointers.m_currentTextCell = NULL;
}

int wxMaxima::FindTagEnd(wxString &data, const wxString &tag)
{
  if((m_currentOutputEnd == wxEmptyString) || (m_currentOutputEnd.Find(tag) != wxNOT_FOUND))
    return data.Find(tag);
  else
    return wxNOT_FOUND;
}

void wxMaxima::ReadStatusBar(wxString &data)
{
  wxString statusbarStart = wxT("<statusbar>");
  if (!data.StartsWith(statusbarStart))
    return;

  m_console->m_cellPointers.m_currentTextCell = NULL;

  wxString sts = wxT("</statusbar>");
  int end;
  if ((end = FindTagEnd(data,sts)) != wxNOT_FOUND)
  {
    wxXmlDocument xmldoc;
    wxString xml = data.Left( end + sts.Length());
    wxStringInputStream xmlStream(xml);
    xmldoc.Load(xmlStream, wxT("UTF-8"));
    wxXmlNode *node = xmldoc.GetRoot();
    if(node != NULL)
    {
      wxXmlNode *contents = node->GetChildren();
      if(contents)
        SetStatusText(contents->GetContent());
    }
    // Remove the status bar info from the data string
    data = data.Right(data.Length()-end-sts.Length());
  }
}

/***
 * Checks if maxima displayed a new chunk of math
 */
void wxMaxima::ReadMath(wxString &data)
{
  wxString mthstart = wxT("<mth>");
  if (!data.StartsWith(mthstart))
    return;

  m_console->m_cellPointers.m_currentTextCell = NULL;

  // Append everything from the "beginning of math" to the "end of math" marker
  // to the console and remove it from the data we got.
  wxString mthend = wxT("</mth>");
  int end;
  if ((end = FindTagEnd(data,mthend)) != wxNOT_FOUND)
  {
    wxString o = data.Left(end + mthend.Length());
    data = data.Right(data.Length()-end-mthend.Length());
    o.Trim(true);
    o.Trim(false);

    if (o.Length() > 0)
    {
      if (m_console->m_configuration->UseUserLabels())
      {
        ConsoleAppend(o, MC_TYPE_DEFAULT,m_console->m_evaluationQueue.GetUserLabel());
      }
      else
      {
        ConsoleAppend(o, MC_TYPE_DEFAULT);
      }
    }
  }
}

void wxMaxima::ReadLoadSymbols(wxString &data)
{
  if (!data.StartsWith(m_symbolsPrefix))
    return;

  m_console->m_cellPointers.m_currentTextCell = NULL;

  int end = FindTagEnd(data, m_symbolsSuffix);

  if (end != wxNOT_FOUND)
  {
    // Put the symbols into a separate string
    wxString symbols = data.Left( end + m_symbolsSuffix.Length());
    m_console->AddSymbols(symbols);

    // Remove the symbols from the data string
    data = data.Right(data.Length()-end-m_symbolsSuffix.Length());
  }
}

void wxMaxima::ReadVariables(wxString &data)
{
  if (!data.StartsWith(m_variablesPrefix))
    return;

  int end = FindTagEnd(data, m_variablesSuffix);

  if (end != wxNOT_FOUND)
  {
    wxLogMessage(_("Maxima sends a new set of auto-completible symbols."));
    wxXmlDocument xmldoc;
    wxString xml = data.Left( end + m_variablesSuffix.Length());
    wxStringInputStream xmlStream(xml);
    xmldoc.Load(xmlStream, wxT("UTF-8"));
    wxXmlNode *node = xmldoc.GetRoot();
    if(node != NULL)
    {
      wxXmlNode *vars = node->GetChildren();
      while (vars != NULL)
      {
        wxXmlNode *var = vars->GetChildren();

        wxString name;
        wxString value;
        bool bound = false;
        while(var != NULL)
        {
          if(var->GetName() == wxT("name"))
          {
            wxXmlNode *namenode = var->GetChildren();
            if(namenode)
              name = namenode->GetContent();
          }
          if(var->GetName() == wxT("value"))
          {
            wxXmlNode *valnode = var->GetChildren();
            if(valnode)
            {
              bound = true;
              value = valnode->GetContent();
            }
          }

          if(bound)
          {
            if(name == "maxima_userdir")
              m_console->m_configuration->m_dirStructure.UserConfDir(value);
            if(name == "maxima_tempdir")
              m_maximaTempDir = value;
            if(name == "*autoconf-version*")
              m_maximaVersion = value;
            if(name == "*autoconf-host*")
              m_maximaArch = value;
            if(name == "*lisp-name*")
              m_lispType = value;
            if(name == "*lisp-version*")
              m_lispVersion = value;
            if(name == "*wx-load-file-name*")
            {
              if(m_nestedLoadCommands == 0)
              {
                m_recentPackages.AddDocument(value);
                wxLogMessage(wxString::Format(_("Maxima loads the file %s."),value));
              }
            }
            if(name == "*wx-load-file-start*")
            {
              if(value == "0")
                m_nestedLoadCommands -= 1;
              if(value == "1")
                m_nestedLoadCommands += 1;
              if(m_nestedLoadCommands < 0)
                m_nestedLoadCommands = 0;
              if((value == "0") && (m_nestedLoadCommands == 0))
                UpdateRecentDocuments();
            }
         }
          var = var->GetNext();
        }
        vars = vars->GetNext();
      }
    }

    // Remove the symbols from the data string
    data = data.Right(data.Length()-end-m_variablesSuffix.Length());
  }
}

/***
 * Checks if maxima displayed a new prompt.
 */
void wxMaxima::ReadPrompt(wxString &data)
{
  if (!data.StartsWith(m_promptPrefix))
    return;

  m_console->m_cellPointers.m_currentTextCell = NULL;

  // Assume we don't have a question prompt
  m_console->m_questionPrompt = false;
  m_ready = true;
  int end = FindTagEnd(data,m_promptSuffix);
  // Did we find a prompt?
  if (end == wxNOT_FOUND)
    return;

  wxString o = data.SubString(m_promptPrefix.Length(), end - 1);
  // Remove the prompt we will process from the string.
  data = data.Right(data.Length()-end-m_promptSuffix.Length());
  if(data == wxT(" "))
    data = wxEmptyString;

  // If we got a prompt our connection to maxima was successful.
  if(m_unsuccessfulConnectionAttempts > 0)
    m_unsuccessfulConnectionAttempts--;

  // Input prompts have a length > 0 and end in a number followed by a ")".
  // They also begin with a "(". Questions (hopefully)
  // don't do that; Lisp prompts look like question prompts.
  if (
          (
                  (o.Length() > 3) &&
                  (o[o.Length() - 3] >= (wxT('0'))) &&
                  (o[o.Length() - 3] <= (wxT('9'))) &&
                  (o[o.Length() - 2] == (wxT(')'))) &&
                  (o[0] == (wxT('(')))
          ) ||
          m_inLispMode ||
          (o.StartsWith(wxT("MAXIMA>"))) ||
          (o.StartsWith(wxT("\nMAXIMA>")))
          )
  {
    o.Trim(true);
    o.Trim(false);
    // Maxima displayed a new main prompt => We don't have a question
    m_console->QuestionAnswered();

    //m_lastPrompt = o.Mid(1,o.Length()-1);
    //m_lastPrompt.Replace(wxT(")"), wxT(":"), false);
    m_lastPrompt = o;
    // remove the event maxima has just processed from the evaluation queue
    m_console->m_evaluationQueue.RemoveFirst();
    // if we remove a command from the evaluation queue the next output line will be the
    // first from the next command.
    m_outputCellsFromCurrentCommand = 0;
    if (m_console->m_evaluationQueue.Empty())
    { // queue empty.
      StatusMaximaBusy(waiting);
      // If we have selected a cell in order to show we are evaluating it
      // we should now remove this marker.
      if (m_console->FollowEvaluation())
      {
        if (m_console->GetActiveCell())
          m_console->GetActiveCell()->SelectNone();
        m_console->SetSelection(NULL, NULL);
      }
      m_console->FollowEvaluation(false);
      if (m_exitAfterEval)
      {
        SaveFile(false);
        Close();
      }
      // Inform the user that the evaluation queue is empty.
      EvaluationQueueLength(0);
      m_console->m_cellPointers.SetWorkingGroup(NULL);
      m_console->m_evaluationQueue.RemoveFirst();
      m_console->RequestRedraw();
    }
    else
    { // we don't have an empty queue
      m_ready = false;
      m_console->RequestRedraw();
      StatusMaximaBusy(calculating);
      m_console->m_cellPointers.SetWorkingGroup(NULL);
      TryEvaluateNextInQueue();
    }

    if (m_console->m_evaluationQueue.Empty())
    {
      if ((m_console->m_configuration->GetOpenHCaret()) && (m_console->GetActiveCell() == NULL))
        m_console->OpenNextOrCreateCell();
    }
  }
  else
  {  // We have a question
    m_console->QuestionAnswered();
    m_console->QuestionPending(true);
    // If the user answers a question additional output might be required even
    // if the question has been preceded by many lines.
    m_outputCellsFromCurrentCommand = 0;
    if((m_console->GetWorkingGroup() == NULL) ||
       ((m_console->GetWorkingGroup()->m_knownAnswers.empty()) &&
        m_console->GetWorkingGroup()->AutoAnswer()))
       m_console->SetNotification(_("Maxima asks a question!"),wxICON_INFORMATION);
    if (!o.IsEmpty())
    {
      m_console->m_configuration->SetDefaultMathCellToolTip(
        _("Most questions can be avoided using the assume() "
          "and the declare() command"));
      if (o.Find(wxT("<mth>")) > -1)
        DoConsoleAppend(o, MC_TYPE_PROMPT);
      else
        DoRawConsoleAppend(o, MC_TYPE_PROMPT);
      m_console->m_configuration->SetDefaultMathCellToolTip(wxEmptyString);
  }
    if (m_console->ScrolledAwayFromEvaluation())
    {
      if (m_console->m_mainToolBar)
        m_console->m_mainToolBar->EnableTool(ToolBar::tb_follow, true);
    }
    else
    {
      m_console->OpenQuestionCaret();
    }
    StatusMaximaBusy(userinput);
  }

  if (o.StartsWith(wxT("\nMAXIMA>")))
    m_inLispMode = true;
  else
    m_inLispMode = false;
}

void wxMaxima::SetCWD(wxString file)
{
  // If maxima isn't connected we cannot do anything
  if (!m_client)
    return;

  // Tell the math parser where to search for local files.
  MathParser mParser(&m_console->m_configuration, &m_console->m_cellPointers);
  m_console->m_configuration->SetWorkingDirectory(wxFileName(file).GetPath());

#if defined __WXMSW__
  file.Replace(wxT("\\"), wxT("/"));
#endif

  wxFileName filename(file);

  if (filename.GetPath() == wxEmptyString)
    filename.AssignDir(wxGetCwd());

  // Escape all backslashes in the filename if needed by the OS.
  wxString filenamestring = filename.GetFullPath();
  wxString dirname = filename.GetPath();

#if defined (__WXMSW__)
  // On MSW filenames with a "\" are widely used - but only partially supported.
  filenamestring.Replace(wxT("\\"),wxT("/"));
  dirname.Replace(wxT("\\"),wxT("/"));
#endif

  wxString workingDirectory = filename.GetPath();

  bool wxcd = true;

#if defined (__WXMSW__)
  wxConfig::Get()->Read(wxT("wxcd"),&wxcd);
#endif

  if (wxcd && (workingDirectory != GetCWD()))
  {
    m_configCommands += wxT(":lisp-quiet (setf $wxfilename \"") +
      filenamestring +
      wxT("\")\n");
    m_configCommands += wxT(":lisp-quiet (setf $wxdirname \"") +
      dirname +
      wxT("\")\n");

    m_configCommands += wxT(":lisp-quiet (wx-cd \"") + filenamestring + wxT("\")\n");
    if (m_ready)
    {
      if (m_console->m_evaluationQueue.Empty())
        StatusMaximaBusy(waiting);
    }
    m_CWD = workingDirectory;
  }
}

wxString wxMaxima::ReadMacContents(wxString file)
{
  bool xMaximaFile = file.Lower().EndsWith(wxT(".out"));

    // open mac file
  wxTextFile inputFile(file);

  if (!inputFile.Open())
  {
    wxMessageBox(_("wxMaxima encountered an error loading ") + file, _("Error"), wxOK | wxICON_EXCLAMATION);
    StatusMaximaBusy(waiting);
    m_newStatusText = _("File could not be opened");
    return wxEmptyString;
  }

  bool input = true;
  wxString macContents;
  wxString line = inputFile.GetFirstLine();
  do
  {
    if(xMaximaFile)
    {
      // Detect output cells.
      if(line.StartsWith(wxT("(%o")))
        input = false;

      if(line.StartsWith(wxT("(%i")))
      {
        int end = line.Find(wxT(")"));
        if(end > 0)
        {
          line = line.Right(line.Length() - end - 2);
          input = true;
        }
      }

    }

    if(input)
      macContents += line + wxT("\n");

    if(!inputFile.Eof())
      line = inputFile.GetNextLine();

  } while (!inputFile.Eof());
  inputFile.Close();

  return macContents;
}

bool wxMaxima::OpenMACFile(wxString file, MathCtrl *document, bool clearDocument)
{
  // Show a busy cursor while we open the file.
  wxBusyCursor crs;

  m_newStatusText = _("Opening file");
  document->Freeze();

  wxString macContents = ReadMacContents(file);

  if(macContents == wxEmptyString)
  {
    document->Thaw();
    return false;
  }

  if (clearDocument)
    document->ClearDocument();

  GroupCell *last = NULL;
  wxString line = wxEmptyString;
  wxChar lastChar = wxT(' ');
  wxString::iterator ch = macContents.begin();
  while (ch != macContents.end())
  {

    // Handle comments
    if((*ch == '*') && (lastChar == '/'))
    {
      // Does the current line contain nothing but a comment?
      bool isCommentLine = false;
      wxString test = line;
      test.Trim(false);
      if(test == wxT("/"))
      {
        isCommentLine = true;
        line.Trim(false);
      }

      // Skip to the end of the comment
      while (ch != macContents.end())
      {
        line += *ch;

        if ((lastChar == wxT('*')) && (*ch == wxT('/')))
        {
          lastChar = *ch;
          if(ch != macContents.end())
            ++ch;
          break;
        }

        lastChar = *ch;
        if(ch != macContents.end())
          ++ch;
      }

      if(isCommentLine)
      {
        line.Trim(true);
        line.Trim(false);

        // Is this a comment from wxMaxima?
        if(line.StartsWith(wxT("/* [wxMaxima: ")))
        {

          // Add the rest of this comment block to the "line".
          while(
            (
              !(
                  (line.EndsWith(" end   ] */")) ||
                  (line.EndsWith(" end   ] */\n"))
                  )
              ) &&
            (ch != macContents.end())
            )
          {
            while(ch != macContents.end())
            {
              wxChar c = *ch;
              line += *ch;
              ++ch;
              if(c == wxT('\n'))
                break;
            }
          }

          // If the last block was a caption block we need to read in the image
          // the caption was for, as well.
          if(line.StartsWith(wxT("/* [wxMaxima: caption start ]")))
          {
            if(ch != macContents.end())
            {
              line += *ch;
              ++ch;
            }
            while(ch != macContents.end())
            {
              wxChar c = *ch;
              line += *ch;
              ++ch;
              if(c == wxT('\n'))
                break;
            }
            while(
              (
                !(
                  (line.EndsWith(" end   ] */")) ||
                  (line.EndsWith(" end   ] */\n"))
                  )
              ) &&
              (ch != macContents.end())
              )
            {
              while(ch != macContents.end())
              {
                wxChar c = *ch;
                line += *ch;
                ++ch;
                if(c == wxT('\n'))
                  break;
              }
            }
          }

          //  Convert the comment block to an array of lines
          wxStringTokenizer tokenizer(line, "\n");
          wxArrayString commentLines;
          while ( tokenizer.HasMoreTokens() )
            commentLines.Add(tokenizer.GetNextToken());

          // Interpret this array of lines as wxm code.
          GroupCell *cell;
          document->InsertGroupCells(
            cell = m_console->CreateTreeFromWXMCode(&commentLines),
            last);
          last = cell;

        }
          else
        {
          GroupCell *cell;

          if((line.StartsWith("/* ")) || (line.StartsWith("/*\n")))
            line = line.SubString(3,line.length()-1);
          else
            line = line.SubString(2,line.length()-1);

          if((line.EndsWith(" */")) || (line.EndsWith("\n*/")))
            line = line.SubString(0,line.length()-4);
          else
            line = line.SubString(0,line.length()-3);

          document->InsertGroupCells(
            cell = new GroupCell(&(document->m_configuration),
                                 GC_TYPE_TEXT, &document->m_cellPointers,
                                 line),
            last);
          last = cell;
        }

        line = wxEmptyString;
      }
    }
    // Handle strings
    else if((*ch == '\"') )
    {
      // Skip to the end of the string
      while (ch != macContents.end())
      {
        line += *ch;

        if ((*ch == wxT('\"')))
        {
          lastChar = *ch;
          if(ch != macContents.end())
            ++ch;
          break;
        }
      }
    }
    // Handle escaped chars
    else if((*ch == '\\') )
    {
      if(ch != macContents.end())
      {
        line += *ch;
        lastChar = *ch;
        ++ch;
      }
    }
    else
    {
      line += *ch;

      // A line ending followed by a new line means: We want to insert a new code cell.
      if(((lastChar == wxT('$')) || ((lastChar == wxT(';')))) && (*ch == wxT('\n')))
      {
        line.Trim(true);
        line.Trim(false);
        GroupCell *cell;
        document->InsertGroupCells(
          cell = new GroupCell(&(document->m_configuration),
                        GC_TYPE_CODE, &document->m_cellPointers, line),
          last);
        last = cell;
        line = wxEmptyString;
      }
      lastChar = *ch;
      if(ch != macContents.end())
        ++ch;
    }
  }

  line.Trim(true);
  line.Trim(false);
  if(line != wxEmptyString)
  {
    document->InsertGroupCells(
      new GroupCell(&(document->m_configuration),
                    GC_TYPE_CODE, &document->m_cellPointers, line),
      last);
  }

  if (clearDocument)
  {
//    m_console->m_currentFile = file.SubString(0,file.Length()-4) + wxT("wxmx");
    StartMaxima();
    m_console->m_currentFile = file;
    ResetTitle(true, true);
    document->SetSaved(true);
  }
  else
  {
    ResetTitle(false);
    m_console->UpdateTableOfContents();
  }

  document->Thaw();
  document->RequestRedraw(); // redraw document outside Freeze-Thaw

  m_console->SetDefaultHCaret();
  m_console->SetFocus();

  SetCWD(file);

  StatusMaximaBusy(waiting);
  m_newStatusText = _("File opened");

  m_console->SetHCaret(NULL);
  m_console->ScrollToCaret();
  return true;
}

// OpenWXMFile
// Clear document (if clearDocument == true), then insert file
bool wxMaxima::OpenWXMFile(wxString file, MathCtrl *document, bool clearDocument)
{
  // Show a busy cursor while we open the file.
  wxBusyCursor crs;

  m_newStatusText = _("Opening file");
  document->Freeze();

  // open wxm file
  wxTextFile inputFile(file);
  wxArrayString *wxmLines = NULL;

  if (!inputFile.Open())
  {
    document->Thaw();
    wxMessageBox(_("wxMaxima encountered an error loading ") + file, _("Error"), wxOK | wxICON_EXCLAMATION);
    StatusMaximaBusy(waiting);
    m_newStatusText = _("File could not be opened");
    return false;
  }

  if (inputFile.GetFirstLine() !=
      wxT("/* [wxMaxima batch file version 1] [ DO NOT EDIT BY HAND! ]*/"))
  {
    inputFile.Close();
    document->Thaw();
    wxMessageBox(_("wxMaxima encountered an error loading ") + file, _("Error"), wxOK | wxICON_EXCLAMATION);
    return false;
  }
  wxmLines = new wxArrayString();
  wxString line;
  for (line = inputFile.GetFirstLine();
       !inputFile.Eof();
       line = inputFile.GetNextLine())
  {
    wxmLines->Add(line);
  }
  wxmLines->Add(line);

  inputFile.Close();

  GroupCell *tree = m_console->CreateTreeFromWXMCode(wxmLines);

  wxDELETE(wxmLines);

  // from here on code is identical for wxm and wxmx
  if (clearDocument)
  {
    document->ClearDocument();
    StartMaxima();
  }

  document->InsertGroupCells(tree); // this also requests a recalculate

  if (clearDocument)
  {
    m_console->m_currentFile = file;
    ResetTitle(true, true);
    document->SetSaved(true);
  }
  else
    ResetTitle(false);

  document->Thaw();
  document->RequestRedraw(); // redraw document outside Freeze-Thaw

  m_console->SetDefaultHCaret();
  m_console->SetFocus();

  SetCWD(file);

  StatusMaximaBusy(waiting);
  m_newStatusText = _("File opened");

  m_console->SetHCaret(NULL);
  m_console->ScrollToCaret();
  RemoveTempAutosavefile();
  return true;
}

bool wxMaxima::OpenWXMXFile(wxString file, MathCtrl *document, bool clearDocument)
{
  // Show a busy cursor while we open a file.
  wxBusyCursor crs;

  m_newStatusText = _("Opening file");

  document->Freeze();

  // If the file is empty we don't want to generate an error, but just
  // open an empty file.
  //
  // This makes the following thing work on windows without the need of an
  // empty template file:
  //
  // - Create a registry key named HKEY_LOKAL_MACHINE\SOFTWARE\CLASSES\.wxmx\ShellNew
  // - Create a string named "NullFile" within this key
  //
  // => After the next reboot the right-click context menu's "new" submenu contains
  //    an entry that creates valid empty .wxmx files.
  if (wxFile(file, wxFile::read).Eof())
  {
    document->ClearDocument();
    StartMaxima();

    m_console->m_currentFile = file;
    ResetTitle(true, true);
    document->SetSaved(true);
    document->Thaw();
    RemoveTempAutosavefile();
    return true;
  }

  // open wxmx file
  wxXmlDocument xmldoc;

  // We get only absolute paths so the path should start with a "/"
  //if(!file.StartsWith(wxT("/")))
  //  file = wxT("/") + file;

  wxFileSystem fs;
  wxString wxmxURI = wxURI(wxT("file://") + file).BuildURI();
  // wxURI doesn't know that a "#" in a file name is a literal "#" and
  // not an anchor within the file so we have to care about url-encoding
  // this char by hand.
  wxmxURI.Replace("#", "%23");

#ifdef  __WXMSW__
  // Fixes a missing "///" after the "file:". This works because we always get absolute
  // file names.
  wxRegEx uriCorector1("^file:([a-zA-Z]):");
  wxRegEx uriCorector2("^file:([a-zA-Z][a-zA-Z]):");

  uriCorector1.ReplaceFirst(&wxmxURI,wxT("file:///\\1:"));
  uriCorector2.ReplaceFirst(&wxmxURI,wxT("file:///\\1:"));
#endif
  // The URI of the wxm code contained within the .wxmx file
  wxString filename = wxmxURI + wxT("#zip:content.xml");

  // Open the file
  wxFSFile *fsfile = fs.OpenFile(filename);
  if (!fsfile)
  {
    filename = wxmxURI + wxT("#zip:/content.xml");
    fsfile = fs.OpenFile(filename);
  }

  // Did we succeed in opening the file?
  if (fsfile)
  {
    // Let's see if we can load the XML contained in this file.
    if (!xmldoc.Load(*(fsfile->GetStream()), wxT("UTF-8"), wxXMLDOC_KEEP_WHITESPACE_NODES))
    {
      // If we cannot read the file a typical error in old wxMaxima versions was to include
      // a letter of ascii code 27 in content.xml. Let's filter this char out.

      // Re-open the file.
      wxDELETE(fsfile);
      fsfile = fs.OpenFile(filename);
      if (fsfile)
      {
        // Read the file into a string
        wxString s;
        wxTextInputStream istream1(*fsfile->GetStream(), wxT('\t'), wxConvAuto(wxFONTENCODING_UTF8));
        while (!fsfile->GetStream()->Eof())
          s += istream1.ReadLine() + wxT("\n");

        // Remove the illegal character
        s.Replace(wxT('\x1b'), wxT("|"));

        {
          // Write the string into a memory buffer
          wxMemoryOutputStream ostream;
          wxTextOutputStream txtstrm(ostream);
          txtstrm.WriteString(s);
          wxMemoryInputStream istream(ostream);

          // Try to load the file from the memory buffer.
          xmldoc.Load(istream, wxT("UTF-8"), wxXMLDOC_KEEP_WHITESPACE_NODES);
        }
      }
    }
  }
  else
  {
    document->Thaw();
    wxMessageBox(_("wxMaxima cannot open content.xml in the .wxmx zip archive ") + file +
                 wxT(", URI=") + filename, _("Error"),
                 wxOK | wxICON_EXCLAMATION);
    StatusMaximaBusy(waiting);
    m_newStatusText = _("File could not be opened");
    return false;
  }


  wxDELETE(fsfile);

  if (!xmldoc.IsOk())
  {
    document->Thaw();
    wxMessageBox(_("wxMaxima cannot read the xml contents of ") + file, _("Error"),
                 wxOK | wxICON_EXCLAMATION);
    StatusMaximaBusy(waiting);
    m_newStatusText = _("File could not be opened");
    return false;
  }

  // start processing the XML file
  if (xmldoc.GetRoot()->GetName() != wxT("wxMaximaDocument"))
  {
    document->Thaw();
    wxMessageBox(_("xml contained in the file claims not to be a wxMaxima worksheet. ") + file, _("Error"),
                 wxOK | wxICON_EXCLAMATION);
    StatusMaximaBusy(waiting);
    m_newStatusText = _("File could not be opened");
    return false;
  }

  // read document version and complain
  wxString docversion = xmldoc.GetRoot()->GetAttribute(wxT("version"), wxT("1.0"));
  if (!CheckWXMXVersion(docversion))
  {
    document->Thaw();
    StatusMaximaBusy(waiting);
    return false;
  }

  // Determine where the cursor was before saving
  wxString ActiveCellNumber_String = xmldoc.GetRoot()->GetAttribute(wxT("activecell"), wxT("-1"));
  long ActiveCellNumber;
  if (!ActiveCellNumber_String.ToLong(&ActiveCellNumber))
    ActiveCellNumber = -1;

  // read zoom factor
  wxString doczoom = xmldoc.GetRoot()->GetAttribute(wxT("zoom"), wxT("100"));

  // Read the worksheet's contents.
  wxXmlNode *xmlcells = xmldoc.GetRoot();
  GroupCell *tree = CreateTreeFromXMLNode(xmlcells, wxmxURI);

  // from here on code is identical for wxm and wxmx
  if (clearDocument)
  {
    document->ClearDocument();
    StartMaxima();
    long int zoom = 100;
    if (!(doczoom.ToLong(&zoom)))
      zoom = 100;
    document->SetZoomFactor(double(zoom) / 100.0, false); // Set zoom if opening, don't recalculate
  }

  document->InsertGroupCells(tree); // this also requests a recalculate
  if (clearDocument)
  {
    m_console->m_currentFile = file;
    ResetTitle(true, true);
    document->SetSaved(true);
  }
  else
    ResetTitle(false);

  document->Thaw();
  document->RequestRedraw(); // redraw document outside Freeze-Thaw

  m_console->SetDefaultHCaret();
  m_console->SetFocus();

  SetCWD(file);

  // We can set the cursor to the last known position.
  if (ActiveCellNumber == 0)
    m_console->SetHCaret(NULL);
  if (ActiveCellNumber > 0)
  {
    GroupCell *pos = m_console->GetTree();

    for (long i = 1; i < ActiveCellNumber; i++)
      if (pos)
        pos = dynamic_cast<GroupCell *>(pos->m_next);

    if (pos)
      m_console->SetHCaret(pos);
  }
  StatusMaximaBusy(waiting);
  m_newStatusText = _("File opened");
  RemoveTempAutosavefile();
  return true;
}

bool wxMaxima::CheckWXMXVersion(wxString docversion)
{
  double version = 1.0;
  if (docversion.ToDouble(&version))
  {
    int version_major = int(version);
    int version_minor = int(10 * (version - double(version_major)));

    if (version_major > DOCUMENT_VERSION_MAJOR)
    {
      wxMessageBox(_("Document was saved using a newer version of wxMaxima. Please update your wxMaxima."),
                   _("Error"), wxOK | wxICON_EXCLAMATION);
      m_newStatusText = _("File could not be opened");
      return false;
    }
    if (version_minor > DOCUMENT_VERSION_MINOR)
      wxMessageBox(
              _("Document was saved using a newer version of wxMaxima so it may not load correctly. Please update your wxMaxima."),
              _("Warning"), wxOK | wxICON_EXCLAMATION);
  }
  return true;
}

bool wxMaxima::OpenXML(wxString file, MathCtrl *document)
{
  // Show a busy cursor as long as we open a file.
  wxBusyCursor crs;

  m_newStatusText = _("Opening file");

  document->Freeze();

  wxXmlDocument xmldoc;

  // Let's see if we can load the XML contained in this file.
  xmldoc.Load(file);

  if (!xmldoc.IsOk())
  {
    document->Thaw();
    wxMessageBox(
            _("The .xml file doesn't seem to be valid xml or isn't a content.xml extracted from a .wxmx zip archive"),
            _("Error"),
            wxOK | wxICON_EXCLAMATION);
    StatusMaximaBusy(waiting);
    m_newStatusText = _("File could not be opened");
    return false;
  }

  // Process the XML document
  if (xmldoc.GetRoot()->GetName() != wxT("wxMaximaDocument"))
  {
    document->Thaw();
    wxMessageBox(_("xml contained in the file claims not to be a wxMaxima worksheet. ") + file, _("Error"),
                 wxOK | wxICON_EXCLAMATION);
    StatusMaximaBusy(waiting);
    m_newStatusText = _("File could not be opened");
    return false;
  }

  // read document version and complain
  wxString docversion = xmldoc.GetRoot()->GetAttribute(wxT("version"), wxT("1.0"));
  if (!CheckWXMXVersion(docversion))
  {
    document->Thaw();
    StatusMaximaBusy(waiting);
    return false;
  }

  // Read the worksheet's contents.
  wxXmlNode *xmlcells = xmldoc.GetRoot();
  GroupCell *tree = CreateTreeFromXMLNode(xmlcells, file);

  document->ClearDocument();
  StartMaxima();
  document->InsertGroupCells(tree); // this also requests a recalculate
  m_console->m_currentFile = file;
  ResetTitle(true, true);
  document->Thaw();
  document->RequestRedraw();
  m_console->SetDefaultHCaret();
  m_console->SetFocus();
  SetCWD(file);

  StatusMaximaBusy(waiting);
  m_newStatusText = _("File opened");
  return true;
}

GroupCell *wxMaxima::CreateTreeFromXMLNode(wxXmlNode *xmlcells, wxString wxmxfilename)
{
  MathParser mp(&m_console->m_configuration, &m_console->m_cellPointers, wxmxfilename);
  GroupCell *tree = NULL;
  GroupCell *last = NULL;

  bool warning = true;

  if (xmlcells)
    xmlcells = xmlcells->GetChildren();

  while (xmlcells != NULL)
  {
    if (xmlcells->GetType() != wxXML_TEXT_NODE)
    {
      MathCell *mc = mp.ParseTag(xmlcells, false);
      if (mc != NULL)
      {
        GroupCell *cell = dynamic_cast<GroupCell *>(mc);

        if (last == NULL)
        {
          // first cell
          last = tree = cell;
        }
        else
        {
          // The rest of the cells
          last->m_next = last->m_nextToDraw = cell;
          last->m_next->m_previous = last->m_next->m_previousToDraw = last;

          last = dynamic_cast<GroupCell *>(last->m_next);
        }
      }
      else if (warning)
      {
        wxMessageBox(_("Parts of the document will not be loaded correctly!"), _("Warning"),
                     wxOK | wxICON_WARNING);
        warning = false;
      }
    }
    xmlcells = xmlcells->GetNext();
  }
  return tree;
}

wxString wxMaxima::EscapeForLisp(wxString str)
{
  str.Replace(wxT("\\"), wxT("\\\\"));
  str.Replace(wxT("\""), wxT("\\\""));
  return(str);
}

void wxMaxima::SetupVariables()
{
  SendMaxima(wxT(":lisp-quiet (setf *prompt-suffix* \"") +
             m_promptSuffix +
             wxT("\")\n"));
  SendMaxima(wxT(":lisp-quiet (setf *prompt-prefix* \"") +
             m_promptPrefix +
             wxT("\")\n"));
  SendMaxima(wxT(":lisp-quiet (setf $in_netmath nil)\n"));
  SendMaxima(wxT(":lisp-quiet (setf $show_openplot t)\n"));

  wxString cmd;
  wxString wxMathMl = ";; wxMaxima xml format (based on David Drysdale MathML printing)\n"
    ";; Andrej Vodopivec,  2004-2014\n"
    ";; Gunter Königsmann, 2014-2018\n"
    ";;  SPDX-License-Identifier: GPL-2.0+\n"
    "\n"
    ";; MathML-printing\n"
    ";; Created by David Drysdale (DMD), December 2002/January 2003\n"
    ";;\n"
    ";; closely based on the original TeX conversion code in mactex.lisp,\n"
    ";; for which the following credits apply:\n"
    ";;   (c) copyright 1987, Richard J. Fateman\n"
    ";;   small corrections and additions: Andrey Grozin, 2001\n"
    ";;   additional additions: Judah Milgram (JM), September 2001\n"
    ";;   additional corrections: Barton Willis (BLW), October 2001\n"
    ";; Method:\n"
    "\n"
    ";; Producing wxml from a Maxima internal expression is done by\n"
    ";; a reversal of the parsing process.  Fundamentally, a\n"
    ";; traversal of the expression tree is produced by the program,\n"
    ";; with appropriate substitutions and recognition of the\n"
    ";; infix / prefix / postfix / matchfix relations on symbols. Various\n"
    ";; changes are made to this so that MathML will like the results.\n"
    "\n"
    ";(format t \"<wxxml-start/>\")\n"
    "\n"
    ";; This is necessary to make file and directory names that contain special characters\n"
    ";; work under windows.\n"
    "(progn\n"
    "  #+sbcl (setf sb-impl::*default-external-format* :UTF-8)\n"
    "\n"
    "  (in-package :maxima)\n"
    "\n"
    "  (declare-top\n"
    "   (special lop rop $inchar)\n"
    "   (*expr wxxml-lbp wxxml-rbp))\n"
    "\n"
    "  ;; Use rounded parenthesis for matrices by default\n"
    "  (setq $lmxchar #\\()\n"
    "  (setq $rmxchar #\\()\n"
    "\n"
    "  ;; A few variables whose value can be configured from wxMaxima\n"
    "  (defvar *wx-plot-num* 0 \"The serial number of the current plot\")\n"
    "  (defvar $wxfilename \"\" \"The filename of the current wxMaxima worksheet\")\n"
    "  (defvar $wxdirname \"\" \"The directory the current wxMaxima worksheet lies in\")\n"
    "  (defvar $wxanimate_autoplay nil \"Automatically playback new animations?\")\n"
    "  (defvar wxUserConfDir \"\" \"The location wxMaxima looks for maxima\'s config files in\")\n"
    "  (defvar wxHelpDir \"\" \"The location wxMaxima searches for help files in\")\n"
    "  (defvar wxMaximaLispLocation \"\" \"The location wxMaxima searches for lisp files in\")\n"
    "  (defvar $wxplot_size \'((mlist simp) 800 600) \"The size of new plots\")\n"
    "  (defvar $wxchangedir t \"Change the PWD of maxima to the current document\'s path?\")\n"
    "  (defvar $wxmaximaversion t \"The wxMaxima version\")\n"
    "  (defprop $wxmaximaversion read-only-assign assign)\n"
    "  (defvar $wxwidgetsversion t \"The wxWidgets version wxMaxima is using.\")\n"
    "  (defvar $wxsubscripts t\n"
    "    \"Recognize TeX-style subscripts\")\n"
    "  (defvar $wxplot_pngcairo nil \"Use gnuplot\'s pngcairo terminal for new plots?\")\n"
    "  (defmvar $wxplot_old_gnuplot nil)\n"
    "\n"
    "\n"
    "  ;; Escape all chars that need escaping in XML\n"
    "  (defun wxxml-fix-string (x)\n"
    "    (if (stringp x)\n"
    "	(let* ((tmp-x (string-substitute \"&amp;\" #\\& x))\n"
    "	       (tmp-x (string-substitute \"&lt;\" #\\< tmp-x))\n"
    "	       (tmp-x (string-substitute \"&gt;\" #\\> tmp-x))\n"
    "	       (tmp-x (string-substitute \"&#13;\" #\\Return tmp-x))\n"
    "	       (tmp-x (string-substitute \"&#13;\" #\\Linefeed tmp-x))\n"
    "	       (tmp-x (string-substitute \"&#13;\" #\\Newline tmp-x)))\n"
    "	  tmp-x)\n"
    "      x))\n"
    "\n"/*
    "  ;; Allow the user to communicate what to display in the statusbar whilst\n"
    "  ;; the current program is running\n"
    "  (defun $wxstatusbar (&rest status)\n"
    "    (format t \"<statusbar>~a</statusbar>~%\" (wxxml-fix-string\n"
    "					     (apply \'$sconcat status))))\n"
    "\n"
    "\n"
    ";;; Without this command encountering unicode characters might cause\n"
    ";;; Maxima to stop responding on windows.\n"
    "  #+(and clisp win32) (setf (stream-external-format *socket-connection*) charset:utf-8)\n"
    "  #+(and clisp win32) (setf custom:*default-file-encoding* charset:utf-8)\n"
    "\n"
    ";;; Muffle compiler-notes globally\n"
    "  #+sbcl (declaim (sb-ext:muffle-conditions sb-ext:compiler-note))\n"
    "  (defmacro no-warning (form)\n"
    "    #+sbcl `(handler-bind\n"
    "		((style-warning #\'muffle-warning)\n"
    "		 (sb-ext:compiler-note #\'muffle-warning))\n"
    "	      ,form)\n"
    "    #+clisp `(let ((custom:*suppress-check-redefinition* t)) ,form)\n"
    "    #-(or sbcl clisp) `(progn ,form))\n"
    "\n"
    "  (defun read-wxmaxima-version (v)\n"
    "    (let* ((d1 (position #\\. v))\n"
    "	   (year (subseq v 0 d1))\n"
    "	   (d2 (position #\\. v :start (1+ d1)))\n"
    "	   (month (subseq v (1+ d1) d2))\n"
    "	   (rest (subseq v (1+ d2))))\n"
    "      (list \'(mlist simp) (parse-integer year) (parse-integer month) rest)))\n"
    "\n"
    "  (defun $wxbuild_info ()\n"
    "    (let ((year (sixth cl-user:*maxima-build-time*))\n"
    "	  (month (fifth cl-user:*maxima-build-time*))\n"
    "	  (day (fourth cl-user:*maxima-build-time*))\n"
    "	  (hour (third cl-user:*maxima-build-time*))\n"
    "	  (minute (second cl-user:*maxima-build-time*))\n"
    "	  (seconds (first cl-user:*maxima-build-time*)))\n"
    "      (format t \"wxMaxima version: ~a~%\" $wxmaximaversion)\n"
    "      (format t \"using wxWidgets version: ~a~%\" $wxwidgetsversion)\n"
    "      (format t \"Maxima version: ~a~%\" *autoconf-version*)\n"
    "      (format t \"Maxima build date: ~4,\'0d-~2,\'0d-~2,\'0d ~2,\'0d:~2,\'0d:~2,\'0d~%\"\n"
    "	      year month day hour minute seconds)\n"
    "      (format t \"Host type: ~a~%\" *autoconf-host*)\n"
    "      (format t \"System type: ~a ~a ~a~%\" (software-type) (software-version) (machine-type))\n"
    "      (format t \"Lisp implementation type: ~a~%\" (lisp-implementation-type))\n"
    "      (format t \"Lisp implementation version: ~a~%\" (lisp-implementation-version))\n"
    "      (format t \"~%~%wxMaxima\'s idea of the directory layout is:~%User configuration dir: ~a~%\" wxUserConfDir)\n"
    "      (format t \"Help dir: ~a~%\" wxHelpDir)\n"
    "      (format t \"Maxima lisp dir: ~a~%\" wxMaximaLispLocation))\n"
    "    \"\")\n"
    "\n"
    "  (defmfun $wxbug_report ()\n"
    "    (format t \"wxMaxima is a graphical front end for Maxima, which does the mathematics in the background.~%\")\n"
    "    (format t \"If you encounter a mathematical problem, it is probably a Maxima bug und should be submitted there.~%\")\n"
    "    (format t \"~%The Maxima bug database is available at~%\")\n"
    "    (format t \"    https://sourceforge.net/p/maxima/bugs~%\")\n"
    "    (format t \"Submit bug reports by following the \'Create Ticket\' link on that page.~%\")\n"
    "    (format t \"To report a Maxima bug, you must have a Sourceforge account.~%~%\")\n"
    "    (format t \"A problem in the graphical user interface is probably a wxMaxima bug.~%\")\n"
    "    (format t \"The wxMaxima bug database is available at~%\")\n"
    "    (format t \"    https://github.com/andrejv/wxmaxima/issues?direction=desc&sort=created&state=open~%\")\n"
    "    (format t \"Submit bug reports by following the \'New issue\' link on that page.~%~%\")\n"
    "    (format t \"Please check before submitting, if your bug was already reported.~%~%\")\n"
    "    (format t \"Please include the following information with your bug report:~%\")\n"
    "    (format t \"-------------------------------------------------------------~%\")\n"
    "    ($wxbuild_info)\n"
    "    (format t \"-------------------------------------------------------------~%\"))\n"
    "\n"
    "  (defvar *var-tag* \'(\"<v>\" \"</v>\"))\n"
    "\n"
    "  (defun wxxml-get (x p)\n"
    "    (if (symbolp x) (get x p)))\n"
    "\n"
    "  (defun wxxml-array (x l r &aux f)\n"
    "    (if (eq \'mqapply (caar x))\n"
    "	(setq f (cadr x)\n"
    "	      x (cdr x)\n"
    "	      l (wxxml f (append l (list \"<i><p>\")) (list \"</p>\")\n"
    "		       \'mparen \'mparen))\n"
    "      (setq f (caar x)\n"
    "	    l (wxxml f (append l \'(\"<i><r>\"))\n"
    "		     (list \"</r>\") lop \'mfunction)))\n"
    "    (setq r (nconc (wxxml-list (cdr x) (list \"<r>\")\n"
    "			       (list \"</r></i>\") \"<v>,</v>\") r))\n"
    "    (nconc l r))\n"
    "\n"
    "  (defmacro make-tag (val tag)\n"
    "    ``((wxxmltag simp) ,,val ,,tag))\n"
    "\n"
    "  (defun $wxxmltag (val tag)\n"
    "    (make-tag ($sconcat val) ($sconcat tag)))\n"
    "\n"
    "  (defun string-substitute (newstring oldchar x &aux matchpos)\n"
    "    (setq matchpos (position oldchar x))\n"
    "    (if (null matchpos) x\n"
    "      (concatenate \'string\n"
    "		   (subseq x 0 matchpos)\n"
    "		   newstring\n"
    "		   (string-substitute newstring oldchar\n"
    "				      (subseq x (1+ matchpos))))))\n"
    "\n"
    ";;; First we have the functions which are called directly by wxxml and its\n"
    ";;; descendants\n"
    "  (defun $wxdeclare_subscript (x &optional (opt t))\n"
    "    (unless (listp x)\n"
    "      (setq x (list \'(mlist simp) x)))\n"
    "    (dolist (s (cdr x))\n"
    "      ($put s opt \'$wxxml_subscript))\n"
    "    opt)\n"
    "\n"
    "  (defun $wxdeclare_subscripted (x &optional (opt t))\n"
    "    (unless (listp x)\n"
    "      (setq x (list \'(mlist simp) x)))\n"
    "    (dolist (s (cdr x))\n"
    "      ($put s opt \'$wxxml_subscripted))\n"
    "    opt)\n"
    "\n"
    "  (defun subscriptp (x)\n"
    "    (unless (symbolp x)\n"
    "      (return-from subscriptp x))\n"
    "    (let* ((name (subseq (maybe-invert-string-case (symbol-name x)) 1))\n"
    "	   (pos (search \"_\" name :from-end t))\n"
    "	   #-gcl (*readtable* (copy-readtable nil)))\n"
    "      #-gcl (setf (readtable-case *readtable*) :invert)\n"
    "      (when pos\n"
    "	(let* ((sub (subseq name (+ pos 1)))\n"
    "	       (sub-var (subseq name 0 pos))\n"
    "	       (sub-var-symb (read-from-string (concatenate \'string \"$\" sub-var)))\n"
    "	       (sub-symb (read-from-string (concatenate \'string \"$\" sub)))\n"
    "	       (sub-int (ignore-errors\n"
    "			  (parse-integer sub))))\n"
    "	  (when (and (> (length sub-var) 0)\n"
    "		     (or sub-int\n"
    "			 (eq $wxsubscripts \'$all)\n"
    "			 (= (length sub) 1)\n"
    "			 (= (length sub-var) 1)\n"
    "			 ($get x \'$wxxml_subscripted)\n"
    "			 ($get sub-symb \'$wxxml_subscript)))\n"
    "	    (format nil  \"<i altCopy=\\\"~{~a~}\\\"><r>~a</r><r>~a</r></i>\"\n"
    "		    (mstring x)\n"
    "		    (or (get sub-var-symb \'wxxmlword)\n"
    "			(format nil \"<v>~a</v>\" sub-var))\n"
    "		    (if sub-int\n"
    "			(format nil \"<n>~a</n>\" sub-int)\n"
    "                      (format nil \"<v>~a</v>\" sub))))))))\n"
    "\n"
    "  (defun wxxmlnumformat (atom)\n"
    "    (let (r firstpart exponent)\n"
    "      (cond ((integerp atom)\n"
    "	     (format nil \"<n>~{~c~}</n>\" (exploden atom)))\n"
    "	    (t\n"
    "	     (setq r (exploden atom))\n"
    "	     (setq exponent (member \'e r :test #\'string-equal))\n"
    "	     (cond ((null exponent)\n"
    "		    (format nil \"<n>~{~c~}</n>\" r))\n"
    "		   (t\n"
    "		    (setq firstpart\n"
    "			  (nreverse (cdr (member \'e (reverse r)\n"
    "						 :test #\'string-equal))))\n"
    "		    (if (char= (cadr exponent) #\\+)\n"
    "			(setq exponent (cddr exponent))\n"
    "		      (setq exponent (cdr exponent)))\n"
    "		    (format nil\n"
    "			    \"<r><n>~{~c~}</n><h>*</h><e><n>10</n><n>~{~c~}</n></e></r>\"\n"
    "			    firstpart exponent)))))))\n"
    "\n"
    "  (defun wxxml-stripdollar (sym &aux pname)\n"
    "    (or (symbolp sym)\n"
    "	(return-from wxxml-stripdollar\n"
    "		     (wxxml-fix-string (format nil \"~a\" sym))))\n"
    "    (setq pname (maybe-invert-string-case (symbol-name sym)))\n"
    "    (setq pname (cond ((and (> (length pname) 0)\n"
    "			    (member (elt pname 0) \'(#\\$ #\\&) :test #\'eq))\n"
    "		       (subseq pname 1))\n"
    "		      ((and (> (length pname) 0)\n"
    "			    (equal (elt pname 0) #\\%))\n"
    "		       (if $noundisp\n"
    "			   (concatenate \'string \"\'\"\n"
    "					(subseq pname 1))\n"
    "			 (subseq pname 1)))\n"
    "		      ($lispdisp\n"
    "		       (concatenate \'string \"?\" pname))\n"
    "		      (t pname)))\n"
    "    (setq pname (wxxml-fix-string pname))\n"
    "    (concatenate \'string (car *var-tag*) pname (cadr *var-tag*)))\n"
    "\n"
    "  (defun wxxml-atom (x l r &aux tmp-x)\n"
    "    (append l\n"
    "	    (list (cond ((numberp x) (wxxmlnumformat x))\n"
    "			((and (symbolp x) (get x \'wxxmlword)))\n"
    "			((and (symbolp x) (get x \'reversealias))\n"
    "			 (wxxml-stripdollar (get x \'reversealias)))\n"
    "			((stringp x)\n"
    "			 (setq tmp-x (wxxml-fix-string x))\n"
    "			 (if (and (boundp \'$stringdisp) $stringdisp)\n"
    "			     (setq tmp-x (format nil \"\\\"~a\\\"\" tmp-x)))\n"
    "			 (concatenate \'string \"<st>\" tmp-x \"</st>\"))\n"
    "			((arrayp x)\n"
    "			 (format nil \"<v>#{Lisp array [~{~a~^,~}]}</v>\"\n"
    "				 (array-dimensions x)))\n"
    "			((functionp x)\n"
    "			 (format nil \"<v>~a</v>\"\n"
    "				 (wxxml-fix-string\n"
    "				  (stripdollar\n"
    "				   (maybe-invert-string-case (format nil \"~A\" x))))))\n"
    "			((streamp x)\n"
    "			 (format nil \"<v>#{Stream [~A]</v>}\"\n"
    "				 (stream-element-type x)))\n"
    "			((member (type-of x) \'(GRAPH DIGRAPH))\n"
    "			 (format nil \"<v>~a</v>\" x))\n"
    "			((typep x \'structure-object)\n"
    "			 (let ((tmp-string (format nil \"~s\" x)))\n"
    "			   (format nil \"<st>~a</st>\" (wxxml-fix-string tmp-string))))\n"
    "			((hash-table-p x)\n"
    "			 (format nil \"<v>#{HashTable}</v>\"))\n"
    "			((and $wxsubscripts (subscriptp x)))\n"
    "			(t (wxxml-stripdollar x))))\n"
    "	    r))\n"
    "\n"
    "  ;; we could patch this so sin x rather than sin(x), but instead we made\n"
    "  ;; sin a prefix operator\n"
    "  (defun wxxml-function (x l r)\n"
    "    (setq l\n"
    "	  (let ((*var-tag* \'(\"<fnm>\" \"</fnm>\")))\n"
    "	    (wxxml (caar x) (append l \'(\"<fn>\"))\n"
    "		   nil \'mparen \'mparen))\n"
    "	  r (wxxml (cons \'(mprogn) (cdr x)) nil (append \'(\"</fn>\") r)\n"
    "		   \'mparen \'mparen))\n"
    "    (append l r))\n"
    "\n"
    "  (defun wxxml-defstruct (x l r)\n"
    "    (let ((L1 (cdr (get (caar x) \'defstruct-template)))\n"
    "	  (L2 (cdr x)))\n"
    "      (wxxml-function\n"
    "       (cons (car x)\n"
    "	     (mapcar #\'(lambda (e1 e2) (if (eq e1 e2) e1 `((mequal) ,e1 ,e2))) L1 L2))\n"
    "       l r)))\n"
    "\n"
    "  (defun wxxml-matchfix-dim (x l r)\n"
    "    (setq l (append l\n"
    "		    (list (wxxml-dissym-to-string (car (get (caar x) \'dissym)))))\n"
    "	  r (append (list (wxxml-dissym-to-string (cdr (get (caar x) \'dissym))))\n"
    "		    r)\n"
    "	  x (wxxml-list (cdr x) nil r \"<t>,</t>\"))\n"
    "    (append l x))\n"
    "\n"
    "  (defun wxxml-nary (x l r)\n"
    "    (let* ((op (caar x))\n"
    "	   (sym (cond ((member op \'(mtimes wxtimes) :test #\'eq)\n"
    "		       (if $stardisp\n"
    "			   \"<t>*</t>\"\n"
    "			 \"<h>*</h>\"))\n"
    "					;((wxxmlsym op))\n"
    "		      ((eq (get op \'dimension) \'dimension-nary)\n"
    "		       (wxxml-dissym-to-string (get op \'dissym)))))\n"
    "	   (y (cdr x))\n"
    "	   (ext-lop lop)\n"
    "	   (ext-rop rop))\n"
    "      (cond ((null y)\n"
    "	     (wxxml-function x l r)) ; this should not happen\n"
    "	    ((null (cdr y))\n"
    "	     (wxxml-function x l r)) ; this should not happen, too\n"
    "	    (t (do ((nl) (lop ext-lop op)\n"
    "		    (rop op (if (null (cdr y)) ext-rop op)))\n"
    "		   ((null (cdr y))\n"
    "		    (setq nl (nconc nl (wxxml (car y) l r lop rop))) nl)\n"
    "		   (setq nl (nconc nl (wxxml (car y) l (list sym) lop rop))\n"
    "			 y (cdr y)\n"
    "			 l nil))))))\n"
    "\n"
    "  (defun wxxml (x l r lop rop)\n"
    "    ;; x is the expression of interest; l is the list of strings to its\n"
    "    ;; left, r to its right. lop and rop are the operators on the left\n"
    "    ;; and right of x in the tree, and will determine if parens must\n"
    "    ;; be inserted\n"
    "    (setq x (nformat x))\n"
    "    (cond ((atom x) (wxxml-atom x l r))\n"
    "	  ((not (listp (car x)))\n"
    "	   (wxxml (cons \'(mlist simp) x) l r lop rop))\n"
    "	  ((or (<= (wxxml-lbp (caar x)) (wxxml-rbp lop))\n"
    "	       (> (wxxml-lbp rop) (wxxml-rbp (caar x))))\n"
    "	   (wxxml-paren x l r))\n"
    "	  ;; special check needed because macsyma notates arrays peculiarly\n"
    "	  ((member \'array (cdar x) :test #\'eq) (wxxml-array x l r))\n"
    "	  ;; dispatch for object-oriented wxxml-ifiying\n"
    "	  ((wxxml-get (caar x) \'wxxml) (funcall (get (caar x) \'wxxml) x l r))\n"
    "	  ((equal (wxxml-get (caar x) \'dimension) \'dimension-infix)\n"
    "	   (wxxml-infix x l r))\n"
    "	  ((equal (wxxml-get (caar x) \'dimension) \'dimension-match)\n"
    "	   (wxxml-matchfix-dim x l r))\n"
    "	  ((equal (wxxml-get (caar x) \'dimension) \'dimension-nary)\n"
    "	   (wxxml-nary x l r))\n"
    "	  ((equal (wxxml-get (caar x) \'dimension) \'dimension-postfix)\n"
    "	   (wxxml-postfix x l r))\n"
    "	  ((wxxml-get (caar x) \'defstruct-template)\n"
    "	   (wxxml-defstruct x l r))\n"
    "	  (t (wxxml-function x l r))))\n"
    "\n"
    "  (defun wxxml-paren (x l r)\n"
    "    (wxxml x (append l \'(\"<r><p>\")) (cons \"</p></r>\" r) \'mparen \'mparen))\n"
    "\n"
    "  ;; set up a list , separated by symbols (, * ...)  and then tack on the\n"
    "  ;; ending item (e.g. \"]\" or perhaps \")\"\n"
    "  (defun wxxml-list (x l r sym)\n"
    "    (if (null x) r\n"
    "      (do ((nl))\n"
    "	  ((null (cdr x))\n"
    "	   (setq nl (nconc nl (wxxml (car x)  l r \'mparen \'mparen)))\n"
    "	   nl)\n"
    "	  (setq nl (nconc nl (wxxml (car x)  l (list sym) \'mparen \'mparen))\n"
    "		x (cdr x)\n"
    "		l nil))))\n"
    "\n"
    ";;; Now we have functions which are called via property lists\n"
    "\n"
    "  (defun wxxml-prefix (x l r)\n"
    "    (wxxml (cadr x) (append l (wxxmlsym (caar x))) r (caar x) rop))\n"
    "\n"
    "  (defun wxxml-infix (x l r)\n"
    "    ;; check for 2 args\n"
    "    (if (or (null (cddr x)) (cdddr x)) (wna-err (caar x)))\n"
    "    (setq l (wxxml (cadr x) l nil lop (caar x)))\n"
    "    (wxxml (caddr x) (append l (wxxmlsym (caar x))) r (caar x) rop))\n"
    "\n"
    "  (defun wxxml-postfix (x l r)\n"
    "    (wxxml (cadr x) l (append (wxxmlsym (caar x)) r) lop (caar x)))\n"
    "\n"
    "  (defun wxxml-nofix (x l r) (wxxml (caar x) l r (caar x) rop))\n"
    "\n"
    "  (defun wxxml-matchfix (x l r)\n"
    "    (setq l (append l (car (wxxmlsym (caar x))))\n"
    "	  ;; car of wxxmlsym of a matchfix operator is the lead op\n"
    "	  r (append (cdr (wxxmlsym (caar x))) r)\n"
    "	  ;; cdr is the trailing op\n"
    "	  x (wxxml-list (cdr x) nil r \"<t>,</t>\"))\n"
    "    (append l x))\n"
    "\n"
    "\n"
    "  (defun wxxml-dissym-to-string (lst &aux pname)\n"
    "    (setq pname\n"
    "	  (wxxml-fix-string (format nil \"~{~a~}\" lst)))\n"
    "    (concatenate \'string \"<v>\" pname \"</v>\"))\n"
    "\n"
    "  (defun wxxmlsym (x)\n"
    "    (or (get x \'wxxmlsym)\n"
    "	(get x \'strsym)\n"
    "	(and (get x \'dissym)\n"
    "	     (list (wxxml-dissym-to-string (get x \'dissym))))\n"
    "	(list (stripdollar x))))\n"
    "\n"
    "  (defun wxxmlword (x)\n"
    "    (or (get x \'wxxmlword)\n"
    "	(stripdollar x)))\n"
    "\n"
    "  (defprop bigfloat wxxml-bigfloat wxxml)\n"
    "\n"
    "  ;;(defun mathml-bigfloat (x l r) (declare (ignore l r)) (fpformat x))\n"
    "  (defun wxxml-bigfloat (x l r)\n"
    "    (append l \'(\"<n>\") (fpformat x) \'(\"</n>\") r))\n"
    "\n"
    "  (defprop mprog  \"<fnm>block</fnm>\" wxxmlword)\n"
    "  (defprop $true  \"<t>true</t>\"  wxxmlword)\n"
    "  (defprop $false \"<t>false</t>\" wxxmlword)\n"
    "\n"
    "  (defprop mprogn wxxml-matchfix wxxml)\n"
    "  (defprop mprogn ((\"<r><p>\") \"</p></r>\") wxxmlsym)\n"
    "\n"
    "  (defprop mlist wxxml-matchfix wxxml)\n"
    "  (defprop mlist ((\"<r><t>[</t>\")\"<t>]</t></r>\") wxxmlsym)\n"
    "\n"
    "  (defprop $set wxxml-matchfix wxxml)\n"
    "  (defprop $set ((\"<r><t>{</t>\")\"<t>}</t></r>\") wxxmlsym)\n"
    "\n"
    "  (defprop mabs wxxml-matchfix wxxml)\n"
    "  (defprop mabs ((\"<r><a>\")\"</a></r>\") wxxmlsym)\n"
    "\n"
    "  (defprop $conjugate wxxml-matchfix wxxml)\n"
    "  (defprop $conjugate ((\"<r><cj>\")\"</cj></r>\") wxxmlsym)\n"
    "\n"
    "  (defprop %conjugate wxxml-matchfix wxxml)\n"
    "  (defprop %conjugate ((\"<r><cj>\")\"</cj></r>\") wxxmlsym)\n"
    "\n"
    "  (defprop mbox wxxml-mbox wxxml)\n"
    "  (defprop mlabox wxxml-mbox wxxml)\n"
    "\n"
    "  (defprop mbox 10. wxxml-rbp)\n"
    "  (defprop mbox 10. wxxml-lbp)\n"
    "\n"
    "  (defprop mlabbox 10. wxxml-rbp)\n"
    "  (defprop mlabbox 10. wxxml-lbp)\n"
    "\n"
    "  (defun wxxml-mbox (x l r)\n"
    "    (setq l (wxxml (cadr x) (append l \'(\"<r><hl>\")) nil \'mparen \'mparen)\n"
    "	  r (append \'(\"</hl></r>\") r))\n"
    "    (append l r))\n"
    "\n"
    "  (defprop mqapply wxxml-mqapply wxxml)\n"
    "\n"
    "  (defun wxxml-mqapply (x l r)\n"
    "    (setq l (wxxml (cadr x) (append l \'(\"<fn>\"))\n"
    "		   (list \"<p>\" ) lop \'mfunction)\n"
    "	  r (wxxml-list (cddr x) nil (cons \"</p></fn>\" r) \"<t>,</t>\"))\n"
    "    (append l r))\n"
    "\n"
    "\n"
    "  (defprop $zeta \"<g>zeta</g>\" wxxmlword)\n"
    "  (defprop %zeta \"<g>zeta</g>\" wxxmlword)\n"
    "\n"
    "  ;;\n"
    "  ;; Greek characters\n"
    "  ;;\n"
    "  (defprop $%alpha \"<g>%alpha</g>\" wxxmlword)\n"
    "  (defprop $alpha \"<g>alpha</g>\" wxxmlword)\n"
    "  (defprop $%beta \"<g>%beta</g>\" wxxmlword)\n"
    "  (defprop $beta \"<g>beta</g>\" wxxmlword)\n"
    "  (defprop $%gamma \"<g>%gamma</g>\" wxxmlword)\n"
    "  (defprop %gamma \"<g>gamma</g>\" wxxmlword)\n"
    "  (defprop $%delta \"<g>%delta</g>\" wxxmlword)\n"
    "  (defprop $delta \"<g>delta</g>\" wxxmlword)\n"
    "  (defprop $%epsilon \"<g>%epsilon</g>\" wxxmlword)\n"
    "  (defprop $epsilon \"<g>epsilon</g>\" wxxmlword)\n"
    "  (defprop $%zeta \"<g>%zeta</g>\" wxxmlword)\n"
    "  (defprop $%eta \"<g>%eta</g>\" wxxmlword)\n"
    "  (defprop $eta \"<g>eta</g>\" wxxmlword)\n"
    "  (defprop $%theta \"<g>%theta</g>\" wxxmlword)\n"
    "  (defprop $theta \"<g>theta</g>\" wxxmlword)\n"
    "  (defprop $%iota \"<g>%iota</g>\" wxxmlword)\n"
    "  (defprop $iota \"<g>iota</g>\" wxxmlword)\n"
    "  (defprop $%kappa \"<g>%kappa</g>\" wxxmlword)\n"
    "  (defprop $kappa \"<g>kappa</g>\" wxxmlword)\n"
    "  (defprop $%lambda \"<g>%lambda</g>\" wxxmlword)\n"
    "  (defprop $lambda \"<g>lambda</g>\" wxxmlword)\n"
    "  (defprop $%mu \"<g>%mu</g>\" wxxmlword)\n"
    "  (defprop $mu \"<g>mu</g>\" wxxmlword)\n"
    "  (defprop $%nu \"<g>%nu</g>\" wxxmlword)\n"
    "  (defprop $nu \"<g>nu</g>\" wxxmlword)\n"
    "  (defprop $%xi \"<g>%xi</g>\" wxxmlword)\n"
    "  (defprop $xi \"<g>xi</g>\" wxxmlword)\n"
    "  (defprop $%omicron \"<g>%omicron</g>\" wxxmlword)\n"
    "  (defprop $omicron \"<g>omicron</g>\" wxxmlword)\n"
    "  (defprop $%pi \"<s>%pi</s>\" wxxmlword)\n"
    "  (defprop $pi \"<g>pi</g>\" wxxmlword)\n"
    "  (defprop $%rho \"<g>%rho</g>\" wxxmlword)\n"
    "  (defprop $rho \"<g>rho</g>\" wxxmlword)\n"
    "  (defprop $%sigma \"<g>%sigma</g>\" wxxmlword)\n"
    "  (defprop $sigma \"<g>sigma</g>\" wxxmlword)\n"
    "  (defprop $%tau \"<g>%tau</g>\" wxxmlword)\n"
    "  (defprop $tau \"<g>tau</g>\" wxxmlword)\n"
    "  (defprop $%upsilon \"<g>%upsilon</g>\" wxxmlword)\n"
    "  (defprop $upsilon \"<g>upsilon</g>\" wxxmlword)\n"
    "  (defprop $%phi \"<g>%phi</g>\" wxxmlword)\n"
    "  (defprop $phi \"<g>phi</g>\" wxxmlword)\n"
    "  (defprop $%chi \"<g>%chi</g>\" wxxmlword)\n"
    "  (defprop $chi \"<g>chi</g>\" wxxmlword)\n"
    "  (defprop $%psi \"<g>%psi</g>\" wxxmlword)\n"
    "  (defprop $psi \"<g>psi</g>\" wxxmlword)\n"
    "  (defprop $%omega \"<g>%omega</g>\" wxxmlword)\n"
    "  (defprop $omega \"<g>omega</g>\" wxxmlword)\n"
    "  (defprop |$%Alpha| \"<g>%Alpha</g>\" wxxmlword)\n"
    "  (defprop |$Alpha| \"<g>Alpha</g>\" wxxmlword)\n"
    "  (defprop |$%Beta| \"<g>%Beta</g>\" wxxmlword)\n"
    "  (defprop |$Beta| \"<g>Beta</g>\" wxxmlword)\n"
    "  (defprop |$%Gamma| \"<g>%Gamma</g>\" wxxmlword)\n"
    "  (defprop |$Gamma| \"<g>Gamma</g>\" wxxmlword)\n"
    "  (defprop |$%Delta| \"<g>%Delta</g>\" wxxmlword)\n"
    "  (defprop |$Delta| \"<g>Delta</g>\" wxxmlword)\n"
    "  (defprop |$%Epsilon| \"<g>%Epsilon</g>\" wxxmlword)\n"
    "  (defprop |$Epsilon| \"<g>Epsilon</g>\" wxxmlword)\n"
    "  (defprop |$%Zeta| \"<g>%Zeta</g>\" wxxmlword)\n"
    "  (defprop |$Zeta| \"<g>Zeta</g>\" wxxmlword)\n"
    "  (defprop |$%Eta| \"<g>%Eta</g>\" wxxmlword)\n"
    "  (defprop |$Eta| \"<g>Eta</g>\" wxxmlword)\n"
    "  (defprop |$%Theta| \"<g>%Theta</g>\" wxxmlword)\n"
    "  (defprop |$Theta| \"<g>Theta</g>\" wxxmlword)\n"
    "  (defprop |$%Iota| \"<g>%Iota</g>\" wxxmlword)\n"
    "  (defprop |$Iota| \"<g>Iota</g>\" wxxmlword)\n"
    "  (defprop |$%Kappa| \"<g>%Kappa</g>\" wxxmlword)\n"
    "  (defprop |$Kappa| \"<g>Kappa</g>\" wxxmlword)\n"
    "  (defprop |$%Lambda| \"<g>%Lambda</g>\" wxxmlword)\n"
    "  (defprop |$Lambda| \"<g>Lambda</g>\" wxxmlword)\n"
    "  (defprop |$%Mu| \"<g>%Mu</g>\" wxxmlword)\n"
    "  (defprop |$Mu| \"<g>Mu</g>\" wxxmlword)\n"
    "  (defprop |$%Nu| \"<g>%Nu</g>\" wxxmlword)\n"
    "  (defprop |$Nu| \"<g>Nu</g>\" wxxmlword)\n"
    "  (defprop |$%Xi| \"<g>%Xi</g>\" wxxmlword)\n"
    "  (defprop |$Xi| \"<g>Xi</g>\" wxxmlword)\n"
    "  (defprop |$%Omicron| \"<g>%Omicron</g>\" wxxmlword)\n"
    "  (defprop |$Omicron| \"<g>Omicron</g>\" wxxmlword)\n"
    "  (defprop |$%Rho| \"<g>%Rho</g>\" wxxmlword)\n"
    "  (defprop |$Rho| \"<g>Rho</g>\" wxxmlword)\n"
    "  (defprop |$%Sigma| \"<g>%Sigma</g>\" wxxmlword)\n"
    "  (defprop |$Sigma| \"<g>Sigma</g>\" wxxmlword)\n"
    "  (defprop |$%Tau| \"<g>%Tau</g>\" wxxmlword)\n"
    "  (defprop |$Tau| \"<g>Tau</g>\" wxxmlword)\n"
    "  (defprop |$%Upsilon| \"<g>%Upsilon</g>\" wxxmlword)\n"
    "  (defprop |$Upsilon| \"<g>Upsilon</g>\" wxxmlword)\n"
    "  (defprop |$%Phi| \"<g>%Phi</g>\" wxxmlword)\n"
    "  (defprop |$Phi| \"<g>Phi</g>\" wxxmlword)\n"
    "  (defprop |$%Chi| \"<g>%Chi</g>\" wxxmlword)\n"
    "  (defprop |$Chi| \"<g>Chi</g>\" wxxmlword)\n"
    "  (defprop |$%Psi| \"<g>%Psi</g>\" wxxmlword)\n"
    "  (defprop |$Psi| \"<g>Psi</g>\" wxxmlword)\n"
    "  (defprop |$%Omega| \"<g>%Omega</g>\" wxxmlword)\n"
    "  (defprop |$Omega| \"<g>Omega</g>\" wxxmlword)\n"
    "  (defprop |$%Pi| \"<g>%Pi</g>\" wxxmlword)\n"
    "  (defprop |$Pi| \"<g>Pi</g>\" wxxmlword)\n"
    "\n"
    "  (defprop $%i \"<s>%i</s>\" wxxmlword)\n"
    "  (defprop $%e \"<s>%e</s>\" wxxmlword)\n"
    "  (defprop $inf \"<s>inf</s>\" wxxmlword)\n"
    "  (defprop $minf \"<t>-</t><s>inf</s>\" wxxmlword)\n"
    "\n"
    "  (defprop mreturn \"return\" wxxmlword)\n"
    "\n"
    "  (defprop mquote wxxml-prefix wxxml)\n"
    "  (defprop mquote (\"<t>\'</t>\") wxxmlsym)\n"
    "  (defprop mquote \"<t>\'</t>\" wxxmlword)\n"
    "  (defprop mquote 201. wxxml-rbp)\n"
    "\n"
    "  (defprop msetq wxxml-infix wxxml)\n"
    "  (defprop msetq (\"<t>:</t>\") wxxmlsym)\n"
    "  (defprop msetq \"<t>:</t>\" wxxmlword)\n"
    "  (defprop msetq 180. wxxml-rbp)\n"
    "  (defprop msetq 20. wxxml-rbp)\n"
    "\n"
    "  (defprop mset wxxml-infix wxxml)\n"
    "  (defprop mset (\"<t>::</t>\") wxxmlsym)\n"
    "  (defprop mset \"<t>::</t>\" wxxmlword)\n"
    "  (defprop mset 180. wxxml-lbp)\n"
    "  (defprop mset 20. wxxml-rbp)\n"
    "\n"
    "  (defprop mdefine wxxml-infix wxxml)\n"
    "  (defprop mdefine (\"<t>:=</t>\") wxxmlsym)\n"
    "  (defprop mdefine \"<t>:=</t>\" wxxmlword)\n"
    "  (defprop mdefine 180. wxxml-lbp)\n"
    "  (defprop mdefine 20. wxxml-rbp)\n"
    "\n"
    "  (defprop mdefmacro wxxml-infix wxxml)\n"
    "  (defprop mdefmacro (\"<t>::=</t>\") wxxmlsym)\n"
    "  (defprop mdefmacro \"<t>::=</t>\" wxxmlword)\n"
    "  (defprop mdefmacro 180. wxxml-lbp)\n"
    "  (defprop mdefmacro 20. wxxml-rbp)\n"
    "\n"
    "  (defprop marrow wxxml-infix wxxml)\n"
    "  (defprop marrow (\"<t>-></t>\") wxxmlsym)\n"
    "  (defprop marrow \"<t>-></t>\" wxxmlword)\n"
    "  (defprop marrow 25 wxxml-lbp)\n"
    "  (defprop marrow 25 wxxml-rbp)\n"
    "\n"
    "  (defprop mfactorial wxxml-postfix wxxml)\n"
    "  (defprop mfactorial (\"<t>!</t>\") wxxmlsym)\n"
    "  (defprop mfactorial \"<t>!</t>\" wxxmlword)\n"
    "  (defprop mfactorial 160. wxxml-lbp)\n"
    "\n"
    "  (defprop mexpt wxxml-mexpt wxxml)\n"
    "  (defprop mexpt 140. wxxml-lbp)\n"
    "  (defprop mexpt 139. wxxml-rbp)\n"
    "\n"
    "  (defprop %sum 90. wxxml-rbp)\n"
    "  (defprop %product 95. wxxml-rbp)\n"
    "\n"
    "  ;; insert left-angle-brackets for mncexpt. a^<t> is how a^^n looks.\n"
    "\n"
    "  (defun wxxml-mexpt (x l r)\n"
    "    (cond ((atom (cadr x))\n"
    "	   (wxxml-mexpt-simple x l r))\n"
    "	  ((member \'array (caadr x))\n"
    "	   (wxxml-mexpt-array x l r))\n"
    "	  (t\n"
    "	   (wxxml-mexpt-simple x l r))))\n"
    "\n"
    "  (defun wxxml-mexpt-array (x l r)\n"
    "    (let* ((nc (eq (caar x) \'mncexpt))\n"
    "	   f (xarr (cadr x))\n"
    "	   (xexp (nformat (caddr x))))\n"
    "      ;; the index part\n"
    "      (if (eq \'mqapply (caar xarr))\n"
    "	  (setq f (cadr xarr)\n"
    "		xarr (cdr xarr)\n"
    "		l (wxxml f (append l (list \"<ie><p>\")) (list \"</p>\")\n"
    "			 \'mparen \'mparen))\n"
    "	(setq f (caar xarr)\n"
    "	      l (wxxml f (append l (if nc\n"
    "				       (list \"<ie mat=\\\"true\\\"><r>\")\n"
    "				     (list \"<ie><r>\")))\n"
    "		       (list \"</r>\") lop \'mfunction)))\n"
    "      (setq  l (append l (wxxml-list (cdr xarr) (list \"<r>\")\n"
    "				     (list \"</r>\") \"<v>,</v>\")))\n"
    "      ;; The exponent part\n"
    "      (setq r (if (mmminusp xexp)\n"
    "		  ;; the change in base-line makes parens unnecessary\n"
    "		  (wxxml (cadr xexp) \'(\"<r><v>-</v>\")\n"
    "			 (cons \"</r></ie>\" r) \'mparen \'mparen)\n"
    "		(if (and (integerp xexp) (< xexp 10))\n"
    "		    (wxxml xexp nil\n"
    "			   (cons \"</ie>\" r) \'mparen \'mparen)\n"
    "		  (wxxml xexp (list \"<r>\")\n"
    "			 (cons \"</r></ie>\" r) \'mparen \'mparen)\n"
    "		  )))\n"
    "      (append l r)))\n"
    "\n"
    "  (defun wxxml-mexpt-simple (x l r)\n"
    "    (let((nc (eq (caar x) \'mncexpt)))\n"
    "      (setq l (wxxml (cadr x) (append l (if nc\n"
    "					    \'(\"<e mat=\\\"true\\\"><r>\")\n"
    "					  \'(\"<e><r>\")))\n"
    "		     nil lop (caar x))\n"
    "	    r (if (mmminusp (setq x (nformat (caddr x))))\n"
    "		  ;; the change in base-line makes parens unnecessary\n"
    "		  (wxxml (cadr x) \'(\"</r><r><v>-</v>\")\n"
    "			 (cons \"</r></e>\" r) \'mminus \'mminus)\n"
    "		(if (and (integerp x) (< x 10))\n"
    "		    (wxxml x (list \"</r>\")\n"
    "			   (cons \"</e>\" r) \'mparen \'mparen)\n"
    "		  (wxxml x (list \"</r><r>\")\n"
    "			 (cons \"</r></e>\" r) \'mparen \'mparen)\n"
    "		  )))\n"
    "      (append l r)))\n"
    "\n"
    "  (defprop mncexpt wxxml-mexpt wxxml)\n"
    "\n"
    "  (defprop mncexpt 135. wxxml-lbp)\n"
    "  (defprop mncexpt 134. wxxml-rbp)\n"
    "\n"
    "  (defprop mnctimes wxxml-nary wxxml)\n"
    "  (defprop mnctimes \"<t>.</t>\" wxxmlsym)\n"
    "  (defprop mnctimes \"<t>.</t>\" wxxmlword)\n"
    "  (defprop mnctimes 110. wxxml-lbp)\n"
    "  (defprop mnctimes 109. wxxml-rbp)\n"
    "\n"
    "  (defprop mtimes wxxml-nary wxxml)\n"
    "  (defprop mtimes \"<h>*</h>\" wxxmlsym)\n"
    "  (defprop mtimes \"<t>*</t>\" wxxmlword)\n"
    "  (defprop mtimes 120. wxxml-lbp)\n"
    "  (defprop mtimes 120. wxxml-rbp)\n"
    "\n"
    "  (defprop wxtimes wxxml-nary wxxml)\n"
    "  (defprop wxtimes \"<h>*</h>\" wxxmlsym)\n"
    "  (defprop wxtimes \"<t>*</t>\" wxxmlword)\n"
    "  (defprop wxtimes 120. wxxml-lbp)\n"
    "  (defprop wxtimes 120. wxxml-rbp)\n"
    "\n"
    "  (defprop %sqrt wxxml-sqrt wxxml)\n"
    "\n"
    "  (defun wxxml-sqrt (x l r)\n"
    "    (wxxml (cadr x) (append l  \'(\"<q>\"))\n"
    "	   (append \'(\"</q>\") r) \'mparen \'mparen))\n"
    "\n"
    "  (defprop mquotient wxxml-mquotient wxxml)\n"
    "  (defprop mquotient (\"<t>/</t>\") wxxmlsym)\n"
    "  (defprop mquotient \"<t>/</t>\" wxxmlword)\n"
    "  (defprop mquotient 122. wxxml-lbp) ;;dunno about this\n"
    "  (defprop mquotient 123. wxxml-rbp)\n"
    "\n"
    "  (defun wxxml-mquotient (x l r)\n"
    "    (if (or (null (cddr x)) (cdddr x)) (wna-err (caar x)))\n"
    "    (setq l (wxxml (cadr x) (append l \'(\"<f><r>\")) nil \'mparen \'mparen)\n"
    "	  r (wxxml (caddr x) (list \"</r><r>\")\n"
    "		   (append \'(\"</r></f>\")r) \'mparen \'mparen))\n"
    "    (append l r))\n"
    "\n"
    "  (defprop $matrix wxxml-matrix-test wxxml)\n"
    "\n"
    "  (defun wxxml-matrix-test (x l r)\n"
    "    (if (every #\'$listp (cdr x))\n"
    "	(wxxml-matrix x l r)\n"
    "      (wxxml-function x l r)))\n"
    "\n"
    "  (defun wxxml-matrix(x l r) ;;matrix looks like ((mmatrix)((mlist) a b) ...)\n"
    "    (cond ((null (cdr x))\n"
    "	   (append l `(\"<fn><fnm>matrix</fnm><p/></fn>\") r))\n"
    "	  ((and (null (cddr x))\n"
    "		(null (cdadr x)))\n"
    "	   (append l `(\"<fn><fnm>matrix</fnm><p><t>[</t><t>]</t></p></fn>\") r))\n"
    "	  (t\n"
    "	   (append l (cond\n"
    "		      ((find \'inference (car x))\n"
    "		       (list \"<tb inference=\\\"true\\\">\"))\n"
    "		      ((find \'special (car x))\n"
    "		       (list (format nil \"<tb special=\\\"true\\\" rownames=~s colnames=~s>\"\n"
    "				     (if (find \'rownames (car x)) \"true\" \"false\")\n"
    "				     (if (find \'colnames (car x)) \"true\" \"false\"))))\n"
    "		      ((string= $lmxchar #\\()\n"
    "		       (list \"<tb roundedParens=\\\"true\\\">\"))\n"
    "		      (t\n"
    "		       (list \"<tb>\")))\n"
    "		   (mapcan #\'(lambda (y)\n"
    "			       (cond ((null (cdr y))\n"
    "				      (list \"<mtr><mtd><mspace/></mtd></mtr>\"))\n"
    "				     (t\n"
    "				      (wxxml-list (cdr y)\n"
    "						  (list \"<mtr><mtd>\")\n"
    "						  (list \"</mtd></mtr>\")\n"
    "						  \"</mtd><mtd>\"))))\n"
    "			   (cdr x))\n"
    "		   `(\"</tb>\") r))))\n"
    "\n"
    "  ;; macsyma sum or prod is over integer range, not  low <= index <= high\n"
    "  ;; wxxml is lots more flexible .. but\n"
    "\n"
    "  (defprop %sum wxxml-sum wxxml)\n"
    "  (defprop %lsum wxxml-lsum wxxml)\n"
    "  (defprop %product wxxml-sum wxxml)\n"
    "  (defprop $sum wxxml-sum wxxml)\n"
    "  (defprop $lsum wxxml-lsum wxxml)\n"
    "  (defprop $product wxxml-sum wxxml)\n"
    "\n"
    "  ;; easily extended to union, intersect, otherops\n"
    "\n"
    "  (defun wxxml-lsum(x l r)\n"
    "    (let ((op \"<sm type=\\\"lsum\\\"><r>\")\n"
    "	  ;; gotta be one of those above\n"
    "	  (s1 (wxxml (cadr x) nil nil \'mparen rop));; summand\n"
    "	  (index ;; \"index = lowerlimit\"\n"
    "	   (wxxml `((min simp) , (caddr x), (cadddr x))\n"
    "		  nil nil \'mparen \'mparen)))\n"
    "      (append l `(,op ,@index\n"
    "		      \"</r><r><mn/></r><r>\"\n"
    "		      ,@s1 \"</r></sm>\") r)))\n"
    "\n"
    "  (defun wxxml-sum(x l r)\n"
    "    (let ((op (if (or (eq (caar x) \'%sum)\n"
    "		      (eq (caar x) \'$sum))\n"
    "		  \"<sm><r>\"\n"
    "		\"<sm type=\\\"prod\\\"><r>\"))\n"
    "	  (s1 (wxxml (cadr x) nil nil \'mparen rop));; summand\n"
    "	  (index ;; \"index = lowerlimit\"\n"
    "	   (wxxml `((mequal simp) ,(caddr x) ,(cadddr x))\n"
    "		  nil nil \'mparen \'mparen))\n"
    "	  (toplim (wxxml (car (cddddr x)) nil nil \'mparen \'mparen)))\n"
    "      (append l `( ,op ,@index \"</r><r>\" ,@toplim\n"
    "		       \"</r><r>\"\n"
    "		       ,@s1 \"</r></sm>\") r)))\n"
    "\n"
    "  (defprop %integrate wxxml-int wxxml)\n"
    "  (defprop $integrate wxxml-int wxxml)\n"
    "\n"
    "  (defun wxxml-int (x l r)\n"
    "    (let ((s1 (wxxml (cadr x) nil nil \'mparen \'mparen));;integrand delims / & d\n"
    "	  (var (wxxml (caddr x) nil nil \'mparen rop))) ;; variable\n"
    "      (cond ((= (length x) 3)\n"
    "	     (append l `(\"<in def=\\\"false\\\"><r>\"\n"
    "			 ,@s1\n"
    "			 \"</r><r><s>d</s>\"\n"
    "			 ,@var\n"
    "			 \"</r></in>\") r))\n"
    "	    (t ;; presumably length 5\n"
    "	     (let ((low (wxxml (nth 3 x) nil nil \'mparen \'mparen))\n"
    "		   ;; 1st item is 0\n"
    "		   (hi (wxxml (nth 4 x) nil nil \'mparen \'mparen)))\n"
    "	       (append l `(\"<in><r>\"\n"
    "			   ,@low\n"
    "			   \"</r><r>\"\n"
    "			   ,@hi\n"
    "			   \"</r><r>\"\n"
    "			   ,@s1\n"
    "			   \"</r><r><s>d</s>\"\n"
    "			   ,@var \"</r></in>\") r))))))\n"
    "\n"
    "  (defprop %limit wxxml-limit wxxml)\n"
    "\n"
    "  (defprop mrarr wxxml-infix wxxml)\n"
    "  (defprop mrarr (\"<t>-></t>\") wxxmlsym)\n"
    "  (defprop mrarr 80. wxxml-lbp)\n"
    "  (defprop mrarr 80. wxxml-rbp)\n"
    "\n"
    "  (defun wxxml-limit (x l r) ;; ignoring direction, last optional arg to limit\n"
    "    (let ((s1 (wxxml (second x) nil nil \'mparen rop));; limitfunction\n"
    "	  (subfun ;; the thing underneath \"limit\"\n"
    "	   (wxxml `((mrarr simp) ,(third x)\n"
    "		    ,(fourth x)) nil nil \'mparen \'mparen)))\n"
    "      (case (fifth x)\n"
    "	    ($plus\n"
    "	     (append l `(\"<lm><fnm>lim</fnm><r>\"\n"
    "			 ,@subfun \"<v>+</v></r><r>\"\n"
    "			 ,@s1 \"</r></lm>\") r))\n"
    "	    ($minus\n"
    "	     (append l `(\"<lm><fnm>lim</fnm><r>\"\n"
    "			 ,@subfun \"<t>-</t></r><r>\"\n"
    "			 ,@s1 \"</r></lm>\") r))\n"
    "	    (otherwise\n"
    "	     (append l `(\"<lm><fnm>lim</fnm><r>\"\n"
    "			 ,@subfun \"</r><r>\"\n"
    "			 ,@s1 \"</r></lm>\") r)))))\n"
    "\n"
    "  (defprop %at wxxml-at wxxml)\n"
    "  ;; e.g.  at(diff(f(x)),x=a)\n"
    "  (defun wxxml-at (x l r)\n"
    "    (let ((s1 (wxxml (cadr x) nil nil lop rop))\n"
    "	  (sub (wxxml (caddr x) nil nil \'mparen \'mparen)))\n"
    "      (append l \'(\"<at><r>\") s1\n"
    "	      \'(\"</r><r>\") sub \'(\"</r></at>\") r)))\n"
    "\n"
    "  ;;binomial coefficients\n"
    "\n"
    "  (defprop %binomial wxxml-choose wxxml)\n"
    "\n"
    "\n"
    "  (defun wxxml-choose (x l r)\n"
    "    `(,@l\n"
    "      \"<p print=\\\"no\\\"><f line=\\\"no\\\"><r>\"\n"
    "      ,@(wxxml (cadr x) nil nil \'mparen \'mparen)\n"
    "      \"</r><r>\"\n"
    "      ,@(wxxml (caddr x) nil nil \'mparen \'mparen)\n"
    "      \"</r></f></p>\"\n"
    "      ,@r))\n"
    "\n"
    "\n"
    "  (defprop rat wxxml-rat wxxml)\n"
    "  (defprop rat 120. wxxml-lbp)\n"
    "  (defprop rat 121. wxxml-rbp)\n"
    "  (defun wxxml-rat(x l r) (wxxml-mquotient x l r))\n"
    "\n"
    "  (defprop mplus wxxml-mplus wxxml)\n"
    "  (defprop mplus 100. wxxml-lbp)\n"
    "  (defprop mplus 100. wxxml-rbp)\n"
    "\n"
    "  (defun wxxml-mplus (x l r)\n"
    "    (cond ((member \'trunc (car x) :test #\'eq)\n"
    "	   (setq r (cons \"<v>+</v><t>...</t>\" r))))\n"
    "    (cond ((null (cddr x))\n"
    "	   (if (null (cdr x))\n"
    "	       (wxxml-function x l r)\n"
    "	     (wxxml (cadr x) l r \'mplus rop)))\n"
    "	  (t (setq l (wxxml (cadr x) l nil lop \'mplus)\n"
    "		   x (cddr x))\n"
    "	     (do ((nl l)  (dissym))\n"
    "		 ((null (cdr x))\n"
    "		  (if (mmminusp (car x)) (setq l (cadar x) dissym\n"
    "					       (list \"<v>-</v>\"))\n"
    "		    (setq l (car x) dissym (list \"<v>+</v>\")))\n"
    "		  (setq r (wxxml l dissym r \'mplus rop))\n"
    "		  (append nl r))\n"
    "		 (if (mmminusp (car x)) (setq l (cadar x) dissym\n"
    "					      (list \"<v>-</v>\"))\n"
    "		   (setq l (car x) dissym (list \"<v>+</v>\")))\n"
    "		 (setq nl (append nl (wxxml l dissym nil \'mplus \'mplus))\n"
    "		       x (cdr x))))))\n"
    "\n"
    "  (defprop mminus wxxml-prefix wxxml)\n"
    "  (defprop mminus (\"<v>-</v>\") wxxmlsym)\n"
    "  (defprop mminus \"<v>-</v>\" wxxmlword)\n"
    "  (defprop mminus 101. wxxml-rbp)\n"
    "  (defprop mminus 101. wxxml-lbp)\n"
    "\n"
    "  (defprop $~ wxxml-infix wxxml)\n"
    "  (defprop $~ (\"<t>~</t>\") wxxmlsym)\n"
    "  (defprop $~ \"<t>~</t>\" wxxmlword)\n"
    "  (defprop $~ 134. wxxml-lbp)\n"
    "  (defprop $~ 133. wxxml-rbp)\n"
    "\n"
    "  (defprop min wxxml-infix wxxml)\n"
    "  (defprop min (\"<fnm>in</fnm>\") wxxmlsym)\n"
    "  (defprop min \"<fnm>in</fnm>\" wxxmlword)\n"
    "  (defprop min 80. wxxml-lbp)\n"
    "  (defprop min 80. wxxml-rbp)\n"
    "\n"
    "  (defprop mequal wxxml-infix wxxml)\n"
    "  (defprop mequal (\"<v>=</v>\") wxxmlsym)\n"
    "  (defprop mequal \"<v>=</v>\" wxxmlword)\n"
    "  (defprop mequal 80. wxxml-lbp)\n"
    "  (defprop mequal 80. wxxml-rbp)\n"
    "\n"
    "  (defprop mnotequal wxxml-infix wxxml)\n"
    "  (defprop mnotequal (\"<t>#</t>\") wxxmlsym)\n"
    "  (defprop mnotequal 80. wxxml-lbp)\n"
    "  (defprop mnotequal 80. wxxml-rbp)\n"
    "\n"
    "  (defprop mgreaterp wxxml-infix wxxml)\n"
    "  (defprop mgreaterp (\"<t>&gt;</t>\") wxxmlsym)\n"
    "  (defprop mgreaterp \"<t>&gt;</t>\" wxxmlword)\n"
    "  (defprop mgreaterp 80. wxxml-lbp)\n"
    "  (defprop mgreaterp 80. wxxml-rbp)\n"
    "\n"
    "  (defprop mgeqp wxxml-infix wxxml)\n"
    "  (defprop mgeqp (\"<t>&gt;=</t>\") wxxmlsym)\n"
    "  (defprop mgeqp \"<t>&gt;=</t>\" wxxmlword)\n"
    "  (defprop mgeqp 80. wxxml-lbp)\n"
    "  (defprop mgeqp 80. wxxml-rbp)\n"
    "\n"
    "  (defprop mlessp wxxml-infix wxxml)\n"
    "  (defprop mlessp (\"<t>&lt;</t>\") wxxmlsym)\n"
    "  (defprop mlessp \"<t>&lt;</t>\" wxxmlword)\n"
    "  (defprop mlessp 80. wxxml-lbp)\n"
    "  (defprop mlessp 80. wxxml-rbp)\n"
    "\n"
    "  (defprop mleqp wxxml-infix wxxml)\n"
    "  (defprop mleqp (\"<t>&lt;=</t>\") wxxmlsym)\n"
    "  (defprop mleqp \"<t>&lt;=</t>\" wxxmlword)\n"
    "  (defprop mleqp 80. wxxml-lbp)\n"
    "  (defprop mleqp 80. wxxml-rbp)\n"
    "\n"
    "  (defprop mnot wxxml-prefix wxxml)\n"
    "  (defprop mnot (\"<fnm altCopy=\\\"not \\\">not</fnm>\") wxxmlsym)\n"
    "  (defprop mnot \"<fnm>not</fnm>\" wxxmlword)\n"
    "  (defprop mnot 70. wxxml-rbp)\n"
    "\n"
    "  (defprop mand wxxml-nary wxxml)\n"
    "  (defprop mand \"<mspace/><fnm>and</fnm><mspace/>\" wxxmlsym)\n"
    "  (defprop mand \"<fnm>and</fnm>\" wxxmlword)\n"
    "  (defprop mand 60. wxxml-lbp)\n"
    "  (defprop mand 60. wxxml-rbp)\n"
    "\n"
    "  (defprop mor wxxml-nary wxxml)\n"
    "  (defprop mor \"<mspace/><fnm>or</fnm><mspace/>\" wxxmlsym)\n"
    "  (defprop mor \"<fnm>or</fnm>\" wxxmlword)\n"
    "  (defprop mor 50. wxxml-lbp)\n"
    "  (defprop mor 50. wxxml-rbp)\n"
    "\n"
    "\n"
    "  (defprop mcond wxxml-mcond wxxml)\n"
    "  (defprop mcond 25. wxxml-lbp)\n"
    "  (defprop mcond 25. wxxml-rbp)\n"
    "\n"
    "  (defprop %derivative wxxml-derivative wxxml)\n"
    "  (defprop %derivative 120. wxxml-lbp)\n"
    "  (defprop %derivative 119. wxxml-rbp)\n"
    "\n"
    "  (defprop $diff wxxml-derivative wxxml)\n"
    "  (defprop $diff 120. wxxml-lbp)\n"
    "  (defprop $diff 119. wxxml-rbp)\n"
    "\n"
    "  (defun wxxml-derivative (x l r)\n"
    "    (if (and $derivabbrev\n"
    "	     (every #\'integerp (odds (cddr x) 0))\n"
    "	     (every #\'atom (odds (cddr x) 1)))\n"
    "	(append l (wxxml-d-abbrev x) r)\n"
    "      (wxxml (wxxml-d x) (append l \'(\"<d>\"))\n"
    "	     (append \'(\"</d>\") r) \'mparen \'mparen)))\n"
    "\n"
    "  (defun $derivabbrev (a)\n"
    "    (if a\n"
    "	(progn\n"
    "	  (defprop %derivative 130. wxxml-lbp)\n"
    "	  (defprop %derivative 129. wxxml-rbp)\n"
    "	  (setq $derivabbrev t))\n"
    "      (progn\n"
    "	(defprop %derivative 120. wxxml-lbp)\n"
    "	(defprop %derivative 119. wxxml-rbp)\n"
    "	(setq $derivabbrev nil))))\n"
    "\n"
    "  (defun wxxml-d-abbrev-subscript (l_vars l_ords &aux var_xml)\n"
    "    (let ((sub ()))\n"
    "      (loop while l_vars do\n"
    "	    (setq var_xml (car (wxxml (car l_vars) nil nil \'mparen \'mparen)))\n"
    "	    (loop for i from 1 to (car l_ords) do\n"
    "		  (setq sub (cons var_xml sub)))\n"
    "	    (setq l_vars (cdr l_vars)\n"
    "		  l_ords (cdr l_ords)))\n"
    "      (reverse sub)))\n"
    "\n"
    "  (defun wxxml-d-abbrev (x)\n"
    "    (let*\n"
    "	((difflist (cddr x))\n"
    "	 (ords (odds  difflist 0))\n"
    "	 (ords (cond ((null ords) \'(1))\n"
    "		     (t ords)))\n"
    "	 (vars (odds difflist 1))\n"
    "	 (fun (wxxml (cadr x) nil nil \'mparen \'mparen)))\n"
    "      (append \'(\"<i d=\\\"1\\\"><r>\") fun \'(\"</r>\")\n"
    "	      \'(\"<r>\") (wxxml-d-abbrev-subscript vars ords) \'(\"</r></i>\"))))\n"
    "\n"
    "  (defun wxxml-d (x)\n"
    "    ;; format the macsyma derivative form so it looks\n"
    "    ;; sort of like a quotient times the deriva-dand.\n"
    "    (let*\n"
    "	(($simp t)\n"
    "	 (arg (cadr x)) ;; the function being differentiated\n"
    "	 (difflist (cddr x)) ;; list of derivs e.g. (x 1 y 2)\n"
    "	 (ords (odds difflist 0)) ;; e.g. (1 2)\n"
    "	 (ords (cond ((null ords) \'(1))\n"
    "		     (t ords)))\n"
    "	 (vars (odds difflist 1)) ;; e.g. (x y)\n"
    "	 (dsym \'((wxxmltag simp) \"d\" \"s\"))\n"
    "	 (numer `((mexpt) ,dsym ((mplus) ,@ords))) ; d^n numerator\n"
    "	 (denom (cons \'(mtimes)\n"
    "		      (mapcan #\'(lambda(b e)\n"
    "				  `(,dsym ,(simplifya `((mexpt) ,b ,e) nil)))\n"
    "			      vars ords))))\n"
    "      `((wxtimes)\n"
    "	((mquotient) ,(simplifya numer nil) ,denom)\n"
    "	,arg)))\n"
    "\n"
    "  (defun wxxml-mcond (x l r)\n"
    "    (let ((res ()))\n"
    "      (setq res (wxxml (cadr x) \'(\"<fnm>if</fnm><mspace/>\")\n"
    "		       \'(\"<mspace/><fnm>then</fnm><mspace/>\") \'mparen \'mparen))\n"
    "      (setq res (append res (wxxml (caddr x) nil\n"
    "				   \'(\"<mspace/>\") \'mparen \'mparen)))\n"
    "      (let ((args (cdddr x)))\n"
    "	(loop while (>= (length args) 2) do\n"
    "	      (cond\n"
    "	       ((and (= (length args) 2) (eql (car args) t))\n"
    "		(unless (or (eql (cadr args) \'$false) (null (cadr args)))\n"
    "		  (setq res (wxxml (cadr args)\n"
    "				   (append res \'(\"<fnm>else</fnm><mspace/>\"))\n"
    "				   nil \'mparen \'mparen))))\n"
    "	       (t\n"
    "		(setq res (wxxml (car args)\n"
    "				 (append res \'(\"<fnm>elseif</fnm><mspace/>\"))\n"
    "				 (wxxml (cadr args)\n"
    "					\'(\"<mspace/><fnm>then</fnm><mspace/>\")\n"
    "					\'(\"<mspace/>\") \'mparen \'mparen)\n"
    "				 \'mparen \'mparen))))\n"
    "	      (setq args (cddr args)))\n"
    "	(append l res r))))\n"
    "\n"
    "  (defprop mdo wxxml-mdo wxxml)\n"
    "  (defprop mdo 30. wxxml-lbp)\n"
    "  (defprop mdo 30. wxxml-rbp)\n"
    "  (defprop mdoin wxxml-mdoin wxxml)\n"
    "  (defprop mdoin 30. wxxml-rbp)\n"
    "\n"
    "  (defun wxxml-lbp (x)\n"
    "    (cond ((wxxml-get x \'wxxml-lbp))\n"
    "	  (t(lbp x))))\n"
    "\n"
    "  (defun wxxml-rbp (x)\n"
    "    (cond ((wxxml-get x \'wxxml-rbp))\n"
    "	  (t(lbp x))))\n"
    "\n"
    "  ;; these aren\'t quite right\n"
    "\n"
    "  (defun wxxml-mdo (x l r)\n"
    "    (wxxml-list (wxxmlmdo x) l r \"<mspace/>\"))\n"
    "\n"
    "  (defun wxxml-mdoin (x l r)\n"
    "    (wxxml-list (wxxmlmdoin x) l r \"<mspace/>\"))\n"
    "\n"
    "  (defun wxxmlmdo (x)\n"
    "    (nconc (cond ((second x) (list (make-tag \"for\" \"fnm\") (second x))))\n"
    "	   (cond ((equal 1 (third x)) nil)\n"
    "		 ((third x)  (list (make-tag \"from\" \"fnm\") (third x))))\n"
    "	   (cond ((equal 1 (fourth x)) nil)\n"
    "		 ((fourth x)\n"
    "		  (list (make-tag \"step\" \"fnm\")  (fourth x)))\n"
    "		 ((fifth x)\n"
    "		  (list (make-tag \"next\" \"fnm\") (fifth x))))\n"
    "	   (cond ((sixth x)\n"
    "		  (list (make-tag \"thru\" \"fnm\") (sixth x))))\n"
    "	   (cond ((null (seventh x)) nil)\n"
    "		 ((eq \'mnot (caar (seventh x)))\n"
    "		  (list (make-tag \"while\" \"fnm\") (cadr (seventh x))))\n"
    "		 (t (list (make-tag \"unless\" \"fnm\") (seventh x))))\n"
    "	   (list (make-tag \"do\" \"fnm\") (eighth x))))\n"
    "\n"
    "  (defun wxxmlmdoin (x)\n"
    "    (nconc (list (make-tag \"for\" \"fnm\") (second x)\n"
    "		 (make-tag \"in\" \"fnm\") (third x))\n"
    "	   (cond ((sixth x)\n"
    "		  (list (make-tag \"thru\" \"fnm\") (sixth x))))\n"
    "	   (cond ((null (seventh x)) nil)\n"
    "		 ((eq \'mnot (caar (seventh x)))\n"
    "		  (list (make-tag \"while\" \"fnm\") (cadr (seventh x))))\n"
    "		 (t (list (make-tag \"unless\" \"fnm\") (seventh x))))\n"
    "	   (list (make-tag \"do\" \"fnm\") (eighth x))))\n"
    "\n"
    "\n"
    "  (defun wxxml-matchfix-np (x l r)\n"
    "    (setq l (append l (car (wxxmlsym (caar x))))\n"
    "	  ;; car of wxxmlsym of a matchfix operator is the lead op\n"
    "	  r (append (cdr (wxxmlsym (caar x))) r)\n"
    "	  ;; cdr is the trailing op\n"
    "	  x (wxxml-list (cdr x) nil r \"\"))\n"
    "    (append l x))\n"
    "\n"
    "  (defprop text-string wxxml-matchfix-np wxxml)\n"
    "  (defprop text-string ((\"<t>\")\"</t>\") wxxmlsym)\n"
    "\n"
    "  (defprop mtext wxxml-matchfix-np wxxml)\n"
    "  (defprop mtext ((\"\")\"\") wxxmlsym)\n"
    "\n"
    "  (defvar *wxxml-mratp* nil)\n"
    "\n"
    "  (defun wxxml-mlable (x l r)\n"
    "    (wxxml (caddr x)\n"
    "	   (append l\n"
    "		   (if (cadr x)\n"
    "		       (list\n"
    "			(format nil \"<lbl>(~A)~A </lbl>\"\n"
    "				(stripdollar (maybe-invert-string-case (symbol-name (cadr x))))\n"
    "				*wxxml-mratp*))\n"
    "		     nil))\n"
    "	   r \'mparen \'mparen))\n"
    "\n"
    "  (defprop mlable wxxml-mlable wxxml)\n"
    "  (defprop mlabel wxxml-mlable wxxml)\n"
    "\n"
    "  (defun wxxml-spaceout (x l r)\n"
    "    (append l (list \" \" (make-string (cadr x) :initial-element #\\.) \"\") r))\n"
    "\n"
    "  (defprop spaceout wxxml-spaceout wxxml)\n"
    "\n"
    "  (defun mydispla (x)\n"
    "    (let ((*print-circle* nil)\n"
    "	  (*wxxml-mratp* (format nil \"~{~a~}\" (cdr (checkrat x)))))\n"
    "      (mapc #\'princ\n"
    "	    (wxxml x \'(\"<mth>\") \'(\"</mth>\") \'mparen \'mparen))))\n"
    "\n"
    "  (setf *alt-display2d* \'mydispla)\n"
    "\n"
    "  (defun $set_display (tp)\n"
    "    (cond\n"
    "     ((eq tp \'$none)\n"
    "      (setq $display2d nil))\n"
    "     ((eq tp \'$ascii)\n"
    "      (setq $display2d t)\n"
    "      (setf *alt-display2d* nil))\n"
    "     ((eq tp \'$xml)\n"
    "      (setq $display2d t)\n"
    "      (setf *alt-display2d* \'mydispla))\n"
    "     (t\n"
    "      (format t \"Unknown display type\")\n"
    "      (setq tp \'$unknown)))\n"
    "    tp)\n"
    "\n"
    "  ;;\n"
    "  ;; inference_result from the stats package\n"
    "  ;;\n"
    "\n"
    "  (defun wxxml-inference (x l r)\n"
    "    (let ((name (cadr x))\n"
    "	  (values (caddr x))\n"
    "	  (dis (cadddr x))\n"
    "	  (m ()))\n"
    "      (labels\n"
    "       ((build-eq (e)\n"
    "		  `((mequal simp) ,(cadr e) ,(caddr e))))\n"
    "       (dolist (i (cdr dis))\n"
    "	 (setq m (append m `(((mlist simp) ,(build-eq (nth i values)))))))\n"
    "       (setq m (cons `((mlist simp) ,name) m))\n"
    "       (setq m (cons \'($matrix simp inference) m))\n"
    "       (wxxml m l r \'mparen \'mparen))))\n"
    "\n"
    "  (defprop $inference_result wxxml-inference wxxml)\n"
    "\n"
    "  (defun wxxml-amatrix (x l r)\n"
    "    (let* ((nr ($@-function x \'$nr))\n"
    "	   (nc ($@-function x \'$nc))\n"
    "	   (M (simplifya ($genmatrix\n"
    "			  `((lambda) ((mlist) i j) (mfuncall \'$get_element ,x i j))\n"
    "			  nr nc)\n"
    "			 t)))\n"
    "      (wxxml-matrix M l r)))\n"
    "\n"
    "  (defprop $amatrix wxxml-amatrix wxxml)\n"
    "\n"
    "  ;;\n"
    "  ;; orthopoly functions\n"
    "  ;;\n"
    "\n"
    "  (defun wxxml-pochhammer (x l r)\n"
    "    (let ((n (cadr x))\n"
    "	  (k (caddr x)))\n"
    "      (append l\n"
    "	      (list (format nil \"<i altCopy=\\\"~{~a~}\\\"><p>\" (mstring x)))\n"
    "	      (wxxml n nil nil \'mparen \'mparen)\n"
    "	      (list \"</p><r>\")\n"
    "	      (wxxml k nil nil \'mparen \'mparen)\n"
    "	      (list \"</r></i>\")\n"
    "	      r)))\n"
    "\n"
    "  (defprop $pochhammer wxxml-pochhammer wxxml)\n"
    "\n"
    "  (defun wxxml-orthopoly (x l r)\n"
    "    (let* ((fun-name (caar x))\n"
    "	   (disp-name (get fun-name \'wxxml-orthopoly-disp))\n"
    "	   (args (cdr x)))\n"
    "      (append l\n"
    "	      (list (format nil \"<fn altCopy=\\\"~{~a~}\\\">\" (mstring x)))\n"
    "	      (if (nth 2 disp-name)\n"
    "		  (list (format nil \"<ie><fnm>~a</fnm><r>\" (car disp-name)))\n"
    "		(list (format nil \"<i><fnm>~a</fnm><r>\" (car disp-name))))\n"
    "	      (wxxml (nth (nth 1 disp-name) args) nil nil \'mparen \'mparen)\n"
    "	      (when (nth 2 disp-name)\n"
    "		(append (list \"</r><r>\")\n"
    "			(when (nth 3 disp-name) (list \"<p>\"))\n"
    "			(wxxml-list (or (nth 5 disp-name)\n"
    "					(mapcar (lambda (i) (nth i args)) (nth 2 disp-name)))\n"
    "				    nil nil \",\")\n"
    "			(when (nth 3 disp-name) (list \"</p>\"))\n"
    "			(list \"</r>\")))\n"
    "	      (if (nth 2 disp-name)\n"
    "		  (list \"</ie>\")\n"
    "		(list \"</r></i>\"))\n"
    "	      (list \"<p>\")\n"
    "	      (wxxml-list (mapcar (lambda (i) (nth i args)) (nth 4 disp-name)) nil nil \",\")\n"
    "	      (list \"</p></fn>\")\n"
    "	      r)))\n"
    "\n"
    "  (dolist (ortho-pair\n"
    "	   \'(($laguerre \"L\" 0 nil nil (1))\n"
    "	     (%laguerre \"L\" 0 nil nil (1))\n"
    "	     ($legendre_p \"P\" 0 nil nil (1))\n"
    "	     (%legendre_p \"P\" 0 nil nil (1))\n"
    "	     ($legendre_q \"Q\" 0 nil nil (1))\n"
    "	     (%legendre_q \"Q\" 0 nil nil (1))\n"
    "	     ($chebyshev_t \"T\" 0 nil nil (1))\n"
    "	     (%chebyshev_t \"T\" 0 nil nil (1))\n"
    "	     ($chebyshev_u \"U\" 0 nil nil (1))\n"
    "	     (%chebyshev_u \"U\" 0 nil nil (1))\n"
    "	     ($hermite \"H\" 0 nil nil (1))\n"
    "	     (%hermite \"H\" 0 nil nil (1))\n"
    "	     ($spherical_bessel_j \"J\" 0 nil nil (1))\n"
    "	     (%spherical_bessel_j \"J\" 0 nil nil (1))\n"
    "	     ($spherical_bessel_y \"Y\" 0 nil nil (1))\n"
    "	     (%spherical_bessel_y \"Y\" 0 nil nil (1))\n"
    "	     ($assoc_legendre_p \"P\" 0 (1) nil (2))\n"
    "	     (%assoc_legendre_p \"P\" 0 (1) nil (2))\n"
    "	     ($assoc_legendre_q \"Q\" 0 (1) nil (2))\n"
    "	     (%assoc_legendre_q \"Q\" 0 (1) nil (2))\n"
    "	     ($jacobi_p \"P\" 0 (1 2) t (3))\n"
    "	     (%jacobi_p \"P\" 0 (1 2) t (3))\n"
    "	     ($gen_laguerre \"L\" 0 (1) t (2))\n"
    "	     (%gen_laguerre \"L\" 0 (1) t (2))\n"
    "	     ($spherical_harmonic \"Y\" 0 (1) nil (2 3))\n"
    "	     (%spherical_harmonic \"Y\" 0 (1) nil (2 3))\n"
    "	     ($ultraspherical \"C\" 0 (1) t (2))\n"
    "	     (%ultraspherical \"C\" 0 (1) t (2))\n"
    "	     ($spherical_hankel1 \"H\" 0 t t (1) (1))\n"
    "	     (%spherical_hankel1 \"H\" 0 t t (1) (1))\n"
    "	     ($spherical_hankel2 \"H\" 0 t t (1) (2))\n"
    "	     (%spherical_hankel2 \"H\" 0 t t (1) (2))))\n"
    "    (setf (get (car ortho-pair) \'wxxml) \'wxxml-orthopoly)\n"
    "    (setf (get (car ortho-pair) \'wxxml-orthopoly-disp) (cdr ortho-pair)))\n"
    "\n"
    ";;;\n"
    ";;; This is the display support only - copy/paste will not work\n"
    ";;;\n"
    "\n"
    "  (defmvar $pdiff_uses_prime_for_derivatives nil)\n"
    "  (defmvar $pdiff_prime_limit 3)\n"
    "  (defmvar $pdiff_uses_named_subscripts_for_derivatives nil)\n"
    "  (defmvar $pdiff_diff_var_names (list \'(mlist) \'|$x| \'|$y| \'|$z|))\n"
    "\n"
    "  (setf (get \'%pderivop \'wxxml) \'wxxml-pderivop)\n"
    "  (setf (get \'$pderivop \'wxxml) \'wxxml-pderivop)\n"
    "\n"
    "  (defun wxxml-pderivop (x l r)\n"
    "    (cond ((and $pdiff_uses_prime_for_derivatives (eq 3 (length x)))\n"
    "	   (let* ((n (car (last x)))\n"
    "		  (p))\n"
    "\n"
    "	     (cond ((<= n $pdiff_prime_limit)\n"
    "		    (setq p (make-list n :initial-element \"\'\")))\n"
    "		   (t\n"
    "		    (setq p (list \"(\" n \")\"))))\n"
    "	     (append (append l \'(\"<r>\"))\n"
    "		     (let ((*var-tag* (list \"<fnm>\" \"</fnm>\"))) (wxxml (cadr x) nil nil lop rop))\n"
    "		     p\n"
    "		     (list \"</r>\")  r)))\n"
    "\n"
    "	  ((and $pdiff_uses_named_subscripts_for_derivatives\n"
    "		(< (apply #\'+ (cddr x)) $pdiff_prime_limit))\n"
    "	   (let ((n (cddr x))\n"
    "		 (v (mapcar #\'stripdollar (cdr $pdiff_diff_var_names)))\n"
    "		 (p))\n"
    "	     (cond ((> (length n) (length v))\n"
    "		    (merror \"Not enough elements in pdiff_diff_var_names to display the expression\")))\n"
    "	     (dotimes (i (length n))\n"
    "	       (setq p (append p (make-list (nth i n)\n"
    "					    :initial-element (nth i v)))))\n"
    "	     (append (append l \'(\"<i><r>\"))\n"
    "		     (wxxml (cadr x) nil nil lop rop)\n"
    "		     (list \"</r><r>\") p (list \"</r></i>\") r)))\n"
    "	  (t\n"
    "	   (append (append l \'(\"<i><r>\"))\n"
    "		   (wxxml (cadr x) nil nil lop rop)\n"
    "		   (list \"</r><r>(\")\n"
    "		   (wxxml-list (cddr x) nil nil \",\")\n"
    "		   (list \")</r></i>\") r))))\n"
    "\n"
    "  ;;\n"
    "  ;; Plotting support\n"
    "  ;;\n"
    "\n"
    "  (defprop wxxmltag wxxml-tag wxxml)\n"
    "\n"
    "  (defun wxxml-tag (x l r)\n"
    "    (let ((name (cadr x))\n"
    "	  (tag (caddr x))\n"
    "	  (prop (cadddr x)))\n"
    "      (if prop\n"
    "	  (append l (list (format nil \"<~a ~a>~a</~a>\" tag prop name tag)) r)\n"
    "	(append l (list (format nil \"<~a>~a</~a>\" tag name tag)) r))))\n"
    "\n"
    "\n"
    "  (defvar *image-counter* 0)\n"
    "\n"
    "  ;; A suitable name for a .gnuplot file\n"
    "  (defun wxplot-gnuplotfilename ()\n"
    "    (incf *wx-plot-num*)\n"
    "    (format nil \"maxout_~d_~d.gnuplot\" (getpid) *wx-plot-num*))\n"
    "\n"
    "  ;; A suitable name for a .data file\n"
    "  (defun wxplot-datafilename ()\n"
    "    (incf *wx-plot-num*)\n"
    "    (format nil \"maxout_~d_~d.data\" (getpid) *wx-plot-num*))\n"
    "\n"
    "  (defun wxplot-filename (&optional (suff t))\n"
    "    (incf *image-counter*)\n"
    "    (plot-temp-file (if suff\n"
    "			(format nil \"maxout_~d_~d.png\" (getpid) *image-counter*)\n"
    "		      (format nil \"maxout_~d_~d\" (getpid) *image-counter*))))\n"
    "\n"
    "  ;; The \"solid\" has to be changed to \"dashed\" as soon as plot() starts\n"
    "  ;; supporting other line styles than \"solid\" or \"dots\".\n"
    "  (defun $wxplot_preamble ()\n"
    "    (let ((frmt\n"
    "	   (cond\n"
    "	    ($wxplot_old_gnuplot \"set terminal png picsize ~d ~d; set zeroaxis;\")\n"
    "	    ($wxplot_pngcairo \"set terminal pngcairo solid background \\\"white\\\" enhanced font \\\"arial,10\\\" fontscale 1.0 size ~d,~d; set zeroaxis;\")\n"
    "	    (t \"set terminal png size ~d,~d; set zeroaxis;\"))))\n"
    "      (format nil frmt\n"
    "	      ($first $wxplot_size)\n"
    "	      ($second $wxplot_size))))\n"
    "\n"
    "  (defun $int_range (lo &optional hi (st 1))\n"
    "    (unless (integerp lo)\n"
    "      ($error \"int_range: first argument is not an integer.\"))\n"
    "    (unless (or (null hi) (integerp hi))\n"
    "      ($error \"int_range: second argument is not an integer.\"))\n"
    "    (when (null hi)\n"
    "      (setq hi lo)\n"
    "      (setq lo 1))\n"
    "    (cons \'(mlist simp) (loop :for i :from lo :to hi :by st :collect i)))\n"
    "\n"
    "  (defvar *default-framerate* 2)\n"
    "  (defvar $wxanimate_framerate *default-framerate*)\n"
    "  (defun slide-tag (images)\n"
    "    (if (eql *default-framerate* $wxanimate_framerate)\n"
    "	($ldisp (list \'(wxxmltag simp) (format nil \"~{~a;~}\" images) \"slide\" (if (eql $wxanimate_autoplay \'t) \"running=\\\"true\\\" del=\\\"true\\\"\" \"running=\\\"false\\\" del=\\\"true\\\"\")))\n"
    "      ($ldisp (list \'(wxxmltag simp) (format nil \"~{~a;~}\" images) \"slide\" (if (eql $wxanimate_autoplay \'t) (format nil \"fr=\\\"~a\\\" running=\\\"true\\\" del=\\\"true\\\"\" $wxanimate_framerate)  (format nil \"fr=\\\"~a\\\" running=\\\"false\\\" del=\\\"true\\\"\" $wxanimate_framerate))\n"
    "                    ))))\n"
    "\n"
    "  (defun wxanimate (scene)\n"
    "    (let* ((scene (cdr scene))\n"
    "	   (a (car scene))\n"
    "	   (a-range (meval (cadr scene)))\n"
    "	   (expr (caddr scene))\n"
    "	   (args (cdddr scene))\n"
    "	   (frameno 1)\n"
    "	   (images ()))\n"
    "      (when (integerp a-range)\n"
    "	(setq a-range (cons \'(mlist simp) (loop for i from 1 to a-range collect i))))\n"
    "      (dolist (aval (reverse (cdr a-range)))\n"
    "	($wxstatusbar (format nil \"Preparing Frame #~d\" frameno))\n"
    "	(setf frameno (+ 1 frameno))\n"
    "	(let ((preamble ($wxplot_preamble))\n"
    "	      (system-preamble (get-plot-option-string \'$gnuplot_preamble 2))\n"
    "	      (filename (wxplot-filename))\n"
    "	      (expr (maxima-substitute aval a expr)))\n"
    "	  (when (string= system-preamble \"false\")\n"
    "	    (setq system-preamble \"\"))\n"
    "	  (setq preamble (format nil \"~a; ~a\" preamble system-preamble))\n"
    "	  (dolist (arg args)\n"
    "	    (if (and (listp arg) (eql (cadr arg) \'$gnuplot_preamble))\n"
    "		(setq preamble (format nil \"~a; ~a\"\n"
    "				       preamble (meval (maxima-substitute aval a (caddr arg)))))))\n"
    "	  (apply #\'$plot2d `(,(meval expr) ,@(mapcar #\'meval args)\n"
    "			     ((mlist simp) $plot_format $gnuplot)\n"
    "			     ((mlist simp) $gnuplot_term ,(if $wxplot_pngcairo \'$pngcairo \'$png))\n"
    "			     ((mlist simp) $gnuplot_preamble ,preamble)\n"
    "			     ((mlist simp) $gnuplot_out_file ,filename)))\n"
    "	  (setq images (cons filename images))))\n"
    "      (when images\n"
    "	(slide-tag images)))\n"
    "    \"\")\n"
    "\n"
    "  (defmspec $with_slider (scene)\n"
    "    (wxanimate scene))\n"
    "\n"
    "  (defmspec $wxanimate (scene)\n"
    "    (wxanimate scene))\n"
    "\n"
    "  (defvar *windows-OS* (string= *autoconf-win32* \"true\"))\n"
    "\n"
    "  (defun get-file-name-opt (scene)\n"
    "    (let (opts filename)\n"
    "      (loop for opt in scene do\n"
    "	    (if (and (not (atom opt))\n"
    "		     (eq (caar opt) \'mequal)\n"
    "		     (eq (cadr opt) \'$file_name))\n"
    "		(setq filename (caddr opt))\n"
    "	      (setq opts (cons opt opts))))\n"
    "      (values (reverse opts) filename)))\n"
    "\n"
    "  (defun get-pic-size-opt ()\n"
    "    (cond\n"
    "     ((eq ($get \'$draw \'$version) 1)\n"
    "      `(((mequal simp) $pic_width ,($first $wxplot_size))\n"
    "	((mequal simp) $pic_height ,($second $wxplot_size))))\n"
    "     (t\n"
    "      `(((mequal simp) $dimensions ,$wxplot_size)))))\n"
    "\n"
    "  (defun wxanimate-draw (scenes scene-head)\n"
    "    (unless ($get \'$draw \'$version) ($load \"draw\"))\n"
    "    (multiple-value-bind (scene file-name) (get-file-name-opt (cdr scenes))\n"
    "			 (let* ((a (meval (car scene)))\n"
    "				(a-range (meval (cadr scene)))\n"
    "				(*windows-OS* t)\n"
    "				(args (cddr scene))\n"
    "				(frameno 1)\n"
    "				(images ()))\n"
    "			   (when (integerp a-range)\n"
    "			     (setq a-range (cons \'(mlist simp) (loop for i from 1 to a-range collect i))))\n"
    "			   (if file-name\n"
    "			       ;; If file_name is set, draw the animation into gif using gnuplot\n"
    "			       (let (imgs)\n"
    "				 (dolist (aval (reverse (cdr a-range)))\n"
    "				   (setq imgs (cons\n"
    "					       (cons scene-head\n"
    "						     (mapcar #\'(lambda (arg) (meval (maxima-substitute aval a arg)))\n"
    "							     args))\n"
    "					       imgs)))\n"
    "				 ($apply \'$draw\n"
    "					 (append\n"
    "					  `((mlist simp)\n"
    "					    ((mequal simp) $terminal $animated_gif)\n"
    "					    ((mequal simp) $file_name ,file-name))\n"
    "					  (get-pic-size-opt)\n"
    "					  imgs))\n"
    "				 \"\")\n"
    "			     ;; If file_name is not set, show the animation in wxMaxima\n"
    "			     (progn\n"
    "			       (dolist (aval (reverse (cdr a-range)))\n"
    "				 ($wxstatusbar (format nil \"Preparing Frame #~d\" frameno))\n"
    "				 (setf frameno (+ 1 frameno))\n"
    "				 (let* ((filename (wxplot-filename nil))\n"
    "					(args (cons scene-head\n"
    "						    (mapcar #\'(lambda (arg) (meval (maxima-substitute aval a arg)))\n"
    "							    args))))\n"
    "				   (setq images (cons (format nil \"~a.png\" filename) images))\n"
    "				   ($apply \'$draw\n"
    "					   (append\n"
    "					    `((mlist simp)\n"
    "					      ((mequal simp) $terminal ,(if $wxplot_pngcairo \'$pngcairo \'$png))\n"
    "					      ((mequal simp) $file_name ,filename))\n"
    "					    (get-pic-size-opt)\n"
    "					    (list args)))))\n"
    "			       (when images\n"
    "				 (slide-tag images))))\n"
    "			   \"\")))\n"
    "\n"
    "  (defmspec $wxanimate_draw (scene)\n"
    "    (wxanimate-draw scene \'($gr2d)))\n"
    "\n"
    "  (defmspec $with_slider_draw (scene)\n"
    "    (wxanimate-draw scene \'($gr2d)))\n"
    "\n"
    "  (defmspec $with_slider_draw2d (scene)\n"
    "    (wxanimate-draw scene \'($gr2d)))\n"
    "\n"
    "  (defmspec $with_slider_draw3d (scene)\n"
    "    (wxanimate-draw scene \'($gr3d)))\n"
    "\n"
    "  (defmspec $wxanimate_draw3d (scene)\n"
    "    (wxanimate-draw scene \'($gr3d)))\n"
    "\n"
    "  (defun $wxplot2d (&rest args)\n"
    "    (let ((preamble ($wxplot_preamble))\n"
    "	  (system-preamble (get-plot-option-string \'$gnuplot_preamble 2))\n"
    "	  (filename (wxplot-filename)))\n"
    "      (when (string= system-preamble \"false\")\n"
    "	(setq system-preamble \"\"))\n"
    "      (setq preamble (format nil \"~a; ~a\" preamble system-preamble))\n"
    "      (dolist (arg args)\n"
    "	(if (and (listp arg) (eql (cadr arg) \'$gnuplot_preamble))\n"
    "	    (setq preamble (format nil \"~a; ~a\" preamble (caddr arg)))))\n"
    "      (apply #\'$plot2d `(,@args\n"
    "			 ((mlist simp) $plot_format $gnuplot)\n"
    "			 ((mlist simp) $gnuplot_term ,(if $wxplot_pngcairo \'$pngcairo \'$png))\n"
    "			 ((mlist simp) $gnuplot_preamble ,preamble)\n"
    "			 ((mlist simp) $gnuplot_out_file ,filename)))\n"
    "      ($ldisp `((wxxmltag simp) ,filename \"img\")))\n"
    "    \"\")\n"
    "\n"
    "  (defun $wxplot3d (&rest args)\n"
    "    (let ((preamble ($wxplot_preamble))\n"
    "	  (system-preamble (get-plot-option-string \'$gnuplot_preamble 2))\n"
    "	  (filename (wxplot-filename)))\n"
    "      (when (string= system-preamble \"false\")\n"
    "	(setq system-preamble \"\"))\n"
    "      (setq preamble (format nil \"~a; ~a\" preamble system-preamble))\n"
    "      (dolist (arg args)\n"
    "	(if (and (listp arg) (eql (cadr arg) \'$gnuplot_preamble))\n"
    "	    (setq preamble (format nil \"~a; ~a\"\n"
    "				   preamble (caddr arg)))))\n"
    "      (apply #\'$plot3d `(,@args\n"
    "			 ((mlist simp) $plot_format $gnuplot)\n"
    "			 ((mlist simp) $gnuplot_term ,(if $wxplot_pngcairo \'$pngcairo \'$png))\n"
    "			 ((mlist simp) $gnuplot_preamble ,preamble)\n"
    "			 ((mlist simp) $gnuplot_out_file ,filename)))\n"
    "      ($ldisp `((wxxmltag simp) ,filename \"img\")))\n"
    "    \"\")\n"
    "\n"
    "\n"
    "  (defun $wxdraw2d (&rest args)\n"
    "    (apply #\'$wxdraw\n"
    "	   (list (cons \'($gr2d) args))))\n"
    "\n"
    "  (defun $wxdraw3d (&rest args)\n"
    "    (apply #\'$wxdraw\n"
    "	   (list (cons \'($gr3d) args))))\n"
    "\n"
    "  (defvar $display_graphics t)\n"
    "\n"
    "  (defun option-sublist (lst)\n"
    "    (cons \'(mlist simp)\n"
    "	  (loop for l in lst\n"
    "		when (and (listp l) (= ($length l) 2))\n"
    "		collect l)))\n"
    "\n"
    "  (defun $wxdraw (&rest args)\n"
    "    (unless ($get \'$draw \'$version) ($load \"draw\"))\n"
    "    (let* ((file_name_spec ($assoc \'$file_name\n"
    "				   (option-sublist (append (cdar args)\n"
    "							   (cdr args)))))\n"
    "	   (gnuplotfilename (wxplot-gnuplotfilename))\n"
    "	   (datafilename (wxplot-datafilename))\n"
    "	   (filename (or file_name_spec (wxplot-filename nil)))\n"
    "	   (*windows-OS* t)\n"
    "	   res)\n"
    "      (setq res ($apply \'$draw\n"
    "			(append\n"
    "			 \'((mlist simp))\n"
    "			 args\n"
    "			 `(((mequal simp) $terminal ,(if $wxplot_pngcairo \'$pngcairo \'$png))\n"
    "			   ((mequal simp) $gnuplot_file_name ,gnuplotfilename)\n"
    "			   ((mequal simp) $data_file_name ,datafilename)\n"
    "			   ((mequal simp) $file_name ,filename))\n"
    "			 (cond\n"
    "			  ((eq ($get \'$draw \'$version) 1)\n"
    "			   `(((mequal simp) $pic_width ,($first $wxplot_size))\n"
    "			     ((mequal simp) $pic_height ,($second $wxplot_size))))\n"
    "			  (t\n"
    "			   `(((mequal simp) $dimensions ,$wxplot_size)))))))\n"
    "      (if $display_graphics\n"
    "	  (progn\n"
    "	    ($ldisp `((wxxmltag simp) ,(format nil \"~a.png\" filename) \"img\"\n"
    "		      ,(if file_name_spec\n"
    "			   (format nil \"del=\\\"no\\\" gnuplotsource=\\\"~a/~a\\\" gnuplotdata=\\\"~a/~a\\\"\" $maxima_tempdir gnuplotfilename $maxima_tempdir datafilename)\n"
    "			 (format nil \"del=\\\"yes\\\" gnuplotsource=\\\"~a/~a\\\" gnuplotdata=\\\"~a/~a\\\"\" $maxima_tempdir gnuplotfilename $maxima_tempdir datafilename)\n"
    "			 )\n"
    "		      ))\n"
    "	    (setq res \"\"))\n"
    "	(setf res `((wxxmltag simp) ,(format nil \"~a.png\" filename) \"img\")))\n"
    "      res))\n"
    "\n"
    "  (defmspec $wxdraw_list (args)\n"
    "    (unless ($get \'$draw \'$version) ($load \"draw\"))\n"
    "    (let (($display_graphics nil))\n"
    "      ($ldisp (cons \'(mlist simp) (mapcar #\'meval (cdr args)))))\n"
    "    \'$done)\n"
    "\n"
    "  (defun $wximplicit_plot (&rest args)\n"
    "    (let ((preamble ($wxplot_preamble))\n"
    "	  (system-preamble (get-plot-option-string \'$gnuplot_preamble 2))\n"
    "	  (filename (wxplot-filename)))\n"
    "      (when (string= system-preamble \"false\")\n"
    "	(setq system-preamble \"\"))\n"
    "      (setq preamble (format nil \"~a; ~a\" preamble system-preamble))\n"
    "      (dolist (arg args)\n"
    "	(if (and (listp arg) (eql (cadr arg) \'$gnuplot_preamble))\n"
    "	    (setq preamble (format nil \"~a; ~a\"\n"
    "				   preamble (caddr arg)))))\n"
    "      ($apply \'$implicit_plot `((mlist simp) ,@args\n"
    "				((mlist simp) $plot_format $gnuplot)\n"
    "				((mlist simp) $gnuplot_term ,(if $wxplot_pngcairo \'$pngcairo \'$png))\n"
    "				((mlist simp) $gnuplot_preamble ,preamble)\n"
    "				((mlist simp) $gnuplot_out_file ,filename)))\n"
    "      ($ldisp `((wxxmltag simp) ,filename \"img\")))\n"
    "    \"\")\n"
    "\n"
    "\n"
    "  (defun $wxcontour_plot (&rest args)\n"
    "    (let ((preamble ($wxplot_preamble))\n"
    "	  ($plot_options $plot_options)\n"
    "	  (system-preamble (get-plot-option-string \'$gnuplot_preamble 2))\n"
    "	  (filename (wxplot-filename)))\n"
    "      (when (string= system-preamble \"false\")\n"
    "	(setq system-preamble \"\"))\n"
    "      (setq preamble (format nil \"~a; ~a\" preamble system-preamble))\n"
    "      (dolist (arg args)\n"
    "	(if (and (listp arg) (eql (cadr arg) \'$gnuplot_preamble))\n"
    "	    (setq preamble (format nil \"~a; ~a\" preamble (caddr arg)))))\n"
    "      (apply #\'$contour_plot `(,@args\n"
    "			       ((mlist simp) $gnuplot_term ,(if $wxplot_pngcairo \'$pngcairo \'$png))\n"
    "			       ((mlist simp) $plot_format $gnuplot)\n"
    "			       ((mlist simp) $gnuplot_preamble ,preamble)\n"
    "			       ((mlist simp) $gnuplot_out_file ,filename)))\n"
    "\n"
    "      ($ldisp `((wxxmltag simp) ,filename \"img\")))\n"
    "    \"\")\n"
    "\n"
    "  (defun $show_image (file)\n"
    "    ($ldisp `((wxxmltag simp) ,file \"img\" \"del=\\\"no\\\"\")))\n"
    "\n"
    "  ;;\n"
    "  ;; Port of Barton Willis\'s texput function.\n"
    "  ;;\n"
    "\n"
    "  (defun $wxxmlput (e s &optional tx lbp rbp)\n"
    "\n"
    "    (when (stringp e)\n"
    "      (setf e (define-symbol e)))\n"
    "\n"
    "    (cond (($listp s)\n"
    "	   (setq s (margs s)))\n"
    "	  ((stringp s)\n"
    "	   (setq s (list s)))\n"
    "	  ((atom s)\n"
    "	   (setq s (list (wxxml-stripdollar ($sconcat s))))))\n"
    "\n"
    "    (when (or (null lbp) (not (integerp lbp)))\n"
    "      (setq lbp 180))\n"
    "    (when (or (null rbp) (not (integerp rbp)))\n"
    "      (setq rbp 180))\n"
    "    (cond ((null tx)\n"
    "	   (if (stringp (nth 0 s))\n"
    "	       (putprop e (nth 0 s) \'wxxmlword)\n"
    "             (let ((fun-name (gensym))\n"
    "                   (fun-body\n"
    "                    `(append l\n"
    "                             (list\n"
    "                              (let ((f-x (mfuncall \',s x)))\n"
    "                                (if (stringp f-x)\n"
    "                                    f-x\n"
    "				  (merror \"wxxml: function ~s did not return a string.~%\"\n"
    "					  ($sconcat \',(nth 0 s))))))\n"
    "                             r)))\n"
    "               (setf (symbol-function fun-name) (coerce `(lambda (x l r) ,fun-body) \'function))\n"
    "               (setf (get e \'wxxml) fun-name))))\n"
    "	  ((eq tx \'$matchfix)\n"
    "	   (putprop e \'wxxml-matchfix \'wxxml)\n"
    "	   (cond ((< (length s) 2)\n"
    "		  (merror\n"
    "		   \"Improper 2nd argument to `wxxmlput\' for matchfix operator.\"))\n"
    "		 ((eq (length s) 2)\n"
    "		  (putprop e (list (list (nth 0 s)) (nth 1 s)) \'wxxmlsym))\n"
    "		 (t\n"
    "		  (putprop\n"
    "		   e (list (list (nth 0 s)) (nth 1 s) (nth 2 s)) \'wxxmlsym))))\n"
    "	  ((eq tx \'$prefix)\n"
    "	   (putprop e \'wxxml-prefix \'wxxml)\n"
    "	   (putprop e s \'wxxmlsym)\n"
    "	   (putprop e lbp \'wxxml-lbp)\n"
    "	   (putprop e rbp \'wxxml-rbp))\n"
    "	  ((eq tx \'$infix)\n"
    "	   (putprop e \'wxxml-infix \'wxxml)\n"
    "	   (putprop e  s \'wxxmlsym)\n"
    "	   (putprop e lbp \'wxxml-lbp)\n"
    "	   (putprop e rbp \'wxxml-rbp))\n"
    "	  ((eq tx \'$postfix)\n"
    "	   (putprop e \'wxxml-postfix \'wxxml)\n"
    "	   (putprop e  s \'wxxmlsym)\n"
    "	   (putprop e lbp \'wxxml-lbp))\n"
    "	  (t (merror \"Improper arguments to `wxxmlput\'.\"))))\n"
    "\n"
    ";;;;;;;;;;;;;\n"
    "  ;; Auto-loaded functions\n"
    ";;;;\n"
    "\n"
    "  (setf (get \'$lbfgs \'autoload) \"lbfgs\")\n"
    "  (setf (get \'$lcm \'autoload) \"functs\")\n"
    "\n"
    ";;;;;;;;;;;;;\n"
    "  ;; Statistics functions\n"
    ";;;;\n"
    "\n"
    "  (defvar $draw_compound t)\n"
    "\n"
    "  (defmacro create-statistics-wrapper (fun wxfun)\n"
    "    `(defun ,wxfun (&rest args)\n"
    "       (let (($draw_compound nil) res)\n"
    "	 (declare (special $draw_compound))\n"
    "	 (setq res ($apply \',fun (cons \'(mlist simp) args)))\n"
    "	 ($apply \'$wxdraw2d res))))\n"
    "\n"
    "  (create-statistics-wrapper $histogram $wxhistogram)\n"
    "  (create-statistics-wrapper $scatterplot $wxscatterplot)\n"
    "  (create-statistics-wrapper $barsplot $wxbarsplot)\n"
    "  (create-statistics-wrapper $piechart $wxpiechart)\n"
    "  (create-statistics-wrapper $boxplot $wxboxplot)\n"
    "\n"
    "  (dolist (fun \'($histogram\n"
    "		 $scatterplot\n"
    "		 $barsplot\n"
    "		 $piechart\n"
    "		 $boxplot))\n"
    "    (setf (get fun \'autoload) \"descriptive\"))\n"
    "\n"
    "  (dolist (fun \'($mean\n"
    "		 $median\n"
    "		 $var\n"
    "		 $std\n"
    "		 $test_mean\n"
    "		 $test_means_difference\n"
    "		 $test_normality\n"
    "		 $simple_linear_regression\n"
    "		 $subsample))\n"
    "    (setf (get fun \'autoload) \"stats\"))\n"
    "\n"
    "  (setf (get \'$lsquares_estimates \'autoload) \"lsquares\")\n"
    "\n"
    "  (setf (get \'$to_poly_solve \'autoload) \"to_poly_solve\")\n"
    "\n"
    ";;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;\n"
    "  ;;\n"
    "  ;; Redefine load so that it prints the list of functions\n"
    "  ;; used for autocompletion.\n"
    "  ;;\n"
    ";;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;\n"
    "  (defun symbol-to-xml (s)\n"
    "    (wxxml-fix-string (format nil \"~a\" (maybe-invert-string-case (symbol-name (stripdollar s))))))\n"
    "\n"
    "  (defun print_unit (unit)\n"
    "    (format nil \"<unit>~a</unit>\" (symbol-to-xml unit)))\n"
    "\n"
    "  (defun $print_function (fun)\n"
    "    (let ((fun-name (symbol-to-xml (caar fun)))\n"
    "	  (*print-circle* nil)\n"
    "	  (args (mapcar (lambda (u)\n"
    "			  (cond ((atom u) (symbol-to-xml u))\n"
    "				((eq (caar u) \'mlist)\n"
    "				 ($concat \"[\" (symbol-to-xml\n"
    "					       (if (atom (cadr u)) (cadr u) (cadadr u))) \"]\"))\n"
    "				(t (symbol-to-xml (cadr u)))))\n"
    "			(cdr fun))))\n"
    "      (format nil \"<function>~a</function><template>~a(~{&lt;~a&gt;~^, ~})</template>\" fun-name fun-name args)))\n"
    "\n"
    "  (defun print_value (val)\n"
    "    (format nil \"<value>~a</value>\" (symbol-to-xml val)))\n"
    "\n"
    "  (defun $add_function_template (&rest functs)\n"
    "    (let ((*print-circle* nil))\n"
    "      (format t \"<wxxml-symbols>~{~a~^$~}</wxxml-symbols>\" (mapcar #\'$print_function functs))\n"
    "      (cons \'(mlist simp) functs)))\n"
    "\n"
    "\n"
    "  ;; A function that determines all symbols for autocompletion\n"
    "  (defun wxPrint_autoompletesymbols ()\n"
    "    (format t \"<wxxml-symbols>\")\n"
    "    ;; Function names and rules\n"
    "    (format t \"~{~a~^$~}\"\n"
    "	    (append (mapcar #\'$print_function (cdr ($append $functions $macros)))\n"
    "		    (mapcar #\'print_value (cdr ($append $values $rules)))))\n"
    "    ;; Idea from Robert Dodier:\n"
    "    ;; Variables defined with mdef don\'t appear in $values nor do they in $myoptions\n"
    "    ;; but they appear in *variable-initial-values*\n"
    "    (maphash (lambda (key val)\n"
    "	       (declare (ignore val))\n"
    "	       (if (eq (char (format nil \"~a\" key) 0) #\\$ )\n"
    "		   (format t \"~a\" (print_value key))))\n"
    "	     *variable-initial-values*)\n"
    "\n"
    "    ;; ezunits publishes all known units in a function.\n"
    "    ;; Currently the following lines produce a warning, though, that\n"
    "    ;; $known_units is undefined.\n"
    "    ;;	(if (boundp \'$known_units)\n"
    "    ;;	    (no-warning\n"
    "    ;;	     (format t \"~{~a~^$~}\"\n"
    "    ;;		     (mapcar #\'print_unit (cdr ($known_units))))))\n"
    "    (format t \"</wxxml-symbols>\"))\n"
    "\n"
    ";;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;\n"
    ";;;\n"
    ";;; Communication between wxMaxima and wxMaxima about variables and directories\n"
    ";;;\n"
    "\n"
    ";;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;\n"
    ";;;\n"
    ";;; Communicate the contents of variables to wxMaxima\n"
    "  (defun wx-print-variable (var)\n"
    "    (format t \"<variable>\")\n"
    "    (format t \"<name>~a</name>\" (symbol-to-xml var))\n"
    "    (if (boundp var)\n"
    "	(format t \"<value>~a</value>\" (wxxml-fix-string(eval var))))\n"
    "    (format t \"</variable>\"))\n"
    "\n"
    "  (defun wx-print-variables ()\n"
    "    ;; Flush stdout as this might hinder clisp on MSW from failing to send\n"
    "    ;; network packets in the wrong order\n"
    "    (finish-output)\n"
    "    (format t \"<variables>\")\n"
    "    (wx-print-variable \'$maxima_userdir)\n"
    "    (wx-print-variable \'$maxima_tempdir)\n"
    "    (wx-print-variable \'*maxima-htmldir*)\n"
    "					;  (wx-print-variable \'*maxima-topdir*)\n"
    "    (wx-print-variable \'$gnuplot_command)\n"
    "    (wx-print-variable \'*maxima-demodir*)\n"
    "    (wx-print-variable \'*maxima-sharedir*)\n"
    "    (wx-print-variable \'*autoconf-version*)\n"
    "    (wx-print-variable \'*autoconf-host*)\n"
    "    (format t \"<variable><name>*lisp-name*</name><value>~a</value></variable>\"\n"
    "	    #+sbcl (ensure-readably-printable-string (lisp-implementation-type))\n"
    "	    #-sbcl (lisp-implementation-type))\n"
    "    (format t \"<variable><name>*lisp-version*</name><value>~a</value></variable>\"\n"
    "	    #+sbcl (ensure-readably-printable-string (lisp-implementation-version))\n"
    "	    #-sbcl (lisp-implementation-version))\n"
    "    (format t \"</variables>\")\n"
    "    ;; Flush stdout as this might hinder clisp on MSW from failing to send\n"
    "    ;; network packets in the wrong order\n"
    "    (finish-output)\n"
    "    )\n"
    "\n"
    ";;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;\n"
    "  ;; A function that allows wxMaxima to set maxima\'s current directory to that\n"
    "  ;; of the worksheet.\n"
    "  (defun wx-cd (dir)\n"
    "    (when $wxchangedir\n"
    "      (let ((dir (cond ((pathnamep dir) dir)\n"
    "		       ((stringp dir)\n"
    "			(make-pathname :directory (pathname-directory dir)\n"
    "				       :host (pathname-host dir)\n"
    "				       :device (pathname-device dir)))\n"
    "		       (t (error \"cd(dir): dir must be a string or pathname.\")))))\n"
    "	#+allegro (excl:chdir dir)\n"
    "	#+clisp (ext:cd dir)\n"
    "	#+cmu (setf (ext:default-directory) dir)\n"
    "	#+cormanlisp (ccl:set-current-directory dir)\n"
    "	#+gcl (si::chdir dir)\n"
    "	#+lispworks (hcl:change-directory dir)\n"
    "	#+lucid (lcl:working-directory dir)\n"
    "	#+sbcl (sb-posix:chdir dir)\n"
    "	#+sbcl (setf *default-pathname-defaults* (sb-ext:native-pathname (format nil \"~A~A\" (sb-posix:getcwd) \"/\")))\n"
    "	#+ccl (ccl:cwd dir)\n"
    "	#+ecl (si::chdir dir)\n"
    "       ;;; Officially gcl supports (si:chdir dir), too. But the version\n"
    "       ;;; shipped with debian and ubuntu (at least in Feb 2017) doesn\'t.\n"
    "	#+gcl (xchdir dir)\n"
    "	#+gcl (setf *default-pathname-defaults* dir)\n"
    "\n"
    "	(namestring dir)\n"
    "	(wx-print-variables))))\n"
    "\n"
    ";;;;;;;;;;;;;;;;;;;;;\n"
    "  ;; table_form implementation\n"
    "\n"
    "  (defun make-zeros (n)\n"
    "    (cons \'(mlist simp) (loop for i from 1 to n collect \"\")))\n"
    "  (defun take-first (l n)\n"
    "    (if (= n 0) nil (cons (first l) (take-first (rest l) (- n 1)))))\n"
    "\n"
    "  (defun $table_form (mat &rest opts)\n"
    "    (when (mapatom mat)\n"
    "      ($error \"table_form: the argument should not be an atom.\"))\n"
    "    (setq mat ($args mat))\n"
    "    (unless (every #\'$listp (cdr mat))\n"
    "      (setq mat (cons \'(mlist simp)\n"
    "		      (mapcar (lambda (e) (list \'(mlist simp) e))\n"
    "			      (cdr mat)))))\n"
    "    (setq opts (cons \'(mlist simp) opts))\n"
    "    (let ((row-names ($assoc \'$row_names opts))\n"
    "	  (col-names ($assoc \'$column_names opts))\n"
    "	  (m (apply #\'max (mapcar \'$length (cdr mat))))\n"
    "	  (n (length (cdr mat)))\n"
    "	  (mtrx \'(special)))\n"
    "      (when ($assoc \'$transpose opts)\n"
    "	(rotatef m n))\n"
    "      (when (eq row-names \'$auto)\n"
    "	(setq row-names (cons \'(mlist simp) (loop for i from 1 to n collect i))))\n"
    "      (when (eq col-names \'$auto)\n"
    "	(setq col-names (cons \'(mlist simp) (loop for i from 1 to m collect i))))\n"
    "      (when row-names\n"
    "	(setq row-names ($append row-names (make-zeros (- n ($length row-names)))))\n"
    "	(setq row-names (cons \'(mlist simp) (take-first (cdr row-names) n))))\n"
    "      (when col-names\n"
    "	(setq col-names ($append col-names (make-zeros (- m ($length col-names)))))\n"
    "	(setq col-names (cons \'(mlist simp) (take-first (cdr col-names) m))))\n"
    "      (when (and row-names col-names)\n"
    "	(setq col-names ($cons \"\" col-names)))\n"
    "      (setq mat (cons \'(mlist simp) (mapcar\n"
    "				     (lambda (r) ($append r (make-zeros (- m ($length r)))))\n"
    "				     (cdr mat))))\n"
    "      (setq mat ($apply \'$matrix mat))\n"
    "      (when ($assoc \'$transpose opts)\n"
    "	(setq mat ($transpose mat)))\n"
    "      (when row-names\n"
    "	(setq mat (cons \'($matrix simp)\n"
    "			(mapcar #\'$append (cdr ($transpose row-names)) (cdr mat))))\n"
    "	(setq mtrx (cons \'rownames mtrx)))\n"
    "      (when col-names\n"
    "	(setq mat (cons \'(matrix simp)\n"
    "			(cons col-names (cdr mat))))\n"
    "	(setq mtrx (cons \'colnames mtrx)))\n"
    "      ($ldisp (cons (append \'($matrix simp) mtrx) (cdr mat)))\n"
    "      \'$done))\n"
    "\n"
    "  (putprop \'$table_form t \'evfun)\n"
    "\n"
    "  ;; Load the initial functions (from mac-init.mac)\n"
    "  (let ((*print-circle* nil))\n"
    "    (format t \"<wxxml-symbols>~{~a~^$~}</wxxml-symbols>\"\n"
    "	    (mapcar #\'$print_function (cdr ($append $functions $macros)))))\n"
    "\n"
    "  (no-warning\n"
    "   (defun mredef-check (fnname)\n"
    "     (declare (ignore fnname))\n"
    "     t))\n"
    "\n"
    "  (when ($file_search \"wxmaxima-init\")\n"
    "    ($load \"wxmaxima-init\"))\n"
    "\n"
    ";;;\n"
    ";;; Now that we have loaded the init file we can rewrite of the function load\n"
    ";;; (maxima/src/mload.lisp) to displays functions and variable names after\n"
    ";;; loading a Maxima package so the autocomplete functionality knows which\n"
    ";;; function and variable names exists.\n"
    ";;;\n"
    ";;; We also inform wxMaxima about the name of the package and about the fact\n"
    ";;; if it was loaded by another package.\n"
    ";;;\n"
    "  (no-warning\n"
    "   (defun $load (filename)\n"
    "     (let ((searched-for\n"
    "	    ($file_search1 filename\n"
    "			   \'((mlist) $file_search_maxima $file_search_lisp  )))\n"
    "	   type)\n"
    "       (setq type ($file_type searched-for))\n"
    "       (case type\n"
    "	     (($maxima)\n"
    "	      (format t \"<variables>\")\n"
    "	      (format t \"<variable><name>*wx-load-file-name*</name><value>~a</value></variable>\"\n"
    "		      (wxxml-fix-string filename))\n"
    "	      (format t \"<variable><name>*wx-load-file-start*</name><value>1</value></variable>\")\n"
    "	      (format t \"</variables>\")\n"
    "	      ($batchload searched-for)\n"
    "	      (format t \"<variables>\")\n"
    "	      (format t \"<variable><name>*wx-load-file-start*</name><value>0</value></variable>\")\n"
    "	      (format t \"</variables>\")\n"
    "	      (wxPrint_autoompletesymbols))\n"
    "	     (($lisp $object)\n"
    "	      ;; do something about handling errors\n"
    "	      ;; during loading. Foobar fail act errors.\n"
    "	      (format t \"<variables>\")\n"
    "	      (format t \"<variable><name>*wx-load-file-name*</name><value>~a</value></variable>\"\n"
    "		      (wxxml-fix-string filename))\n"
    "	      (format t \"<variable><name>*wx-load-file-start*</name><value>1</value></variable>\")\n"
    "	      (format t \"</variables>\")\n"
    "	      (no-warning\n"
    "	       (load-and-tell searched-for))\n"
    "	      (format t \"<variables>\")\n"
    "	      (format t \"<variable><name>*wx-load-file-start*</name><value>0</value></variable>\")\n"
    "	      (format t \"</variables>\")\n"
    "	      (wxPrint_autoompletesymbols))\n"
    "	     (t\n"
    "	      (merror \"Maxima bug: Unknown file type ~M\" type)))\n"
    "       searched-for)))\n"
    "\n"
    ";;; A function that loads bitmaps from files as a slideshow.\n"
    ";;; Todo: Replace this function by at least half-way-optimized LISP code.\n"
    "  (progn\n"
    "    (defprop $wxanimate_from_imgfiles t translated)\n"
    "    (add2lnc \'$wxanimate_from_imgfiles $props)\n"
    "    (defmtrfun ($wxanimate_from_imgfiles $any mdefine t nil)\n"
    "      ($x)\n"
    "      (declare (special $x))\n"
    "      (progn\n"
    "	(simplify (mfunction-call $printf t \'\"<mth><slide\"))\n"
    "	(cond\n"
    "	 ((is-boole-check (trd-msymeval $wxanimate_autoplay \'$wxanimate_autoplay))\n"
    "	  (simplify (mfunction-call $printf t \'\" running=\\\"false\\\"\"))))\n"
    "	(cond\n"
    "	 ((like\n"
    "	   (simplify\n"
    "	    `((mfactorial)\n"
    "	      ,(trd-msymeval $wxanimate_framerate \'$wxanimate_framerate)))\n"
    "	   \'$wxanimate_framerate)\n"
    "	  (simplify\n"
    "	   (mfunction-call $printf t \'\" fr=\\\"~d\\\"\"\n"
    "			   (trd-msymeval $wxanimate_framerate\n"
    "					 \'$wxanimate_framerate)))))\n"
    "	(simplify (mfunction-call $printf t \'\">\"))\n"
    "	(do (($i)\n"
    "	     (mdo (cdr $x) (cdr mdo)))\n"
    "	    ((null mdo) \'$done)\n"
    "	    (declare (special $i))\n"
    "	    (setq $i (car mdo))\n"
    "	    (simplify (mfunction-call $printf t \'\"~a;\" $i)))\n"
    "	(simplify (mfunction-call $printf t \'\"</slide></mth>\")))\n"
    "      ))\n"
    "\n"
    "  ;; Publish all new global variables maxima might contain to wxMaxima\'s\n"
    "  ;; autocompletion feature.\n"
    "  (wxPrint_autoompletesymbols)\n"
    "  (wx-print-variables)\n"
    "  (force-output)*/")\n";

  wxStringTokenizer lines(wxMathMl,wxT("\n"));
  while(lines.HasMoreTokens())
  {
    wxString line = lines.GetNextToken();
    wxString lineWithoutComments;
    std::cerr<<"Line1="<<line<<"\n";

    bool stringIs = false;
    wxString::iterator ch = line.begin();
    while (ch < line.end())
    {
      if (*ch == wxT('\\'))
      {
        lineWithoutComments += *ch;
        ch++;
      }
      else
      {
        if (*ch == wxT('\"'))
          stringIs = !stringIs;
        if ((*ch == wxT(';')) && (!stringIs))
          break;
      }
      lineWithoutComments += *ch;
      ch++;
    }
    cmd += lineWithoutComments + " ";
    std::cerr<<"Line2="<<lineWithoutComments<<"\n";
  }
  cmd = wxT(":lisp-quiet ") + cmd + "\n";
  SendMaxima(cmd,false,false);

  cmd = wxEmptyString;
#if defined (__WXMAC__)
  wxString gnuplotbin(wxT("/Applications/Gnuplot.app/Contents/Resources/bin/gnuplot"));
  if (wxFileExists(gnuplotbin))
    cmd += wxT("\n:lisp-quiet (setf $gnuplot_command \"") + gnuplotbin + wxT("\")\n");
#endif
  cmd.Replace(wxT("\\"),wxT("/"));
  SendMaxima(cmd);

  wxString wxmaximaversion_lisp(wxT(GITVERSION));
  wxmaximaversion_lisp.Replace("\\","\\\\");
  wxmaximaversion_lisp.Replace("\"","\\\"");

  SendMaxima(wxString(wxT(":lisp-quiet (setq $wxmaximaversion \"")) +
             wxmaximaversion_lisp + "\")\n");
  SendMaxima(wxString(wxT(":lisp-quiet ($put \'$wxmaxima (read-wxmaxima-version \"")) +
             wxmaximaversion_lisp +
             wxT("\") '$version)\n"));
  SendMaxima(wxString(wxT(":lisp-quiet (setq $wxwidgetsversion \"")) + wxVERSION_STRING + "\")\n");

  ConfigChanged();

  if ((m_evalOnStartup) && (m_console->m_evaluationQueue.Empty()))
  {
    m_evalOnStartup = false;
    m_console->AddDocumentToEvaluationQueue();
  }
}

///--------------------------------------------------------------------------------
///  Getting configuration
///--------------------------------------------------------------------------------

wxString wxMaxima::GetCommand(bool params)
{
  Configuration *configuration = m_console->m_configuration;
  wxString parameters, command = configuration->MaximaLocation();
  wxConfig::Get()->Read(wxT("parameters"), &parameters);

#if defined (__WXMSW__)
  if (!wxFileExists(command)) {
    wxMessageBox(_("wxMaxima could not find Maxima!\n\n"
                   "Please configure wxMaxima with 'Edit->Configure'.\n"
                   "Then start Maxima with 'Maxima->Restart Maxima'."),
                 _("Warning"),
                 wxOK | wxICON_EXCLAMATION);
    SetStatusText(_("Please configure wxMaxima with 'Edit->Configure'."));
    command = wxT("maxima");
  }
#endif
#if defined (__WXMAC__)
  if (command.Right(4) == wxT(".app")) // if pointing to a Maxima.app
    command.Append(wxT("/Contents/Resources/maxima.sh"));
#endif

  if (params) {
    // escape quotes
    command.Replace(wxT("\""), wxT("\\\""));
    // surround with quotes
    return wxT("\"") + command + wxT("\" ") + parameters;
  }
  else {
    return command;
  }
}

///--------------------------------------------------------------------------------
///  Tips and help
///--------------------------------------------------------------------------------

void wxMaxima::ShowTip(bool force)
{
  bool ShowTips = true;
  int tipNum = 0;

  // A block with a local config variable:
  // The config can change between before showing the tooltip and afterwards.
  {
    wxConfig *config = (wxConfig *) wxConfig::Get();
    config->Read(wxT("ShowTips"), &ShowTips);
    if (!ShowTips && !force)
      return;
  }

  TipOfTheDay *tip = new TipOfTheDay(this);
  tip->Show();
}

wxString wxMaxima::GetHelpFile()
{
#if defined __WXMSW__
  wxFileName command;
  wxString chm;
  wxString html;

  command = wxFileName(GetCommand(false));

  chm = wxFindFirstFile(command.GetPathWithSep() + wxT("..\\share\\maxima\\*"), wxDIR);

  if (chm.empty())
    return wxEmptyString;

  html = chm + wxT("\\doc\\html\\");
  chm = chm + wxT("\\doc\\chm\\");

  wxString locale = wxGetApp().m_locale.GetCanonicalName().Left(2);

  wxString tmp = chm + locale + wxT("\\maxima.chm");
  if (wxFileExists(tmp))
    return tmp;

  tmp = chm + wxT("maxima.chm");
  if (wxFileExists(tmp))
    return tmp;

  tmp = html + locale + wxT("\\header.hhp");
  if (wxFileExists(tmp))
    return tmp;

  tmp = html + wxT("header.hhp");
  if (wxFileExists(tmp))
    return tmp;

  return wxEmptyString;
#else
  wxString headerFile;
  wxConfig::Get()->Read(wxT("helpFile"), &headerFile);

  if (headerFile.Length() && wxFileExists(headerFile))
    return headerFile;
  else
    headerFile = wxEmptyString;

  wxString command = GetCommand();
  command += wxT(" -d");
  wxArrayString output;
  wxExecute(command, output, wxEXEC_ASYNC);

  wxString line;
  wxString docdir;
  wxString langsubdir;

  for (unsigned int i = 0; i < output.GetCount(); i++)
  {
    line = output[i];
    if (line.StartsWith(wxT("maxima-htmldir")))
      docdir = line.Mid(15);
    else if (line.StartsWith(wxT("maxima-lang-subdir")))
    {
      langsubdir = line.Mid(19);
      if (langsubdir == wxT("NIL"))
        langsubdir = wxEmptyString;
    }
  }

  if (docdir.Length() == 0)
    return wxEmptyString;

  headerFile = docdir + wxT("/");
  if (langsubdir.Length())
    headerFile += langsubdir + wxT("/");
  headerFile += wxT("header.hhp");

  if (!wxFileExists(headerFile))
    headerFile = docdir + wxT("/header.hhp");

  if (wxFileExists(headerFile))
    wxConfig::Get()->Write(wxT("helpFile"), headerFile);

  return headerFile;
#endif
}

void wxMaxima::ShowHTMLHelp(wxString helpfile, wxString otherhelpfile, wxString keyword)
{
#if defined (__WXMSW__)
  // Cygwin uses /c/something instead of c:/something and passes this path to the
  // web browser - which doesn't support cygwin paths => convert the path to a
  // native windows pathname if needed.
  if(helpfile.Length()>1 && helpfile[1]==wxT('/'))
  {
    helpfile[1]=helpfile[2];
    helpfile[2]=wxT(':');
  }
#endif

  if (!m_htmlHelpInitialized)
  {
    wxFileName otherhelpfilenname(otherhelpfile);
    if (otherhelpfilenname.FileExists())
      m_htmlhelpCtrl.AddBook(otherhelpfile);
    m_htmlhelpCtrl.AddBook(helpfile);
    m_htmlHelpInitialized = true;
  }

  if ((keyword == wxT("%")) ||
      (keyword == wxT(" << Graphics >> ")))
    m_htmlhelpCtrl.DisplayContents();
  else
    m_htmlhelpCtrl.KeywordSearch(keyword, wxHELP_SEARCH_INDEX);
}

#if defined (__WXMSW__)
void wxMaxima::ShowCHMHelp(wxString helpfile,wxString keyword)
{
  if (m_chmhelpFile != helpfile)
    m_chmhelpCtrl.LoadFile(helpfile);

  if ((keyword == wxT("%")) ||
      (keyword == wxT(" << Graphics >> ")) ||
      (keyword.IsEmpty())
    )
    m_chmhelpCtrl.DisplayContents();
  else
    m_chmhelpCtrl.KeywordSearch(keyword, wxHELP_SEARCH_INDEX);
}
#endif

void wxMaxima::ShowWxMaximaHelp()
{
  wxString htmldir = m_console->m_configuration->m_dirStructure.HelpDir();

#if CHM == true
  wxString helpfile = htmldir + wxT("/wxmaxima.chm");
  ShowCHMHelp(helpfile,wxT("%"));
#else
  wxString helpfile = htmldir + wxT("/wxmaxima.html");
#if defined (__WXMSW__)
  // Cygwin uses /c/something instead of c:/something and passes this path to the
  // web browser - which doesn't support cygwin paths => convert the path to a
  // native windows pathname if needed.
  if(helpfile.Length()>1 && helpfile[1]==wxT('/'))
  {
    helpfile[1]=helpfile[2];
    helpfile[2]=wxT(':');
  }
#endif // __WXMSW__
  wxLaunchDefaultBrowser(wxT("file:///") + helpfile);
#endif // CHM=false
}

void wxMaxima::ShowMaximaHelp(wxString keyword)
{
  if(keyword == wxT("wxdraw"))
     keyword = wxT("draw");
  if(keyword == wxT("wxdraw2d"))
     keyword = wxT("draw2d");
  if(keyword == wxT("wxdraw3d"))
     keyword = wxT("draw3d");
  if(keyword == wxT("with_slider_draw"))
     keyword = wxT("draw");
  if(keyword == wxT("with_slider_draw2d"))
     keyword = wxT("draw2d");
  if(keyword == wxT("with_slider_draw3d"))
     keyword = wxT("draw3d");
  wxString MaximaHelpFile = GetHelpFile();
  if (MaximaHelpFile.Length() == 0)
  {
    wxMessageBox(_("wxMaxima could not find help files."
                           "\n\nPlease check your installation."),
                 _("Error"), wxICON_ERROR | wxOK);
    return;
  }

#if defined (__WXMSW__)
  if(wxFileName(MaximaHelpFile).GetFullPath().Right(4)==wxT(".chm"))
    ShowCHMHelp(MaximaHelpFile,keyword);
  else
#endif
  {
    wxString htmldir = m_console->m_configuration->m_dirStructure.HelpDir();
    wxString wxMaximaHelpFile = htmldir;
    if(wxFileExists(htmldir + wxGetApp().m_locale.GetName()+ wxT("/wxmaxima.hhp")))
      wxMaximaHelpFile = htmldir + wxGetApp().m_locale.GetName() + wxT("/wxmaxima.hhp");
    else
      wxMaximaHelpFile = htmldir + wxT("/wxmaxima.hhp");
    ShowHTMLHelp(MaximaHelpFile,wxMaximaHelpFile,keyword);
  }
}

///-------o-------------------------------------------------------------------------
///  Idle event
///--------------------------------------------------------------------------------

void wxMaxima::OnIdle(wxIdleEvent &event)
{
  // On msw sometimes the communication stalls even if there is new data.
  // Communication can be resumed manually by manually triggering
  // listening to socket events from time to time, see
  // https://groups.google.com/forum/m/#!topic/wx-users/fdMyu3AKFRQ
  wxSocketEvent dummyEvent(wxSOCKET_INPUT);
  ClientEvent(dummyEvent);

  // If wxMaxima has to open a file on startup we wait for that until we have
  // a valid draw context for size calculations.
  //
  // The draw context is created on displaying the worksheet for the 1st time
  // and after drawing the worksheet onIdle is called => we won't miss this
  // event when we wait for it here.
  if ((m_console != NULL) && (m_console->m_configuration->GetDC() != NULL) &&
      (m_openFile.Length()))
  {
    wxString file = m_openFile;
    m_openFile = wxEmptyString;
      m_openInitialFileError = !OpenFile(file);

    // After doing such big a thing we should end our idle event and request
    // a new one to be issued once the computer has time for doing real
    // background stuff.
    event.RequestMore();
    return;
  }

  // Update the info what maxima is currently doing
  UpdateStatusMaximaBusy();

  // Update the info how long the evaluation queue is
  if(m_updateEvaluationQueueLengthDisplay)
  {
    if ((m_EvaluationQueueLength > 0) || (m_commandsLeftInCurrentCell >= 1))
    {
      wxString statusLine = wxString::Format(_("%i cells in evaluation queue"),
                                             m_EvaluationQueueLength);
      if (m_commandsLeftInCurrentCell > 1)
        statusLine += wxString::Format(_("; %i commands left in the current cell"),
                                       m_commandsLeftInCurrentCell - 1);
      SetStatusText(statusLine, 0);
    }
    else
    {
      if (m_first)
      {
        if(!m_openInitialFileError)
          SetStatusText(_("Welcome to wxMaxima"), 0);
      }
      else
        SetStatusText(_("Maxima is ready for input."), 0);
      m_openInitialFileError = false;
    }
    m_updateEvaluationQueueLengthDisplay = false;

    // We have shown that we are still alive => If maxima already offers new data
    // we process this data first and then continue with the idle task.
    if(
      (m_console->m_scheduleUpdateToc) || (m_updateControls) || (m_console->RedrawRequested()) ||
      (
         (m_console->m_findDialog != NULL) &&
         (
           (m_oldFindString != m_console->m_findDialog->GetData()->GetFindString()) ||
           (m_oldFindFlags != m_console->m_findDialog->GetData()->GetFlags())
           )
        ) ||
      (m_newStatusText != wxEmptyString) ||
      ((m_xmlInspector != NULL) && (m_xmlInspector->UpdateNeeded()))
      )
      event.RequestMore();
    else
      event.Skip();
    return;
  }

  if(m_console != NULL)
    m_console->RecalculateIfNeeded();

  // Incremental search is done from the idle task. This means that we don't forcefully
  // need to do a new search on every character that is entered into the search box.
  if (m_console->m_findDialog != NULL)
  {
    if (
      (m_oldFindString != m_console->m_findDialog->GetData()->GetFindString()) ||
      (m_oldFindFlags != m_console->m_findDialog->GetData()->GetFlags())
      )
    {

      m_oldFindFlags = m_console->m_findDialog->GetData()->GetFlags();
      m_oldFindString = m_console->m_findDialog->GetData()->GetFindString();

      bool incrementalSearch = true;
        wxConfig::Get()->Read("incrementalSearch", &incrementalSearch);
        if ((incrementalSearch) && (m_console->m_findDialog != NULL))
        {
          m_console->FindIncremental(m_findData.GetFindString(),
                                     m_findData.GetFlags() & wxFR_DOWN,
                                     !(m_findData.GetFlags() & wxFR_MATCHCASE));
        }

        m_console->RequestRedraw();
        event.RequestMore();
        return;
    }
  }


  if(m_console->RedrawIfRequested())
  {
    m_updateControls = true;

    event.RequestMore();
    return;
  }

  // If nothing which is visible has changed nothing that would cause us to need
  // update the menus and toolbars has.
  if (m_updateControls)
  {
    m_updateControls = false;
    wxUpdateUIEvent dummy;
    UpdateMenus(dummy);
    UpdateToolBar(dummy);
    UpdateSlider(dummy);
    if(m_isNamed)
      ResetTitle(m_console->IsSaved());
    else
      ResetTitle(false);

    // This was a half-way lengthy task => Return from the idle task so we can give
    // maxima a chance to deliver new data.
    if((m_console->m_scheduleUpdateToc) ||
       (m_newStatusText != wxEmptyString) ||
       ((m_xmlInspector != NULL) && (m_xmlInspector->UpdateNeeded())))
      event.RequestMore();
    else
      event.Skip();

    return;
  }

  if(m_newStatusText != wxEmptyString)
  {
    SetStatusText(m_newStatusText, 1);
    m_newStatusText = wxEmptyString;

    if((m_console->m_scheduleUpdateToc) ||
       ((m_xmlInspector != NULL) && (m_xmlInspector->UpdateNeeded()))
      )
      event.RequestMore();
    else
      event.Skip();
    return;
  }

  // If we have set the flag that tells us we should update the table of
  // contents sooner or later we should do so now that wxMaxima is idle.
  if (m_console->m_scheduleUpdateToc)
  {
    if (m_console->m_tableOfContents)
    {
      m_console->m_scheduleUpdateToc = false;
      GroupCell *cursorPos;
      cursorPos = m_console->GetHCaret();
      if ((!m_console->HCaretActive()) && (cursorPos == m_console->GetLastCell()))
      {
        if (m_console->GetActiveCell() != NULL)
          cursorPos = dynamic_cast<GroupCell *>(m_console->GetActiveCell()->GetGroup());
        else
          cursorPos = m_console->FirstVisibleGC();
      }
      m_console->m_tableOfContents->UpdateTableOfContents(m_console->GetTree(), cursorPos);
    }
    m_console->m_scheduleUpdateToc = false;

    if((m_xmlInspector != NULL) && (m_xmlInspector->UpdateNeeded()))
      event.RequestMore();
    else
      event.Skip();
    return;
  }
  if((m_xmlInspector != NULL) && (m_xmlInspector->UpdateNeeded()))
    m_xmlInspector->Update();

  UpdateDrawPane();

  // Tell wxWidgets it can process its own idle commands, as well.
  event.Skip();
}

void wxMaxima::UpdateDrawPane()
{
  if(m_drawPane)
  {
    EditorCell *editor = m_console->GetActiveCell();
    if(editor)
    {
      wxString command = m_console->GetActiveCell()->GetFullCommandUnderCursor();
      int dimensions = 0;
      if(command.Contains(wxT("gr2d")))
        dimensions = 2;
      if(command.Contains(wxT("with_slider_draw")))
        dimensions = 2;
      if(command.Contains(wxT("gr3d")))
        dimensions = 3;
      if(command.Contains(wxT("draw2d")))
        dimensions = 2;
      if(command.Contains(wxT("draw3d")))
        dimensions = 3;
      m_drawPane->SetDimensions(dimensions);
    }
    else
      m_drawPane->SetDimensions(0);
  }
  else
  {
    m_drawPane->SetDimensions(0);
  }
}

///--------------------------------------------------------------------------------
///  Menu and button events
///--------------------------------------------------------------------------------

void wxMaxima::MenuCommand(wxString cmd)
{
  bool evaluating = (!m_console->m_evaluationQueue.Empty()) && (m_StatusMaximaBusy == waiting);

  m_console->SetFocus();
  m_console->OpenHCaret(cmd);
  m_console->AddCellToEvaluationQueue(dynamic_cast<GroupCell *>(m_console->GetActiveCell()->GetGroup()));
  if (!evaluating)
    TryEvaluateNextInQueue();
  m_console->RequestRedraw();
}

///--------------------------------------------------------------------------------
///  Menu and button events
///--------------------------------------------------------------------------------

void wxMaxima::PrintMenu(wxCommandEvent &event)
{
  if(m_console != NULL)
    m_console->CloseAutoCompletePopup();

  switch (event.GetId())
  {
    case wxID_PRINT:
    case ToolBar::tb_print:
    {
      wxPrintDialogData printDialogData;
      if (m_printData)
        printDialogData.SetPrintData(*m_printData);
      wxPrinter printer(&printDialogData);
      wxString title(_("wxMaxima document")), suffix;

      if (m_console->m_currentFile.Length())
      {
        wxString suffix;
        wxFileName::SplitPath(m_console->m_currentFile, NULL, NULL, &title, &suffix);
        title << wxT(".") << suffix;
      }

      {
        // Redraws during printing might end up on paper => temporarily block all redraw
        // events for the console
        m_console->Freeze();
        wxEventBlocker blocker(m_console);
        wxBusyCursor crs;
        MathPrintout printout(title, &m_console->m_configuration);
        GroupCell *copy = m_console->CopyTree();
        printout.SetData(copy);
        if (printer.Print(this, &printout, true))
        {
          wxDELETE(m_printData);
          m_printData = new wxPrintData(printer.GetPrintDialogData().GetPrintData());
        }
        m_console->Thaw();
      }
      m_console->RecalculateForce();
      m_console->RequestRedraw();
      break;
    }
  }
}

void wxMaxima::UpdateMenus(wxUpdateUIEvent &WXUNUSED(event))
{
  wxMenuBar *menubar = GetMenuBar();

  if (!m_console)
    return;
  wxASSERT_MSG((!m_console->HCaretActive()) || (m_console->GetActiveCell() == NULL),
               _("Both horizontal and vertical cursor active at the same time"));

  menubar->Enable(menu_copy_from_console, m_console->CanCopy(true));
  menubar->Enable(menu_cut, m_console->CanCut());
  menubar->Enable(menu_copy_tex_from_console, m_console->CanCopy());
  menubar->Enable(MathCtrl::popid_copy_mathml, m_console->CanCopy());
  menubar->Enable(menu_copy_as_bitmap, m_console->CanCopy());
  menubar->Enable(menu_copy_as_svg, m_console->CanCopy());
  #if wxUSE_ENH_METAFILE
  menubar->Enable(menu_copy_as_emf, m_console->CanCopy());
  #endif
  menubar->Enable(menu_copy_as_rtf, m_console->CanCopy());
  menubar->Enable(menu_copy_to_file, m_console->CanCopy());
  menubar->Enable(menu_copy_text_from_console, m_console->CanCopy(true));
  menubar->Enable(menu_select_all, m_console->GetTree() != NULL);
  menubar->Enable(menu_undo, m_console->CanUndo());
  menubar->Enable(menu_redo, m_console->CanRedo());
  menubar->Enable(menu_interrupt_id, m_pid > 0);
  menubar->Enable(MathCtrl::popid_comment_selection,
                  (m_console->GetActiveCell() != NULL) && (m_console->GetActiveCell()->SelectionActive()));
  menubar->Enable(menu_evaluate, (
                    (m_console->GetActiveCell() != NULL) ||
                          (m_console->CellsSelected())
                    )
    );

  menubar->Enable(menu_evaluate_all_visible, m_console->GetTree() != NULL);
  menubar->Enable(ToolBar::tb_evaltillhere,
                  (m_console->GetTree() != NULL) &&
                  (m_console->CanPaste()) &&
                  (m_console->GetHCaret() != NULL)
  );

  menubar->Enable(menu_jumptoerror, !m_console->m_cellPointers.m_errorList.Empty());
  menubar->Enable(menu_save_id, (!m_fileSaved));

  for (int id = menu_pane_math; id <= menu_pane_stats; id++)
    menubar->Check(id, IsPaneDisplayed(static_cast<Event>(id)));
  if (GetToolBar() != NULL)
  {
#if defined __WXMAC__ || defined __WXMSW__
    menubar->Check(menu_show_toolbar, GetToolBar()->IsShown());
#else
    menubar->Check(menu_show_toolbar, true);
#endif
  }
  else
    menubar->Check(menu_show_toolbar, false);

  bool hidecode = !(m_console->m_configuration->ShowCodeCells());
  menubar->Check(ToolBar::tb_hideCode, hidecode);

  if (m_console->GetTree() != NULL)
  {
    menubar->Enable(MathCtrl::popid_divide_cell, m_console->GetActiveCell() != NULL);
    menubar->Enable(MathCtrl::popid_merge_cells, m_console->CanMergeSelection());
    menubar->Enable(wxID_PRINT, true);
  }
  else
  {
    menubar->Enable(MathCtrl::popid_divide_cell, false);
    menubar->Enable(MathCtrl::popid_merge_cells, false);
    menubar->Enable(wxID_PRINT, false);
  }
  double zf = m_console->m_configuration->GetZoomFactor();
  if (zf < Configuration::GetMaxZoomFactor())
    menubar->Enable(MathCtrl::menu_zoom_in, true);
  else
    menubar->Enable(MathCtrl::menu_zoom_in, false);
  if (zf > Configuration::GetMinZoomFactor())
    menubar->Enable(MathCtrl::menu_zoom_out, true);
  else
    menubar->Enable(MathCtrl::menu_zoom_out, false);

}

void wxMaxima::UpdateToolBar(wxUpdateUIEvent &WXUNUSED(event))
{
  if (!m_console->m_mainToolBar)
    return;

  m_console->m_mainToolBar->CanCopy(m_console->CanCopy(true));
  m_console->m_mainToolBar->CanCut(m_console->CanCut());
  m_console->m_mainToolBar->CanSave((!m_fileSaved));
  m_console->m_mainToolBar->CanPrint(m_console->GetTree() != NULL);
  m_console->m_mainToolBar->CanEvalTillHere(
          (m_console->GetTree() != NULL) &&
          (m_console->CanPaste()) &&
          (m_console->GetHCaret() != NULL) &&
          (m_client != NULL)
  );

  // On MSW it seems we cannot change an icon without side-effects that somehow
  // stop the animation => on this OS we have separate icons for the
  // animation start and stop. On the rest of the OSes we use one combined
  // start/stop button instead.
  if (m_console->CanAnimate())
  {
    SlideShow *slideShow = dynamic_cast<SlideShow *>(m_console->GetSelectionStart());
    if (slideShow->AnimationRunning())
      m_console->m_mainToolBar->AnimationButtonState(ToolBar::Running);
    else
      m_console->m_mainToolBar->AnimationButtonState(ToolBar::Stopped);
  }
  else
    m_console->m_mainToolBar->AnimationButtonState(ToolBar::Inactive);

  bool follow = m_console->ScrolledAwayFromEvaluation();
  switch (m_StatusMaximaBusy)
  {
    case userinput:
      m_console->m_mainToolBar->ShowUserInputBitmap();
      m_console->m_mainToolBar->EnableTool(ToolBar::tb_interrupt, true);
      m_console->m_mainToolBar->EnableTool(ToolBar::tb_follow, true);
      break;
    case waiting:
      m_console->m_mainToolBar->ShowFollowBitmap();
      if (m_console->GetWorkingGroup() == NULL)
      {
        m_console->m_mainToolBar->EnableTool(ToolBar::tb_interrupt, false);
        m_console->m_mainToolBar->EnableTool(ToolBar::tb_follow, false);
      }
      break;
    case calculating:
      m_console->m_mainToolBar->ShowFollowBitmap();
      m_console->m_mainToolBar->EnableTool(ToolBar::tb_interrupt, true);
      m_console->m_mainToolBar->EnableTool(ToolBar::tb_follow, follow);
      break;
    case transferring:
      m_console->m_mainToolBar->ShowFollowBitmap();
      m_console->m_mainToolBar->EnableTool(ToolBar::tb_interrupt, true);
      m_console->m_mainToolBar->EnableTool(ToolBar::tb_follow, follow);
      break;
    case parsing:
      m_console->m_mainToolBar->ShowFollowBitmap();
      m_console->m_mainToolBar->EnableTool(ToolBar::tb_interrupt, true);
      m_console->m_mainToolBar->EnableTool(ToolBar::tb_follow, follow);
      break;
    case wait_for_start:
    case disconnected:
      m_console->m_mainToolBar->ShowFollowBitmap();
      m_console->m_mainToolBar->EnableTool(ToolBar::tb_interrupt, false);
      m_console->m_mainToolBar->EnableTool(ToolBar::tb_follow, false);
      break;
    case process_wont_start:
      m_console->m_mainToolBar->ShowFollowBitmap();
      m_console->m_mainToolBar->EnableTool(ToolBar::tb_interrupt, false);
      m_console->m_mainToolBar->EnableTool(ToolBar::tb_follow, false);
      break;
  }
}

wxString wxMaxima::ExtractFirstExpression(wxString entry)
{
  int semicolon = entry.Find(';');
  int dollar = entry.Find('$');
  bool semiFound = (semicolon != wxNOT_FOUND);
  bool dollarFound = (dollar != wxNOT_FOUND);

  int index;
  if (semiFound && dollarFound)
    index = MIN(semicolon, dollar);
  else if (semiFound && !dollarFound)
    index = semicolon;
  else if (!semiFound && dollarFound)
    index = dollar;
  else // neither semicolon nor dollar found
    index = entry.Length();

  return entry.SubString(0, index - 1);
}

wxString wxMaxima::GetDefaultEntry()
{
  if (m_console->CanCopy(true))
    return (m_console->GetString()).Trim().Trim(false);
  if (m_console->GetActiveCell() != NULL)
    return ExtractFirstExpression(m_console->GetActiveCell()->ToString());
  return wxT("%");
}

bool wxMaxima::OpenFile(wxString file, wxString cmd)
{
  bool retval = true;
  if (file.Length() && wxFileExists(file))
  {
    m_lastPath = wxPathOnly(file);
    wxString unixFilename(file);
#if defined __WXMSW__
    unixFilename.Replace(wxT("\\"), wxT("/"));
#endif

    if (cmd.Length() > 0)
    {
      MenuCommand(cmd + wxT("(\"") + unixFilename + wxT("\")$"));
      if(cmd == wxT("load"))
      {
        ReReadConfig();
        m_recentPackages.AddDocument(unixFilename);
        ReReadConfig();
      }
    }
    else if (file.Right(4).Lower() == wxT(".wxm"))
    {
      retval = OpenWXMFile(file, m_console);
      if(retval)
      {
        ReReadConfig();
        m_recentDocuments.AddDocument(file);
        ReReadConfig();
      }
    }

    else if (file.Right(4).Lower() == wxT(".mac"))
    {
      retval = OpenMACFile(file, m_console);
      if(retval)
      {
        ReReadConfig();
        m_recentDocuments.AddDocument(file);
        ReReadConfig();
      }
    }
    else if (file.Right(4).Lower() == wxT(".out"))
    {
      retval = OpenMACFile(file, m_console);
      if(retval)
      {
        ReReadConfig();
        m_recentDocuments.AddDocument(file);
        ReReadConfig();
      }
    }

    else if (file.Right(5).Lower() == wxT(".wxmx"))
    {
      retval = OpenWXMXFile(file, m_console);
      if(retval)
      {
        ReReadConfig();
        m_recentDocuments.AddDocument(file);
        ReReadConfig();
      }
    }

    else if (file.Right(4).Lower() == wxT(".zip"))
    {
      retval = OpenWXMXFile(file, m_console);
      if(retval)
      {
        ReReadConfig();
        m_recentDocuments.AddDocument(file);
        ReReadConfig();
      }
    }

    else if (file.Right(4).Lower() == wxT(".dem"))
    {
      MenuCommand(wxT("demo(\"") + unixFilename + wxT("\")$"));
      ReReadConfig();
      m_recentPackages.AddDocument(file);
      ReReadConfig();
    }

    else if (file.Right(4).Lower() == wxT(".xml"))
      retval = OpenXML(file, m_console); // clearDocument = true

    else
    {
      MenuCommand(wxT("load(\"") + unixFilename + wxT("\")$"));
      ReReadConfig();
      m_recentPackages.AddDocument(unixFilename);
      ReReadConfig();
    }

    m_isNamed = true;
  }
  else
    retval = false;

  UpdateRecentDocuments();

  if ((m_console->m_configuration->AutoSaveInterval() > 0) && (m_console->m_currentFile.Length() > 0))
    m_autoSaveTimer.StartOnce(m_console->m_configuration->AutoSaveInterval());

  if (m_console)m_console->TreeUndo_ClearBuffers();

  wxConfig *config = (wxConfig *) wxConfig::Get();
  bool wxcd = true;
  config->Read(wxT("wxcd"), &wxcd);
  if (wxcd)
  {
    m_configCommands += wxT(":lisp-quiet (setq $wxchangedir t)\n");
    if (m_console->m_currentFile != wxEmptyString)
    {
      wxString filename(m_console->m_currentFile);
      SetCWD(filename);
    }
  }
  if (m_console->m_tableOfContents != NULL)
  {
    m_console->m_scheduleUpdateToc = false;
    m_console->m_tableOfContents->UpdateTableOfContents(m_console->GetTree(), m_console->GetHCaret());
  }
  m_console->RequestRedraw();

  if(!retval)
    SetStatusText(wxString::Format("Errors trying to open the file %s.", file),0);

  return retval;
}

bool wxMaxima::SaveFile(bool forceSave)
{
  // Show a busy cursor as long as we export a file.
  wxBusyCursor crs;

  wxString file = m_console->m_currentFile;
  wxString fileExt = wxT("wxmx");
  int ext = 0;

  wxConfig *config = (wxConfig *) wxConfig::Get();

  if (file.Length() == 0 || forceSave)
  {
    if (file.Length() == 0)
    {
      config->Read(wxT("defaultExt"), &fileExt);
      file = _("untitled") + wxT(".") + fileExt;
    }
    else
      wxFileName::SplitPath(file, NULL, NULL, &file, &fileExt);

    wxFileDialog fileDialog(this,
                            _("Save As"), m_lastPath,
                            file,
                            _("Whole document (*.wxmx)|*.wxmx|"
                                      "The input, readable by load() (maxima > 5.38) (*.wxm)|*.wxm"),
                            wxFD_SAVE | wxFD_OVERWRITE_PROMPT);

    if (fileExt == wxT("wxmx"))
      fileDialog.SetFilterIndex(0);
    else if (fileExt == wxT("wxm"))
      fileDialog.SetFilterIndex(1);
    else
    {
      fileDialog.SetFilterIndex(0);
      fileExt = wxT("wxmx");
    }
    if (fileDialog.ShowModal() == wxID_OK)
    {
      file = fileDialog.GetPath();
      ext = fileDialog.GetFilterIndex();
    }
    else
    {
      if ((m_console->m_configuration->AutoSaveInterval() > 0) && (m_console->m_currentFile.Length() > 0))
        m_autoSaveTimer.StartOnce(m_console->m_configuration->AutoSaveInterval());
      return false;
    }
  }

  if (file.Length())
  {
    if ((file.Right(4) != wxT(".wxm")) &&
        (file.Right(5) != wxT(".wxmx"))
            )
    {
      switch (ext)
      {
        case 0:
          file += wxT(".wxmx");
          break;
        case 1:
          file += wxT(".wxm");
          break;
        default:
          file += wxT(".wxmx");
      }
    }

    StatusSaveStart();
    config->Write(wxT("defaultExt"), wxT("wxmx"));

    m_console->m_currentFile = file;
    m_lastPath = wxPathOnly(file);
    if (file.Right(5) == wxT(".wxmx"))
    {
      if (!m_console->ExportToWXMX(file))
      {
        StatusSaveFailed();
        if (m_console->m_configuration->AutoSaveInterval() > 0)
          m_autoSaveTimer.StartOnce(m_console->m_configuration->AutoSaveInterval());
        return false;
      }
      else
        m_isNamed = true;

    }
    else
    {
      if (!m_console->ExportToMAC(file))
      {
        config->Write(wxT("defaultExt"), wxT("wxm"));

        StatusSaveFailed();
        if (m_console->m_configuration->AutoSaveInterval() > 0)
          m_autoSaveTimer.StartOnce(m_console->m_configuration->AutoSaveInterval() > 0);
        return false;
      }
      else
        m_isNamed = true;
    }

    m_recentDocuments.AddDocument(file);
    SetCWD(file);

    if (m_console->m_configuration->AutoSaveInterval() > 0)
      m_autoSaveTimer.StartOnce(m_console->m_configuration->AutoSaveInterval() > 0);
    StatusSaveFinished();
    RemoveTempAutosavefile();
    UpdateRecentDocuments();
    return true;
  }

  if (m_console->m_configuration->AutoSaveInterval() > 0)
    m_autoSaveTimer.StartOnce(m_console->m_configuration->AutoSaveInterval() > 0);

  return false;
}

void wxMaxima::ReadStdErr()
{
  // Maxima will never send us any data via stderr after it has finished
  // starting up and will send data via stdout only in rare cases:
  // It rather sends us the data over the network.
  //
  // If something is severely broken this might not be true, though, and we want
  // to inform the user about it.

  if (m_process == NULL) return;

  if (m_process->IsInputAvailable())
  {
    wxASSERT_MSG(m_maximaStdout != NULL, wxT("Bug: Trying to read from maxima but don't have a input stream"));
    wxTextInputStream istrm(*m_maximaStdout, wxT('\t'), wxConvAuto(wxFONTENCODING_UTF8));
    wxString o;
    wxChar ch;
    int len = 0;
    while (((ch = istrm.GetChar()) != wxT('\0')) && (m_maximaStdout->CanRead()))
    {
      o += ch;
      len++;
    }

    wxString o_trimmed = o;
    o_trimmed.Trim();

    o = _("Message from the stdout of Maxima: ") + o;
    if ((o_trimmed != wxEmptyString) && (!o.StartsWith("Connecting Maxima to server on port")) &&
        (!m_first))
      DoRawConsoleAppend(o, MC_TYPE_DEFAULT);
  }
  if (m_process->IsErrorAvailable())
  {
    wxASSERT_MSG(m_maximaStderr != NULL, wxT("Bug: Trying to read from maxima but don't have a error input stream"));
    wxTextInputStream istrm(*m_maximaStderr, wxT('\t'), wxConvAuto(wxFONTENCODING_UTF8));
    wxString o;
    wxChar ch;
    int len = 0;
    while (((ch = istrm.GetChar()) != wxT('\0')) && (m_maximaStderr->CanRead()))
    {
      o += ch;
      len++;
    }

    wxString o_trimmed = o;
    o_trimmed.Trim();

    o = wxT("Message from maxima's stderr stream: ") + o;

    if((o != wxT("Message from maxima's stderr stream: End of animation sequence")) &&
       !o.Contains("frames in animation sequence") && (o_trimmed != wxEmptyString) &&
       (o.Length() > 1))
    {
      DoRawConsoleAppend(o, MC_TYPE_ERROR);
      if(!AbortOnError())
        TryEvaluateNextInQueue();
      m_console->m_cellPointers.m_errorList.Add(m_console->GetWorkingGroup(true));
    }
    else
      DoRawConsoleAppend(o, MC_TYPE_DEFAULT);
  }
}

bool wxMaxima::AbortOnError()
{
  // If maxima did output something it defintively has stopped.
  // The question is now if we want to try to send it something new to evaluate.
  bool abortOnError = false;
  wxConfig::Get()->Read(wxT("abortOnError"), &abortOnError);
  ExitAfterEval(false);
  EvalOnStartup(false);

  if (m_console->m_notificationMessage != NULL)
  {
    if (m_console->GetWorkingGroup(true) !=
        m_console->m_notificationMessage->m_errorNotificationCell)
      m_console->SetNotification(_("Maxima has issued an error!"),wxICON_ERROR);
    m_console->m_notificationMessage->m_errorNotificationCell = m_console->GetWorkingGroup(true);
  }

  m_exitAfterEval = false;
  if (abortOnError)
  {
    m_console->m_evaluationQueue.Clear();
    // Inform the user that the evaluation queue is empty.
    EvaluationQueueLength(0);
    m_console->ScrollToError();
    return true;
  }
  else
    return false;
}

long long wxMaxima::GetTotalCpuTime()
{
#ifdef __WXMSW__
  FILETIME systemtime;
  GetSystemTimeAsFileTime(&systemtime);
  return (long long) systemtime.dwLowDateTime +
        (2^32)*((long long) systemtime.dwHighDateTime);
#else
  int CpuJiffies = 0;
  if(wxFileExists("/proc/stat"))
  {
    wxFileInputStream input("/proc/stat");
    if(input.IsOk())
    {
      wxTextInputStream text(input, wxT('\t'), wxConvAuto(wxFONTENCODING_UTF8));
      wxString line;
      while((!input.Eof()) && (!line.StartsWith("cpu ")))
        line = text.ReadLine();

      // Strip the "cpu" from the line
      line = line.Right(line.Length() - 4);
      line.Trim(false);
      wxStringTokenizer tokens(line,wxT(" "));
      for(int i = 0; i < 3; i++)
      {
        if(tokens.HasMoreTokens())
        {
          long additionalJiffies;
          if(!tokens.GetNextToken().ToLong(&additionalJiffies))
            return -1;
          CpuJiffies += additionalJiffies;
        }
        else
          return -1;
      }
    }
  }
  return CpuJiffies;
#endif
}

long long wxMaxima::GetMaximaCpuTime()
{
  #ifdef __WXMSW__
  HANDLE maximaHandle = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, false, m_pid);
  if(maximaHandle != NULL)
  {
    FILETIME creationTime, exitTime, kernelTime, userTime;
    if(GetProcessTimes(maximaHandle, &creationTime, &exitTime, &kernelTime, &userTime))
    {
      long long retval =
        (long long)kernelTime.dwLowDateTime + userTime.dwLowDateTime +
        (2^32)*((long long)kernelTime.dwHighDateTime + userTime.dwHighDateTime);
      CloseHandle(maximaHandle);

      return retval;
    }
  }
  #endif
  int maximaJiffies = 0;
  wxString statFileName = wxString::Format("/proc/%li/stat",m_pid);
  if(wxFileExists(statFileName))
  {
    wxFileInputStream input(statFileName);
    if(input.IsOk())
    {
      wxTextInputStream text(input, wxT('\t'), wxConvAuto(wxFONTENCODING_UTF8));
      wxString line = text.ReadLine();

      wxStringTokenizer tokens(line,wxT(" "));
      for(int i = 0; i < 13; i++)
      {
        if(tokens.HasMoreTokens())
          tokens.GetNextToken();
        else return -1;
      }

      for(int i = 0; i < 4; i++)
      {
        {
          if(tokens.HasMoreTokens())
          {
            long additionalJiffies;
            if(!tokens.GetNextToken().ToLong(&additionalJiffies))
            {
              maximaJiffies = -1;
              break;
            }
            maximaJiffies += additionalJiffies;
          }
          else return -1;
        }
      }
    }
  }
  return maximaJiffies;
}

double wxMaxima::GetMaximaCPUPercentage()
{

  int CpuJiffies = GetTotalCpuTime();
  if(CpuJiffies < 0)
    return -1;

  // If no time has passed since the last call to this function the number of CPU cycles
  // per timespan is infinite - and this function will cause an error if we don't abort
  // it now.
  if(CpuJiffies == m_cpuTotalJiffies_old)
    return -1;

  if(CpuJiffies <= m_cpuTotalJiffies_old)
  {
    m_cpuTotalJiffies_old = CpuJiffies;
    return -1;
  }

  int maximaJiffies = GetMaximaCpuTime();
  if(maximaJiffies < 0)
    return -1;

  double retval =
    (double)(maximaJiffies - m_maximaJiffies_old)/(CpuJiffies - m_cpuTotalJiffies_old) * 100;

  m_maximaJiffies_old = maximaJiffies;
  m_cpuTotalJiffies_old = CpuJiffies;
  return retval;
}

void wxMaxima::OnTimerEvent(wxTimerEvent &event)
{
  switch (event.GetId())
  {
    case MAXIMA_STDOUT_POLL_ID:
      ReadStdErr();

      if (m_process != NULL)
      {
        // The atexit() of maxima informs us if the process dies. But it sometimes doesn't do
        // so if it dies due to an out of memory => Periodically check if it really lives.
        if (!wxProcess::Exists(m_process->GetPid()))
        {
          wxProcessEvent *processEvent;
          processEvent = new wxProcessEvent();
          GetEventHandler()->QueueEvent(processEvent);
        }

        double cpuPercentage = GetMaximaCPUPercentage();
        m_statusBar->SetMaximaCPUPercentage(cpuPercentage);

        if((m_process != NULL) && (m_pid > 0) &&
           ((cpuPercentage > 0) || (m_StatusMaximaBusy_next != waiting)))
          m_maximaStdoutPollTimer.StartOnce(MAXIMAPOLLMSECS);
      }

      break;
    case KEYBOARD_INACTIVITY_TIMER_ID:
    case AUTO_SAVE_TIMER_ID:
      if ((!m_console->m_keyboardInactiveTimer.IsRunning()) && (!m_autoSaveTimer.IsRunning()))
      {
        if (m_console->m_configuration->AutoSaveInterval() > 10000)
        {
          if(SaveNecessary())
          {
            if ((m_console->m_currentFile.Length() > 0))
            {
              // Automatically safe the file for the user making it seem like the file
              // is always saved -
              SaveFile(false);
            }
            else
            {
              // The file hasn't been given a name yet.
              // Save the file and remember the file name.
              wxString name = GetTempAutosavefileName();
              m_console->ExportToWXMX(name);
              RegisterAutoSaveFile();
              m_fileSaved = false;
            }
          }

          m_autoSaveTimer.StartOnce(m_console->m_configuration->AutoSaveInterval());
        }
      }
      break;
  }
}

void wxMaxima::FileMenu(wxCommandEvent &event)
{
  if(m_console != NULL)
    m_console->CloseAutoCompletePopup();

  wxString expr = GetDefaultEntry();
  wxString cmd;
  bool forceSave = false;
#if defined __WXMSW__
  wxString b = wxT("\\");
  wxString f = wxT("/");
#endif

  switch (event.GetId())
  {
  case mac_closeId:
    Close();
    break;

    case ToolBar::tb_open:
    case menu_open_id:
    {
      if (SaveNecessary())
      {
        int close = SaveDocumentP();

        if (close == wxID_CANCEL)
          return;

        if (close == wxID_YES)
        {
          if (!SaveFile())
            return;
        }
      }

      wxString file = wxFileSelector(_("Open"), m_lastPath,
                                     wxEmptyString, wxEmptyString,
                                     _("All openable types (*.wxm, *.wxmx, *.mac, *.out, *.xml)|*.wxm;*.wxmx;*.mac;*.out;*.xml|"
                                      "wxMaxima document (*.wxm, *.wxmx)|*.wxm;*.wxmx|"
                                      "Maxima session (*.mac)|*.mac|"
                                      "Xmaxima session (*.out)|*.out|"
                                      "xml from broken .wxmx (*.xml)|*.xml"),
                                     wxFD_OPEN);

      // On the mac the "File/New" menu item by default opens a new window instead od
      // reusing the old one.
      #ifdef __WXMAC__
      if(m_console->IsEmpty())
        OpenFile(file,wxEmptyString);
      else
        wxGetApp().NewWindow(file);
      #else
      OpenFile(file,wxEmptyString);
      #endif
    }
      break;

    case menu_save_as_id:
      forceSave = true;
      m_fileSaved = false;

    case ToolBar::tb_save:
    case menu_save_id:
      SaveFile(forceSave);
      // Seems like resetting the title on "file/save as" is a little bit
      // sluggish, otherwise.
      ResetTitle(m_console->IsSaved(), true);
      break;

    case menu_export_html:
    {
      // Determine a sane default file name;
      wxString file = m_console->m_currentFile;

      if (file.Length() == 0)
        file = _("untitled");
      else
        wxFileName::SplitPath(file, NULL, NULL, &file, NULL);

      wxString fileExt = "html";
      wxConfig::Get()->Read(wxT("defaultExportExt"), &fileExt);

      wxFileDialog fileDialog(this,
                              _("Export"), m_lastPath,
                              file + wxT(".") + fileExt,
                              _("HTML file (*.html)|*.html|"
                                        "maxima batch file (*.mac)|*.mac|"
                                        "pdfLaTeX file (*.tex)|*.tex"
                              ),
                              wxFD_SAVE | wxFD_OVERWRITE_PROMPT);

      if (fileExt == wxT("html"))
        fileDialog.SetFilterIndex(0);
      else if (fileExt == wxT("mac"))
        fileDialog.SetFilterIndex(1);
      else
        fileDialog.SetFilterIndex(2);

      if (fileDialog.ShowModal() == wxID_OK)
      {
        file = fileDialog.GetPath();
        if (file.Length())
        {
          int ext = fileDialog.GetFilterIndex();
          if ((file.Right(5) != wxT(".html")) &&
              (file.Right(4) != wxT(".mac")) &&
              (file.Right(4) != wxT(".tex"))
                  )
          {
            switch (ext)
            {
              case 0:
                file += wxT(".html");
                break;
              case 1:
                file += wxT(".mac");
                break;
              case 2:
                file += wxT(".tex");
                break;
              default:
                file += wxT(".html");
            }
          }

          if (file.Right(4) == wxT(".tex"))
          {
            StatusExportStart();

            fileExt = wxT("tex");
            // Show a busy cursor as long as we export a file.
            wxBusyCursor crs;
            if (!m_console->ExportToTeX(file))
            {
              wxMessageBox(_("Exporting to TeX failed!"), _("Error!"),
                           wxOK);
              StatusExportFailed();
            }
            else
              StatusExportFinished();
          }
          else if (file.Right(4) == wxT(".mac"))
          {
            StatusExportStart();

            // Show a busy cursor as long as we export a file.
            wxBusyCursor crs;
            fileExt = wxT("mac");
            if (!m_console->ExportToMAC(file))
            {
              wxMessageBox(_("Exporting to maxima batch file failed!"), _("Error!"),
                           wxOK);
              StatusExportFailed();
            }
            else
              StatusExportFinished();
          }
          else
          {
            StatusExportStart();

            // Show a busy cursor as long as we export a file.
            wxBusyCursor crs;
            fileExt = wxT("html");
            if (!m_console->ExportToHTML(file))
            {
              wxMessageBox(_("Exporting to HTML failed!"), _("Error!"),
                           wxOK);
              StatusExportFailed();
            }
            else
              StatusExportFinished();
          }
          if (m_console->m_configuration->AutoSaveInterval() > 10000)
            m_autoSaveTimer.StartOnce(m_console->m_configuration->AutoSaveInterval());

          wxFileName::SplitPath(file, NULL, NULL, NULL, &fileExt);
          wxConfig::Get()->Write(wxT("defaultExportExt"), fileExt);
        }
      }
    }
      break;

    case menu_load_id:
    {
      wxString file = wxFileSelector(_("Load Package"), m_lastPath,
                                     wxEmptyString, wxEmptyString,
                                     _("Maxima package (*.mac)|*.mac|"
                                               "Lisp package (*.lisp)|*.lisp|All|*"),
                                     wxFD_OPEN);
      OpenFile(file, wxT("load"));
    }
      break;

    case menu_batch_id:
    {
      wxString file = wxFileSelector(_("Batch File"), m_lastPath,
                                     wxEmptyString, wxEmptyString,
                                     _("Maxima package (*.mac)|*.mac"),
                                     wxFD_OPEN);
      OpenFile(file, wxT("batch"));
    }
      break;

    case wxID_EXIT:
      Close();
      break;

    case ToolBar::tb_animation_startStop:
      if (m_console->CanAnimate())
      {
        SlideShow *slideShow = dynamic_cast<SlideShow *>(m_console->GetSelectionStart());
        if (slideShow->AnimationRunning())
          m_console->Animate(false);
        else
          m_console->Animate(true);
      }
      break;

    case MathCtrl::popid_animation_start:
      if (m_console->CanAnimate())
      {
        SlideShow *slideShow = dynamic_cast<SlideShow *>(m_console->GetSelectionStart());
        slideShow->AnimationRunning(true);
      }
      break;

    default:
      break;
  }
  m_console->RequestRedraw();
}

void wxMaxima::EditMenu(wxCommandEvent &event)
{
  if(m_console != NULL)
    m_console->CloseAutoCompletePopup();

  //if (m_console->m_findDialog != NULL) {
  //  event.Skip();
  //  return;
  //}

  switch (event.GetId())
  {
  case MathCtrl::popid_popup_gnuplot:
  {
    if(m_console->m_cellPointers.m_selectionStart == NULL)
      return;
    if(m_console->m_cellPointers.m_selectionStart->GetType() != MC_TYPE_IMAGE)
      return;
    if(m_console->m_cellPointers.m_selectionStart != m_console->m_cellPointers.m_selectionEnd)
      return;

    wxString gnuplotSource =
      dynamic_cast<ImgCell *>(m_console->m_cellPointers.m_selectionStart)->GnuplotSource();
    if(gnuplotSource == wxEmptyString)
      return;

    if(!wxFileExists(gnuplotSource))
      return;

    // Create a gnuplot file that doesn't select a terminal and output file
    {
      wxFileInputStream input(gnuplotSource);
      if(!input.IsOk())
        return;
      wxTextInputStream textIn(input, wxT('\t'), wxConvAuto(wxFONTENCODING_UTF8));

      wxFileOutputStream output(gnuplotSource + wxT(".popout"));
      if(!output.IsOk())
        return;
      wxTextOutputStream textOut(output);

      textIn.ReadLine();textIn.ReadLine();

      wxString line;
      while(!input.Eof())
      {
        line = textIn.ReadLine();
        textOut << line + wxT("\n");
      }
      // tell gnuplot to wait for the window to close - or for 10 minutex
      // if gnuplot is too old to understand that.
      textOut<<"if(GPVAL_VERSION >= 5.0) bind \"Close\" \"exit gnuplot\"\n";
      textOut<<"if(GPVAL_VERSION >= 5.0) pause mouse close; else pause 600\n";
      textOut<<"quit\n";
   textOut.Flush();
    }

    // Find gnuplot
    wxPathList pathlist;
    pathlist.AddEnvList(wxT("PATH"));
    pathlist.Add(wxFileName(wxStandardPaths::Get().GetExecutablePath()).GetPath());
    pathlist.Add(wxFileName(wxStandardPaths::Get().GetExecutablePath()).GetPath()+"/../");
    pathlist.Add(wxFileName(wxStandardPaths::Get().GetExecutablePath()).GetPath()+"/../gnuplot");
    pathlist.Add(wxFileName(wxStandardPaths::Get().GetExecutablePath()).GetPath()+"/../gnuplot/bin");
    wxString gnuplot_binary = pathlist.FindAbsoluteValidPath(wxT("gnuplot"));
    if(gnuplot_binary == wxEmptyString)
      gnuplot_binary = pathlist.FindAbsoluteValidPath(wxT("gnuplot.exe"));
    if(gnuplot_binary == wxEmptyString)
      gnuplot_binary = pathlist.FindAbsoluteValidPath(wxT("gnuplot.app"));
    if(gnuplot_binary == wxEmptyString)
      gnuplot_binary = wxT("gnuplot");
    // Execute gnuplot
    wxString cmdline = gnuplot_binary + wxT(" " + gnuplotSource + wxT(".popout"));
    wxLogMessage(_("Running gnuplot as: " + cmdline));

    m_gnuplotProcess = new wxProcess(this, gnuplot_process_id);
    if (wxExecute(cmdline,
                  wxEXEC_ASYNC|wxEXEC_SHOW_CONSOLE,
                  m_gnuplotProcess) < 0)
      wxLogMessage(_("Cannot start gnuplot"));
    break;
  }
  case wxID_PREFERENCES:
  case ToolBar::tb_pref:
  {
    wxConfigBase *config = wxConfig::Get();
    // wxGTK uses wxFileConf. ...and wxFileConf loads the config file only once
    // on inintialisation => Let's reload the config file before entering the
    // config dialogue.
    ReReadConfig();
    config = wxConfig::Get();

    ConfigDialogue *configW = new ConfigDialogue(this, m_console->m_configuration);
    configW->Centre(wxBOTH);
    if (configW->ShowModal() == wxID_OK)
    {
      configW->WriteSettings();
      // Write the changes in the configuration to the disk.
      config->Flush();
  // Refresh the display as the settings that affect it might have changed.
      m_console->m_configuration->ReadStyle();
      m_console->RecalculateForce();
      m_console->RequestRedraw();
      ConfigChanged();
    }

    configW->Destroy();
    break;
  }
  case ToolBar::tb_copy:
  case menu_copy_from_console:
    if (m_console->CanCopy(true))
      m_console->Copy();
    break;
  case menu_copy_text_from_console:
    if (m_console->CanCopy(true))
      m_console->Copy(true);
    break;
  case ToolBar::tb_cut:
  case menu_cut:
    if (m_console->CanCut())
      m_console->CutToClipboard();
    break;
  case menu_select_all:
  case ToolBar::tb_select_all:
    m_console->SelectAll();
    break;
  case ToolBar::tb_paste:
  case menu_paste:
    if (m_console->CanPaste())
      m_console->PasteFromClipboard();
    break;
  case menu_undo:
    if (m_console->CanUndo())
      m_console->Undo();
    break;
  case menu_redo:
    if (m_console->CanRedo())
      m_console->Redo();
    break;
  case menu_copy_tex_from_console:
    if (m_console->CanCopy())
      m_console->CopyTeX();
    break;
  case MathCtrl::popid_copy_mathml:
    if (m_console->CanCopy())
      m_console->CopyMathML();
    break;
  case menu_copy_as_bitmap:
    if (m_console->CanCopy())
      m_console->CopyBitmap();
    break;
  case menu_copy_as_svg:
    if (m_console->CanCopy())
      m_console->CopySVG();
    break;
#if wxUSE_ENH_METAFILE
  case menu_copy_as_emf:
    if (m_console->CanCopy())
      m_console->CopyEMF();
    break;
#endif
  case menu_copy_as_rtf:
    if (m_console->CanCopy())
      m_console->CopyRTF();
    break;
  case menu_copy_to_file:
  {
    wxString file = wxFileSelector(_("Save Selection to Image"), m_lastPath,
                                   wxT("image.png"), wxT("png"),
                                   _("PNG image (*.png)|*.png|"
                                     "JPEG image (*.jpg)|*.jpg|"
                                     "Windows bitmap (*.bmp)|*.bmp|"
                                     "Portable animap (*.pnm)|*.pnm|"
                                     "Tagged image file format (*.tif)|*.tif|"
                                     "X pixmap (*.xpm)|*.xpm"
                                     ),
                                   wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
      if (file.Length())
      {
        m_console->CopyToFile(file);
        m_lastPath = wxPathOnly(file);
      }
    }
      break;
    case MathCtrl::popid_delete:
      if (m_console->CanDeleteSelection())
      {
        m_console->DeleteSelection();
        m_console->Recalculate();
        m_console->RequestRedraw();
        return;
      }
      break;
    case MathCtrl::menu_zoom_in:
      m_console->SetZoomFactor(m_console->m_configuration->GetZoomFactor() + 0.1);
      break;
    case MathCtrl::menu_zoom_out:
      m_console->SetZoomFactor(m_console->m_configuration->GetZoomFactor() - 0.1);
      break;
    case menu_zoom_80:
      m_console->SetZoomFactor(0.8);
      break;
    case menu_zoom_100:
      m_console->SetZoomFactor(1.0);
      break;
    case menu_zoom_120:
      m_console->SetZoomFactor(1.2);
      break;
    case menu_zoom_150:
      m_console->SetZoomFactor(1.5);
      break;
    case menu_zoom_200:
      m_console->SetZoomFactor(2.0);
      break;
    case menu_zoom_300:
      m_console->SetZoomFactor(3.0);
      break;
    case menu_math_as_1D_ASCII:
      MenuCommand(wxT("set_display('none)$"));
      break;
    case menu_math_as_2D_ASCII:
      MenuCommand(wxT("set_display('ascii)$"));
      break;
    case menu_math_as_graphics:
      MenuCommand(wxT("set_display('xml)$"));
      break;
    case menu_noAutosubscript:
      MenuCommand(wxT("wxsubscripts: false$"));
      break;
    case menu_defaultAutosubscript:
      MenuCommand(wxT("wxsubscripts: true"));
      break;
    case menu_alwaysAutosubscript:
      MenuCommand(wxT("wxsubscripts: 'always$"));
      break;
    case menu_roundedMatrixParensYes:
      MenuCommand(wxT("lmxchar:\"(\"$rmxchar:\")\"$"));
      break;
    case menu_roundedMatrixParensNo:
      MenuCommand(wxT("lmxchar:\"[\"$rmxchar:\"]\"$"));
      break;
    case menu_fullscreen:
      ShowFullScreen(!IsFullScreen());
      break;
    case ToolBar::tb_hideCode:
      m_console->m_configuration->ShowCodeCells(!m_console->m_configuration->ShowCodeCells());
      m_console->CodeCellVisibilityChanged();
      break;
    case menu_remove_output:
      m_console->RemoveAllOutput();
      break;
    case menu_show_toolbar:
#if defined __WXMAC__ || defined __WXMSW__
      ShowToolBar((GetToolBar() == NULL) || !(GetToolBar()->IsShown()));
#else
      ShowToolBar(!(GetToolBar() != NULL));
#endif
      break;
    case menu_edit_find:
    case ToolBar::tb_find:
      if (m_console->m_findDialog == NULL)
        m_console->m_findDialog = new FindReplaceDialog(
                this,
                &m_findData,
                _("Find and Replace"));

      if (m_console->GetActiveCell() != NULL)
      {
        // Start incremental search and highlighting of search results again.
        if (m_console->m_findDialog != NULL)
          m_oldFindString = wxEmptyString;

        wxString selected = m_console->GetActiveCell()->GetSelectionString();
        if (selected.Length() > 0)
        {
          if (m_console->m_findDialog != NULL)
            m_console->m_findDialog->SetFindString(selected);
        }
      }

      m_console->m_findDialog->Show(true);
      m_console->m_findDialog->SetFocus();
      m_console->m_findDialog->Raise();
      break;
    case menu_history_next:
    {
      wxString command = m_history->GetCommand(true);
      if (command != wxEmptyString)
        m_console->SetActiveCellText(command);
    }
      break;
    case menu_history_previous:
    {
      wxString command = m_history->GetCommand(false);
      if (command != wxEmptyString)
        m_console->SetActiveCellText(command);
    }
      break;
  }
  m_console->RequestRedraw();
}

void wxMaxima::OnFind(wxFindDialogEvent &event)
{
  if (!m_console->FindNext(event.GetFindString(),
                           event.GetFlags() & wxFR_DOWN,
                           !(event.GetFlags() & wxFR_MATCHCASE)))
    wxMessageBox(_("No matches found!"));
}

void wxMaxima::OnFindClose(wxFindDialogEvent &WXUNUSED(event))
{
  if (m_console->m_findDialog != NULL)
    m_console->m_findDialog->Destroy();
  m_oldFindString = wxEmptyString;
  m_console->m_findDialog = NULL;
}

void wxMaxima::OnReplace(wxFindDialogEvent &event)
{
  m_console->Replace(event.GetFindString(),
                     event.GetReplaceString(),
                     !(event.GetFlags() & wxFR_MATCHCASE)
  );

  if (!m_console->FindNext(event.GetFindString(),
                           event.GetFlags() & wxFR_DOWN,
                           !(event.GetFlags() & wxFR_MATCHCASE)
  )
          )
    wxMessageBox(_("No matches found!"));
  else
    m_console->UpdateTableOfContents();
}

void wxMaxima::OnReplaceAll(wxFindDialogEvent &event)
{
  int count = m_console->ReplaceAll(
          event.GetFindString(),
          event.GetReplaceString(),
          !(event.GetFlags() & wxFR_MATCHCASE)
  );

  wxMessageBox(wxString::Format(_("Replaced %d occurrences."), count));
  if (count > 0)
    m_console->UpdateTableOfContents();
}

void wxMaxima::MaximaMenu(wxCommandEvent &event)
{
  if(m_console != NULL)
    m_console->CloseAutoCompletePopup();

  wxString expr = GetDefaultEntry();
  wxString cmd;
  wxString b = wxT("\\");
  wxString f = wxT("/");
  switch (event.GetId())
  {
    case menu_jumptoerror:
      if(m_console->m_cellPointers.m_errorList.FirstError())
      {
        m_console->SetActiveCell(dynamic_cast<GroupCell *>(m_console->m_cellPointers.m_errorList.FirstError())->GetEditable());
        dynamic_cast<GroupCell *>(m_console->m_cellPointers.m_errorList.FirstError())->GetEditable()->CaretToEnd();
      }
      break;
    case ToolBar::menu_restart_id:
      m_closing = true;
      m_console->m_cellPointers.SetWorkingGroup(NULL);
      m_console->m_evaluationQueue.Clear();
      m_console->ResetInputPrompts();
      m_unsuccessfulConnectionAttempts = 0;
      StartMaxima(true);
      break;
    case menu_soft_restart:
      MenuCommand(wxT("kill(all);"));
      break;
    case menu_functions:
      MenuCommand(wxT("functions;"));
      break;
    case menu_variables:
      MenuCommand(wxT("values;"));
      break;
    case menu_display:
    {
      wxString choices[] =
              {
                      wxT("xml"), wxT("ascii"), wxT("none")
              };
      wxString choice = wxGetSingleChoice(
              _("Select math display algorithm"),
              _("Display algorithm"),
              3,
              choices,
              this
      );
      if (choice.Length())
      {
        cmd = wxT("set_display('") + choice + wxT(")$");
        MenuCommand(cmd);
      }
    }
      break;
    case menu_texform:
      cmd = wxT("tex(") + expr + wxT(")$");
      MenuCommand(cmd);
      break;
    case menu_time:
      cmd = wxT("if showtime#false then showtime:false else showtime:all$");
      MenuCommand(cmd);
      break;
    case menu_fun_def:
      cmd = GetTextFromUser(_("Show the definition of function:"),
                            _("Function"), m_console->m_configuration, wxEmptyString, this);
      if (cmd.Length())
      {
        cmd = wxT("fundef(") + cmd + wxT(");");
        MenuCommand(cmd);
      }
      break;
    case menu_add_path:
    {
      if (m_lastPath.Length() == 0)
        m_lastPath = wxGetHomeDir();
      wxString dir = wxDirSelector(_("Add dir to path:"), m_lastPath);
      if (dir.Length())
      {
        m_lastPath = dir;
#if defined (__WXMSW__)
        dir.Replace(wxT("\\"), wxT("/"));
#endif
        cmd = wxT("file_search_maxima : cons(sconcat(\"") + dir +
              wxT("/###.{lisp,mac,mc}\"), file_search_maxima)$");
        MenuCommand(cmd);
      }
    }
      break;
    case menu_evaluate_all_visible:
    {
      m_console->m_evaluationQueue.Clear();
      m_console->ResetInputPrompts();
      EvaluationQueueLength(0);
      if (m_console->m_configuration->RestartOnReEvaluation())
        StartMaxima();
      m_console->AddDocumentToEvaluationQueue();
      // Inform the user about the length of the evaluation queue.
      EvaluationQueueLength(m_console->m_evaluationQueue.Size(), m_console->m_evaluationQueue.CommandsLeftInCell());
      TryEvaluateNextInQueue();
    }
      break;
    case menu_evaluate_all:
    {
      m_console->m_evaluationQueue.Clear();
      m_console->ResetInputPrompts();
      EvaluationQueueLength(0);
      if (m_console->m_configuration->RestartOnReEvaluation())
        StartMaxima();
      m_console->AddEntireDocumentToEvaluationQueue();
      // Inform the user about the length of the evaluation queue.
      EvaluationQueueLength(m_console->m_evaluationQueue.Size(), m_console->m_evaluationQueue.CommandsLeftInCell());
      TryEvaluateNextInQueue();
    }
      break;
    case ToolBar::tb_evaltillhere:
    {
      m_console->m_evaluationQueue.Clear();
      m_console->ResetInputPrompts();
      EvaluationQueueLength(0);
      if (m_console->m_configuration->RestartOnReEvaluation())
        StartMaxima();
      m_console->AddDocumentTillHereToEvaluationQueue();
      // Inform the user about the length of the evaluation queue.
      EvaluationQueueLength(m_console->m_evaluationQueue.Size(), m_console->m_evaluationQueue.CommandsLeftInCell());
      TryEvaluateNextInQueue();
    }
      break;
    case menu_clear_var:
      cmd = GetTextFromUser(_("Delete variable(s):"), _("Delete"),
                            m_console->m_configuration,
                            wxT("all"), this);
      if (cmd.Length())
      {
        cmd = wxT("remvalue(") + cmd + wxT(");");
        MenuCommand(cmd);
      }
      break;
    case menu_clear_fun:
      cmd = GetTextFromUser(_("Delete function(s):"), _("Delete"),
                            m_console->m_configuration,
                            wxT("all"), this);
      if (cmd.Length())
      {
        cmd = wxT("remfunction(") + cmd + wxT(");");
        MenuCommand(cmd);
      }
      break;
    case menu_subst:
    case button_subst:
    {
      SubstituteWiz *wiz = new SubstituteWiz(this, -1, m_console->m_configuration, _("Substitute"));
      wiz->SetValue(expr);
      //wiz->Centre(wxBOTH);
      if (wiz->ShowModal() == wxID_OK)
      {
        wxString val = wiz->GetValue();
        MenuCommand(val);
      }
      wiz->Destroy();
    }
      break;
    default:
      break;
  }
}

void wxMaxima::EquationsMenu(wxCommandEvent &event)
{
  if(m_console != NULL)
    m_console->CloseAutoCompletePopup();

  wxString expr = GetDefaultEntry();
  wxString cmd;
  switch (event.GetId())
  {
    case menu_allroots:
      cmd = wxT("allroots(") + expr + wxT(");");
      MenuCommand(cmd);
      break;
    case menu_bfallroots:
      cmd = wxT("bfallroots(") + expr + wxT(");");
      MenuCommand(cmd);
      break;
    case menu_realroots:
      cmd = wxT("realroots(") + expr + wxT(");");
      MenuCommand(cmd);
      break;
    case button_solve:
    case menu_solve:
    {
      Gen2Wiz *wiz = new Gen2Wiz(_("Equation(s):"), _("Variable(s):"),
                                 expr, wxT("x"),
                                 m_console->m_configuration,
                                 this, -1, _("Solve"), true,
                                 _("solve() will solve a list of equations only if for n "
                                   "independent equations there are n variables to solve to.\n"
                                   "If only one result variable is of interest the other result "
                                   "variables solve needs to do its work can be used to tell "
                                   "solve() which variables to eliminate in the solution "
                                   "for the interesting variable.")
        );
      //wiz->Centre(wxBOTH);
      wiz->SetLabel1ToolTip(_("Comma-separated equations"));
      wiz->SetLabel2ToolTip(_("Comma-separated variables"));
      if (wiz->ShowModal() == wxID_OK)
      {
        cmd = wxT("solve([") + wiz->GetValue1() + wxT("], [") +
              wiz->GetValue2() + wxT("]);");
        MenuCommand(cmd);
      }
      wiz->Destroy();
    }
      break;
    case menu_solve_to_poly:
    {
      Gen2Wiz *wiz = new Gen2Wiz(_("Equation(s):"), _("Variable(s):"),
                                 expr, wxT("x"),
                                 m_console->m_configuration,
                                 this, -1, _("Solve"), true);
      //wiz->Centre(wxBOTH);
      if (wiz->ShowModal() == wxID_OK)
      {
        cmd = wxT("to_poly_solve([") + wiz->GetValue1() + wxT("], [") +
              wiz->GetValue2() + wxT("]);");
        MenuCommand(cmd);
      }
      wiz->Destroy();
    }
      break;
    case menu_solve_num:
    {
      if (expr.StartsWith(wxT("%")))
        expr = wxT("''(") + expr + wxT(")");
      Gen4Wiz *wiz = new Gen4Wiz(_("Equation:"), _("Variable:"),
                                 _("Lower bound:"), _("Upper bound:"),
                                 expr, wxT("x"), wxT("-1"), wxT("1"),
                                 m_console->m_configuration,
                                 this, -1, _("Find root"), true);
      //wiz->Centre(wxBOTH);
      if (wiz->ShowModal() == wxID_OK)
      {
        cmd = wxT("find_root(") + wiz->GetValue1() + wxT(", ") +
              wiz->GetValue2() + wxT(", ") +
              wiz->GetValue3() + wxT(", ") +
              wiz->GetValue4() + wxT(");");
        MenuCommand(cmd);
      }
      wiz->Destroy();
    }
      break;
    case button_solve_ode:
    case menu_solve_ode:
    {
      Gen3Wiz *wiz = new Gen3Wiz(_("Equation:"), _("Function:"), _("Variable:"),
                                 expr, wxT("y"), wxT("x"),
                                 m_console->m_configuration,
                                 this, -1, _("Solve ODE"));
      wiz->SetValue(expr);
      //wiz->Centre(wxBOTH);
      if (wiz->ShowModal() == wxID_OK)
      {
        wxString val = wxT("ode2(") + wiz->GetValue1() + wxT(", ") +
                       wiz->GetValue2() + wxT(", ") + wiz->GetValue3() + wxT(");");
        MenuCommand(val);
      }
      wiz->Destroy();
    }
      break;
    case menu_ivp_1:
    {
      Gen3Wiz *wiz = new Gen3Wiz(_("Solution:"), _("Point:"), _("Value:"),
                                 expr, wxT("x="), wxT("y="),
                                 m_console->m_configuration,
                                 this, -1, _("IC1"), true);
      //wiz->Centre(wxBOTH);
      if (wiz->ShowModal() == wxID_OK)
      {
        wxString val = wxT("ic1(") + wiz->GetValue1() + wxT(", ") +
                       wiz->GetValue2() + wxT(", ") + wiz->GetValue3() + wxT(");");
        MenuCommand(val);
      }
      wiz->Destroy();
    }
      break;
    case menu_ivp_2:
    {
      Gen4Wiz *wiz = new Gen4Wiz(_("Solution:"), _("Point:"),
                                 _("Value:"), _("Derivative:"),
                                 expr, wxT("x="), wxT("y="), wxT("'diff(y,x)="),
                                 m_console->m_configuration,
                                 this, -1, _("IC2"), true);
      //wiz->Centre(wxBOTH);
      if (wiz->ShowModal() == wxID_OK)
      {
        wxString val = wxT("ic2(") + wiz->GetValue1() + wxT(", ") +
                       wiz->GetValue2() + wxT(", ") + wiz->GetValue3() +
                       wxT(", ") + wiz->GetValue4() + wxT(");");
        MenuCommand(val);
      }
      wiz->Destroy();
    }
      break;
    case menu_bvp:
    {
      BC2Wiz *wiz = new BC2Wiz(this, -1, m_console->m_configuration, _("BC2"));
      wiz->SetValue(expr);
      //wiz->Centre(wxBOTH);
      if (wiz->ShowModal() == wxID_OK)
      {
        wxString val = wiz->GetValue();
        MenuCommand(val);
      }
      wiz->Destroy();
    }
      break;
    case menu_eliminate:
    {
      Gen2Wiz *wiz = new Gen2Wiz(_("Equations:"),
                                 _("Variables:"),
                                 expr, wxEmptyString,
                                 m_console->m_configuration,
                                 this, -1, _("Eliminate"), true);
      //wiz->Centre(wxBOTH);
      if (wiz->ShowModal() == wxID_OK)
      {
        cmd = wxT("eliminate([") + wiz->GetValue1() + wxT("],[")
              + wiz->GetValue2() + wxT("]);");
        MenuCommand(cmd);
      }
      wiz->Destroy();
    }
      break;
    case menu_solve_algsys:
    {
      wxString sz = GetTextFromUser(_("Number of equations:"),
                                    _("Solve algebraic system"),
                                    m_console->m_configuration,
                                    wxT("3"), this);
      if (sz.Length() == 0)
        return;
      long isz;
      if (!sz.ToLong(&isz) || isz <= 0)
      {
        wxMessageBox(_("Not a valid number of equations!"), _("Error!"),
                     wxOK | wxICON_ERROR);
        return;
      }
      SysWiz *wiz = new SysWiz(this, -1, m_console->m_configuration, _("Solve algebraic system"), isz);
      //wiz->Centre(wxBOTH);
      if (wiz->ShowModal() == wxID_OK)
      {
        cmd = wxT("algsys") + wiz->GetValue();
        MenuCommand(cmd);
      }
      wiz->Destroy();
    }
      break;
    case menu_solve_lin:
    {
      wxString sz = GetTextFromUser(_("Number of equations:"),
                                    _("Solve linear system"),
                                    m_console->m_configuration,
                                    wxT("3"), this);
      if (sz.Length() == 0)
        return;
      long isz;
      if (!sz.ToLong(&isz) || isz <= 0)
      {
        wxMessageBox(_("Not a valid number of equations!"), _("Error!"),
                     wxOK | wxICON_ERROR);
        return;
      }
      SysWiz *wiz = new SysWiz(this, -1, m_console->m_configuration, _("Solve linear system"), isz);
      //wiz->Centre(wxBOTH);
      if (wiz->ShowModal() == wxID_OK)
      {
        cmd = wxT("linsolve") + wiz->GetValue();
        MenuCommand(cmd);
      }
      wiz->Destroy();
    }
      break;
    case menu_solve_de:
    {
      Gen2Wiz *wiz = new Gen2Wiz(_("Equation(s):"), _("Function(s):"),
                                 expr, wxT("y(x)"),
                                 m_console->m_configuration,
                                 this, -1, _("Solve ODE"));
      //wiz->Centre(wxBOTH);
      if (wiz->ShowModal() == wxID_OK)
      {
        cmd = wxT("desolve([") + wiz->GetValue1() + wxT("],[")
              + wiz->GetValue2() + wxT("]);");
        MenuCommand(cmd);
      }
      wiz->Destroy();
    }
      break;
    case menu_atvalue:
    {
      Gen3Wiz *wiz = new Gen3Wiz(_("Expression:"), _("Point:"),
                                 _("Value:"), expr, wxT("x=0"), wxT("0"),
                                 m_console->m_configuration,
                                 this, -1, _("At value"));
      wiz->SetValue(expr);
      //wiz->Centre(wxBOTH);
      if (wiz->ShowModal() == wxID_OK)
      {
        wxString val = wxT("atvalue(") + wiz->GetValue1() + wxT(", ")
                       + wiz->GetValue2() +
                       wxT(", ") + wiz->GetValue3() + wxT(");");
        MenuCommand(val);
      }
      wiz->Destroy();
    }
      break;
    case menu_lhs:
      cmd = wxT("lhs(") + expr + wxT(");");
      MenuCommand(cmd);
      break;
    case menu_rhs:
      cmd = wxT("rhs(") + expr + wxT(");");
      MenuCommand(cmd);
      break;
    default:
      break;
  }
}

void wxMaxima::AlgebraMenu(wxCommandEvent &event)
{
  if(m_console != NULL)
    m_console->CloseAutoCompletePopup();

  wxString expr = GetDefaultEntry();
  wxString cmd;
  switch (event.GetId())
  {
    case menu_invert_mat:
      cmd = wxT("invert(") + expr + wxT(");");
      MenuCommand(cmd);
      break;
    case menu_determinant:
      cmd = wxT("determinant(") + expr + wxT(");");
      MenuCommand(cmd);
      break;
    case menu_eigen:
      cmd = wxT("eigenvalues(") + expr + wxT(");");
      MenuCommand(cmd);
      break;
    case menu_eigvect:
      cmd = wxT("eigenvectors(") + expr + wxT(");");
      MenuCommand(cmd);
      break;
    case menu_adjoint_mat:
      cmd = wxT("adjoint(") + expr + wxT(");");
      MenuCommand(cmd);
      break;
    case menu_transpose:
      cmd = wxT("transpose(") + expr + wxT(");");
      MenuCommand(cmd);
      break;
    case menu_map_mat:
    {
      Gen2Wiz *wiz = new Gen2Wiz(_("Function:"), _("Matrix:"),
                                 wxEmptyString, expr,
                                 m_console->m_configuration,
                                 this, -1, _("Matrix map"));
      //wiz->Centre(wxBOTH);
      if (wiz->ShowModal() == wxID_OK)
      {
        cmd = wxT("matrixmap(") + wiz->GetValue1() + wxT(", ")
              + wiz->GetValue2() + wxT(");");
        MenuCommand(cmd);
      }
      wiz->Destroy();
    }
      break;
    case menu_enter_mat:
    case menu_stats_enterm:
    {
      MatDim *wiz = new MatDim(this, -1,
                               m_console->m_configuration,
                               _("Matrix"));
      //wiz->Centre(wxBOTH);
      if (wiz->ShowModal() == wxID_OK)
      {
        if (wiz->GetValue0() != wxEmptyString)
          cmd = wiz->GetValue0() + wxT(": ");
        long w, h;
        int type = wiz->GetMatrixType();
        if (!(wiz->GetValue2()).ToLong(&w) ||
            !(wiz->GetValue1()).ToLong(&h) ||
            w <= 0 || h <= 0)
        {
          wxMessageBox(_("Not a valid matrix dimension!"), _("Error!"),
                       wxOK | wxICON_ERROR);
          return;
        }
        if (w != h)
          type = MatWiz::MATRIX_GENERAL;
        MatWiz *mwiz = new MatWiz(this, -1, m_console->m_configuration, _("Enter matrix"),
                                  type, w, h);
        //wiz->Centre(wxBOTH);
        if (mwiz->ShowModal() == wxID_OK)
        {
          cmd += mwiz->GetValue();
          MenuCommand(cmd);
        }
        mwiz->Destroy();
      }
      wiz->Destroy();
    }
      break;
    case menu_cpoly:
    {
      Gen2Wiz *wiz = new Gen2Wiz(_("Matrix:"), _("Variable:"),
                                 expr, wxT("x"),
                                 m_console->m_configuration,
                                 this, -1, _("Char poly"));
      //wiz->Centre(wxBOTH);
      if (wiz->ShowModal() == wxID_OK)
      {
        cmd = wxT("charpoly(") + wiz->GetValue1() + wxT(", ")
              + wiz->GetValue2() + wxT("), expand;");
        MenuCommand(cmd);
      }
      wiz->Destroy();
    }
      break;
    case menu_gen_mat:
    {
      Gen4Wiz *wiz = new Gen4Wiz(_("Array:"), _("Rows:"), _("Columns:"), _("Name:"),
                                 expr, wxT("3"), wxT("3"), wxEmptyString,
                                 m_console->m_configuration,
                                 this, -1, _("Generate Matrix"));
      wiz->SetValue(expr);
      //wiz->Centre(wxBOTH);
      if (wiz->ShowModal() == wxID_OK)
      {
        wxString val = wxT("genmatrix(") + wiz->GetValue1() +
                       wxT(", ") + wiz->GetValue2() +
                       wxT(", ") + wiz->GetValue3() + wxT(");");
        if (wiz->GetValue4() != wxEmptyString)
          val = wiz->GetValue4() + wxT(": ") + val;
        MenuCommand(val);
      }
      wiz->Destroy();
    }
      break;
    case menu_gen_mat_lambda:
    {
      Gen4Wiz *wiz = new Gen4Wiz(_("matrix[i,j]:"), _("Rows:"), _("Columns:"), _("Name:"),
                                 expr, wxT("3"), wxT("3"), wxEmptyString,
                                 m_console->m_configuration,
                                 this, -1, _("Generate Matrix"));
      wiz->SetValue(expr);
      //wiz->Centre(wxBOTH);
      if (wiz->ShowModal() == wxID_OK)
      {
        wxString val = wxT("genmatrix(lambda([i,j], ") + wiz->GetValue1() +
                       wxT("), ") + wiz->GetValue2() +
                       wxT(", ") + wiz->GetValue3() + wxT(");");
        if (wiz->GetValue4() != wxEmptyString)
          val = wiz->GetValue4() + wxT(": ") + val;
        MenuCommand(val);
      }
      wiz->Destroy();
    }
      break;
    case button_map:
    case menu_map:
    {
      Gen2Wiz *wiz = new Gen2Wiz(_("Function:"), _("List(s):"),
                                 wxEmptyString, expr,
                                 m_console->m_configuration,
                                 this, -1, _("Map"));
      //wiz->Centre(wxBOTH);
      if (wiz->ShowModal() == wxID_OK)
      {
        cmd = wxT("map(") + wiz->GetValue1() + wxT(", ") + wiz->GetValue2() +
              wxT(");");
        MenuCommand(cmd);
      }
      wiz->Destroy();
    }
      break;
    case menu_make_list:
    {
      Gen4Wiz *wiz = new Gen4Wiz(_("Expression:"), _("Variable:"),
                                 _("From:"), _("To:"),
                                 expr, wxT("k"), wxT("1"), wxT("10"),
                                 m_console->m_configuration,
                                 this, -1, _("Make list"));
      //wiz->Centre(wxBOTH);
      if (wiz->ShowModal() == wxID_OK)
      {
        cmd = wxT("makelist(") + wiz->GetValue1() + wxT(", ") +
              wiz->GetValue2() + wxT(", ") +
              wiz->GetValue3() + wxT(", ") +
              wiz->GetValue4() + wxT(");");
        MenuCommand(cmd);
      }
      wiz->Destroy();
    }
      break;
    case menu_apply:
    {
      Gen2Wiz *wiz = new Gen2Wiz(_("Function:"), _("List:"),
                                 wxT("\"+\""), expr,
                                 m_console->m_configuration,
                                 this, -1, _("Apply"), true);
      //wiz->Centre(wxBOTH);
      if (wiz->ShowModal() == wxID_OK)
      {
        cmd = wxT("apply(") + wiz->GetValue1() + wxT(", ")
              + wiz->GetValue2() + wxT(");");
        MenuCommand(cmd);
      }
      wiz->Destroy();
    }
      break;
    default:
      break;
  }
}

void wxMaxima::AddDrawParameter(wxString cmd, int dimensionsOfNewDrawCommand)
{
  if(!m_drawPane)
    return;

  int dimensions = 0;
  dimensions = m_drawPane->GetDimensions();

  if(dimensions < 2)
  {
    if(dimensionsOfNewDrawCommand < 3)
      cmd = wxT("wxdraw2d(\n    ") + cmd + wxT("\n)$");
    else
      cmd = wxT("wxdraw3d(\n    ") + cmd + wxT("\n)$");
    m_console->OpenHCaret(cmd);
    m_console->GetActiveCell()->SetCaretPosition(
      m_console->GetActiveCell()->GetCaretPosition() - 3);
  }
  else
  {
    if(m_console->GetActiveCell())
    {
      m_console->GetActiveCell()->AddDrawParameter(cmd);
      m_console->Recalculate();
      m_console->RequestRedraw();
    }
  }
  m_console->SetFocus();
}

void wxMaxima::DrawMenu(wxCommandEvent &event)
{
  if(!m_drawPane)
    return;

  UpdateDrawPane();
  int dimensions = m_drawPane->GetDimensions();

  if(m_console != NULL)
    m_console->CloseAutoCompletePopup();

  wxString expr;
  if(dimensions < 2)
    expr = GetDefaultEntry();
  else
    expr = "%";

  wxString cmd;
  switch (event.GetId())
  {
  case menu_draw_2d:
  {
    DrawWiz *wiz = new DrawWiz(this, m_console->m_configuration, 2);
    //wiz->Centre(wxBOTH);
    if (wiz->ShowModal() == wxID_OK)
    {
      m_console->SetFocus();

      m_console->OpenHCaret(wiz->GetValue());
      m_console->GetActiveCell()->SetCaretPosition(
        m_console->GetActiveCell()->GetCaretPosition() - 3);
    }
    wiz->Destroy();
    break;
  }
  case menu_draw_3d:
    if(dimensions < 2)
    {
      DrawWiz *wiz = new DrawWiz(this, m_console->m_configuration, 3);
      //wiz->Centre(wxBOTH);
      if (wiz->ShowModal() == wxID_OK)
      {
        m_console->SetFocus();

        m_console->OpenHCaret(wiz->GetValue());
        m_console->GetActiveCell()->SetCaretPosition(
        m_console->GetActiveCell()->GetCaretPosition() - 3);
      }
      wiz->Destroy();
      break;
    }
    else
    {
      Wiz3D *wiz = new Wiz3D(this, m_console->m_configuration);
      //wiz->Centre(wxBOTH);
      if (wiz->ShowModal() == wxID_OK)
        AddDrawParameter(wiz->GetValue());
      wiz->Destroy();
      break;
    }
  case menu_draw_fgcolor:
  {
    wxColour col = wxGetColourFromUser(this);
    if (col.IsOk())
      AddDrawParameter(
        wxString::Format("color=\"#%02x%02x%02x\"",
                         col.Red(),col.Green(),col.Blue()));
    break;
  }
  case menu_draw_fillcolor:
  {
    wxColour col = wxGetColourFromUser(this);
    if (col.IsOk())
      AddDrawParameter(
        wxString::Format("fill_color=\"#%02x%02x%02x\"",
                         col.Red(),col.Green(),col.Blue()));
  break;
  }
  case menu_draw_title:
  {
    Gen1Wiz *wiz = new Gen1Wiz(this, -1, m_console->m_configuration,
                               _("Set the diagram title"),
                               _("Title (Sub- and superscripts as x_{10} or x^{10})"),expr);
    //wiz->Centre(wxBOTH);
    if (wiz->ShowModal() == wxID_OK)
    {
      cmd = wxT("title=\"") + wiz->GetValue() + wxT("\"");
      AddDrawParameter(cmd);
    }
    wiz->Destroy();
    break;
  }
  case menu_draw_key:
  {
    Gen1Wiz *wiz = new Gen1Wiz(this, -1, m_console->m_configuration,
                               _("Set the next plot's title. Empty = no title."),
                               _("Title (Sub- and superscripts as x_{10} or x^{10})"),expr);
    //wiz->Centre(wxBOTH);
    if (wiz->ShowModal() == wxID_OK)
    {
      cmd = wxT("key=\"") + wiz->GetValue() + wxT("\"");
      AddDrawParameter(cmd);
    }
    wiz->Destroy();
    break;
  }
  case menu_draw_explicit:
  {
    ExplicitWiz *wiz = new ExplicitWiz(this, m_console->m_configuration, expr, dimensions);
    //wiz->Centre(wxBOTH);
    if (wiz->ShowModal() == wxID_OK)
      AddDrawParameter(wiz->GetValue());
    wiz->Destroy();
    break;
  }

  case menu_draw_implicit:
  {
    ImplicitWiz *wiz = new ImplicitWiz(this, m_console->m_configuration, expr, dimensions);
    //wiz->Centre(wxBOTH);
    if (wiz->ShowModal() == wxID_OK)
      AddDrawParameter(wiz->GetValue());
    wiz->Destroy();
    break;
  }

  case menu_draw_parametric:
  {
    ParametricWiz *wiz = new ParametricWiz(this, m_console->m_configuration, dimensions);
    //wiz->Centre(wxBOTH);
    if (wiz->ShowModal() == wxID_OK)
      AddDrawParameter(wiz->GetValue());
    wiz->Destroy();
    break;
  }

  case menu_draw_points:
  {
    WizPoints *wiz = new WizPoints(this, m_console->m_configuration, dimensions, expr);
    //wiz->Centre(wxBOTH);
    if (wiz->ShowModal() == wxID_OK)
      AddDrawParameter(wiz->GetValue());
    wiz->Destroy();
    break;
  }

  case menu_draw_grid:
  {
    Gen2Wiz *wiz = new Gen2Wiz(
                               _("x direction [in multiples of the tick frequency]"),
                               _("y direction [in multiples of the tick frequency]"),
                               "1","1",
                               m_console->m_configuration, this, -1,
                               _("Set the grid density.")
      );
    //wiz->Centre(wxBOTH);
    if (wiz->ShowModal() == wxID_OK)
    {
      cmd = wxT("grid=[") + wiz->GetValue1() + "," + wiz->GetValue2() + wxT("]");
      AddDrawParameter(cmd);
    }
    wiz->Destroy();
    break;
  }

  case menu_draw_axis:
  {
    AxisWiz *wiz = new AxisWiz(this, m_console->m_configuration, dimensions);
    //wiz->Centre(wxBOTH);
    if (wiz->ShowModal() == wxID_OK)
    {
      AddDrawParameter(wiz->GetValue());
    }
    wiz->Destroy();
    break;
  }

  case menu_draw_contour:
  {
    WizContour *wiz = new WizContour(this, m_console->m_configuration);
    //wiz->Centre(wxBOTH);
    if (wiz->ShowModal() == wxID_OK)
      AddDrawParameter(wiz->GetValue(), 3);
    wiz->Destroy();
    break;
  }

  case menu_draw_accuracy:
  {
    WizDrawAccuracy *wiz = new WizDrawAccuracy(this, m_console->m_configuration, dimensions);
    //wiz->Centre(wxBOTH);
    if (wiz->ShowModal() == wxID_OK)
      AddDrawParameter(wiz->GetValue(), dimensions);
    wiz->Destroy();
    break;
  }

  }
}

void wxMaxima::ListMenu(wxCommandEvent &event)
{
  if(m_console != NULL)
    m_console->CloseAutoCompletePopup();

  wxString expr = GetDefaultEntry();
  wxString cmd;
  switch (event.GetId())
  {
  case menu_list_create_from_args:
  {
    wxString arg;
    Gen1Wiz *wiz = new Gen1Wiz(this, -1, m_console->m_configuration,
                               _("Extract function arguments"),
                               _("The function call whose arguments to extract"),
                               expr);
    wiz->SetLabel1ToolTip(_("Something like f(x_1,x_2)"));
    //wiz->Centre(wxBOTH);
    if (wiz->ShowModal() == wxID_OK)
    {
      cmd = wxT("args(") + wiz->GetValue() + wxT(")");
      MenuCommand(cmd);
    }
    wiz->Destroy();
  }
    break;
  case menu_list_list2matrix:
    MenuCommand(wxT("apply('matrix,") + expr + wxT(")"));
    break;
  case menu_list_matrix2list:
    MenuCommand(wxT("args(") + expr + wxT(")"));
    break;
  case menu_list_create_from_elements:
  {
    Gen1Wiz *wiz = new Gen1Wiz(this, -1, m_console->m_configuration,
                               _("Create list from comma-separated elements"),
                               _("Comma-separated elements"),expr);
    //wiz->Centre(wxBOTH);
    if (wiz->ShowModal() == wxID_OK)
    {
      cmd = wxT("[") + wiz->GetValue() + wxT("]");
      MenuCommand(cmd);
    }
    wiz->Destroy();
  }
  break;
  case menu_list_create_from_rule:
  {
    Gen5Wiz *wiz = new Gen5Wiz(_("Rule:"), _("Index variable:"),
                               _("Index Start:"), _("Index End:"), _("Index Step:"),
                               expr, wxT("i"), wxT("1"), wxT("100"), wxT("1"),
                               m_console->m_configuration,
                               this, -1, _("Create a list from a rule"), true);
    wiz->SetLabel1ToolTip(_("The rule that explains how to generate the value of an list item.\n"
                            "Might be something like \"i\", \"i^2\" or \"sin(i)\""));
    wiz->SetLabel2ToolTip(_("The number of the item which is stepped from \"Index Start\" to \"Index End\"."));
    wiz->SetValue(expr);
    //wiz->Centre(wxBOTH);
    if (wiz->ShowModal() == wxID_OK)
    {
      wxString val = wxT("makelist(") + wiz->GetValue1() + wxT(", ") +
        wiz->GetValue2() + wxT(", ") + wiz->GetValue3() + wxT(", ") +
        wiz->GetValue4();
      wxString tst = wiz->GetValue5();
      tst.Trim(true);
      tst.Trim(false);
      if(tst != wxT("1"))
      val += wxT(",") + wiz->GetValue5();
      val += wxT(")");
      MenuCommand(val);
    }
    wiz->Destroy();
  }
    break;
  case menu_list_create_from_list:
  {
    Gen3Wiz *wiz = new Gen3Wiz(_("Rule:"), _("Iterator:"),
                               _("Source list:"),
                               expr, wxT("i"), wxT("list"),
                               m_console->m_configuration,
                               this, -1, _("Create a list from another list"), true);
    wiz->SetLabel1ToolTip(_("The rule that explains how to generate the value of an list item.\n"
                            "Might be something like \"i\", \"i^2\" or \"sin(i)\""));
    wiz->SetLabel2ToolTip(_("The variable the value of the current source item is stored in."));
    wiz->SetValue(expr);
    //wiz->Centre(wxBOTH);    if (wiz->ShowModal() == wxID_OK)
    {
      wxString val = wxT("makelist(") + wiz->GetValue1() + wxT(", ") +
        wiz->GetValue2() + wxT(", ") + wiz->GetValue3() + wxT(")");
      MenuCommand(val);
    }
    wiz->Destroy();
  }
    break;
  case menu_list_actual_values_storage:
  {
    ActualValuesStorageWiz *wiz = new ActualValuesStorageWiz(m_console->m_configuration,
                               this, -1, _("Create a list as a storage for the values of variables"));
    //wiz->Centre(wxBOTH);
    if (wiz->ShowModal() == wxID_OK)
    {
      MenuCommand(wiz->GetValue());
    }
    wiz->Destroy();
  }
    break;
  case menu_list_sort:
  {
    ListSortWiz *wiz = new ListSortWiz(m_console->m_configuration,
                                       this, -1, _("Sort a list"), expr);
    //wiz->Centre(wxBOTH);
    if (wiz->ShowModal() == wxID_OK)
    {
      MenuCommand(wiz->GetValue());
    }
    wiz->Destroy();
  }
    break;
  case menu_list_length:
    MenuCommand(wxT("length(") + expr + wxT(")"));
    break;
  case menu_list_push:
    {
      Gen2Wiz *wiz = new Gen2Wiz(_("List:"), _("Element:"),
                                 expr, wxT("1"),
                                 m_console->m_configuration,
                                 this, -1, _("LCM"), true);
      //wiz->Centre(wxBOTH);
      if (wiz->ShowModal() == wxID_OK)
      {
        cmd = wxT("push(") + wiz->GetValue1() + wxT(", ")
              + wiz->GetValue2() + wxT(");");
        MenuCommand(cmd);
      }
      wiz->Destroy();
    }
    break;
  case menu_list_pop:
    MenuCommand(wxT("pop(") + expr + wxT(")"));
    break;
  case menu_list_reverse:
    MenuCommand(wxT("reverse(") + expr + wxT(")"));
    break;
  case menu_list_first:
    MenuCommand(wxT("first(") + expr + wxT(")"));
    break;
  case menu_list_last:
    MenuCommand(wxT("last(") + expr + wxT(")"));
    break;
  case menu_list_rest:
    {
      Gen2Wiz *wiz = new Gen2Wiz(_("List"), _("n"),
                                 expr, wxEmptyString,
                                 m_console->m_configuration,
                                 this, -1, _("Return the list without its last n elements"),
                                 true);
      //wiz->Centre(wxBOTH);
      if (wiz->ShowModal() == wxID_OK)
      {
        cmd = wxT("rest(") + wiz->GetValue1();
        wxString num = wiz->GetValue2();
        num.Trim(true);
        num.Trim(false);
        if(num != wxT("1"))
        {
          cmd += wxT(",") + wiz->GetValue2();
        }
        cmd += wxT(")");
        MenuCommand(cmd);
      }
      wiz->Destroy();
    }
    break;
  case menu_list_restN:
    {
      Gen2Wiz *wiz = new Gen2Wiz(_("List"), _("n"),
                                 expr, wxEmptyString,
                                 m_console->m_configuration,
                                 this, -1, _("Return the list without its first n elements"),
                                 true);
      //wiz->Centre(wxBOTH);
      if (wiz->ShowModal() == wxID_OK)
      {
        cmd = wxT("rest(") + wiz->GetValue1();
        wxString num = wiz->GetValue2();
        num.Trim(true);
        num.Trim(false);
        cmd += wxT(", -") + num + wxT(")");
        MenuCommand(cmd);
      }
      wiz->Destroy();
    }
    break;
  case menu_list_lastn:
    {
      Gen2Wiz *wiz = new Gen2Wiz(_("List"), _("Number of elements"),
                                 expr, wxEmptyString,
                                 m_console->m_configuration,
                                 this, -1, _("Extract the last n elements from a list"),
                                 true);
      //wiz->Centre(wxBOTH);
      if (wiz->ShowModal() == wxID_OK)
      {
        cmd = wxT("rest(") + wiz->GetValue1() + wxT(",")
          + wiz->GetValue2() + wxT(")");
        MenuCommand(cmd);
      }
      wiz->Destroy();
    }
    break;
  case menu_list_nth:
    {
      Gen2Wiz *wiz = new Gen2Wiz(_("List"), _("element number n"),
                                 expr, wxEmptyString,
                                 m_console->m_configuration,
                                 this, -1, _("Extract the nth element from a list. Slow for n>>0"),
                                 true,
                                 _("This function is slow for large n.\n"
                                   "For efficiently iterating through every element of a large list see \"Create list from list\" instead, which uses the makelist command."),
                                 _("Other than declared arrays in lists there is no way to jump to "
                                   "determine the address of the nth element other than iterating "
                                   "from one element to the other until the nth element is reached. "
                                   "Which isn't a maxima-specific phenomenon but the price one has "
                                   "to pay for lists being way easier to resize than declared "
                                   "arrays. If the address of the current element is known "
                                   "iterating to the next one is trivial, though, so\n\n"
                                   "   for i in list do <something>\n\n"
                                   "or\n\n"
                                   "   makelist(expression,i,list)\n\n"
                                   "provide highly efficient ways to do something on every list "
                                   "element.")
        );
      //wiz->Centre(wxBOTH);
      if (wiz->ShowModal() == wxID_OK)
      {
        cmd = wiz->GetValue1() + wxT("[")
          + wiz->GetValue2() + wxT("]");
        MenuCommand(cmd);
      }
      wiz->Destroy();
    }
  break;
  case menu_list_map:
    {
      Gen2Wiz *wiz = new Gen2Wiz(_("Function"), _("List"),
                                 expr, wxEmptyString,
                                 m_console->m_configuration,
                                 this, -1, _("Apply a function to each list element"), true);
      //wiz->Centre(wxBOTH);
      if (wiz->ShowModal() == wxID_OK)
      {
        cmd = wxT("map(") + wiz->GetValue1() + wxT(",")
          + wiz->GetValue2() + wxT(")");
        MenuCommand(cmd);
      }
      wiz->Destroy();
    }
    break;
  case menu_list_use_actual_values:
    {
      Gen2Wiz *wiz = new Gen2Wiz(_("Equation"), _("List with values"),
                                 expr, wxEmptyString,
                                 m_console->m_configuration,
                                 this, -1, _("Introduce a list of actual values into an equation"), true);
      wiz->SetLabel2ToolTip(_("The list with values can be generated by \"solve()\" or using "
                              "\"Create list/as storage for actual values for variables\"."));
      //wiz->Centre(wxBOTH);
      if (wiz->ShowModal() == wxID_OK)
      {
        cmd = wxT("subst(") + wiz->GetValue2() + wxT(",")
          + wiz->GetValue1() + wxT(")");
        MenuCommand(cmd);
      }
      wiz->Destroy();
    }
    break;
  case menu_list_extract_value:
    {
      Gen2Wiz *wiz = new Gen2Wiz(_("List"), _("Variable name"),
                                 expr, wxEmptyString,
                                 m_console->m_configuration,
                                 this, -1,
                                 _("Extract a variable's value from a list of variable values"),
                                 true);
      //wiz->Centre(wxBOTH);
      if (wiz->ShowModal() == wxID_OK)
      {
        cmd = wxT("subst(") + wiz->GetValue1() + wxT(",")
          + wiz->GetValue2() + wxT(")");
        MenuCommand(cmd);
      }
      wiz->Destroy();
    }
    break;
  case menu_list_as_function_arguments:
    {
      Gen2Wiz *wiz = new Gen2Wiz(_("Function name"), _("List"),
                                 expr, wxEmptyString,
                                 m_console->m_configuration,
                                 this, -1,
                                 _("Use a list as parameter list for a function"),
                                 true);
      //wiz->Centre(wxBOTH);
      if (wiz->ShowModal() == wxID_OK)
      {
        cmd = wxT("apply(") + wiz->GetValue1() + wxT(",")
          + wiz->GetValue2() + wxT(")");
        MenuCommand(cmd);
      }
      wiz->Destroy();
    }
    break;
  case menu_list_do_for_each_element:
  {
    Gen3Wiz *wiz = new Gen3Wiz(_("List:"), _("Iterator:"),
                               _("What to do:"),
                               expr, wxT("i"), wxT("disp(i)"),
                               m_console->m_configuration,
                               this, -1, _("Do for each list element"), true);
    wiz->SetValue(expr);
    wiz->SetLabel2ToolTip(_("The variable the value of the current source item is stored in."));
    wiz->SetLabel3ToolTip(_("Either a single expression or a comma-separated list of expressions "
                            "between parenthesis. In the latter case the result of the last "
                            "expression in the parenthesis is used."));
    //wiz->Centre(wxBOTH);    if (wiz->ShowModal() == wxID_OK)
    {
      wxString val = wxT("for ") + wiz->GetValue2() + wxT(" in ") +
        wiz->GetValue1() + wxT(" do ") + wiz->GetValue3();
      MenuCommand(val);
    }
    wiz->Destroy();
  }
    break;
  case menu_list_remove_duplicates:
    MenuCommand(wxT("unique(") + expr + wxT(")"));
    break;
  case menu_list_remove_element:
    {
      Gen2Wiz *wiz = new Gen2Wiz(_("Element"), _("List"),
                                 wxT("1"), expr,
                                 m_console->m_configuration,
                                 this, -1,
                                 _("Remove an element from a list"),
                                 true);
      //wiz->Centre(wxBOTH);
      if (wiz->ShowModal() == wxID_OK)
      {
        cmd = wxT("delete(") + wiz->GetValue1() + wxT(",")
          + wiz->GetValue2() + wxT(")");
        MenuCommand(cmd);
      }
      wiz->Destroy();
    }
    break;
  case menu_list_append_item:
    {
      Gen2Wiz *wiz = new Gen2Wiz(_("List"), _("Item"),
                                 expr, wxT("1"),
                                 m_console->m_configuration,
                                 this, -1,
                                 _("Add an element to a list"),
                                 true);
      //wiz->Centre(wxBOTH);
      if (wiz->ShowModal() == wxID_OK)
      {
        cmd = wxT("append(") + wiz->GetValue1() + wxT(",[")
          + wiz->GetValue2() + wxT("])");
        MenuCommand(cmd);
      }
      wiz->Destroy();
    }
    break;
  case menu_list_append_list:
    {
      Gen2Wiz *wiz = new Gen2Wiz(_("List1"), _("List2"),
                                 expr, wxT("[1]"),
                                 m_console->m_configuration,
                                 this, -1,
                                 _("Append a list to a list"),
                                 true);
      //wiz->Centre(wxBOTH);
      if (wiz->ShowModal() == wxID_OK)
      {
        cmd = wxT("append(") + wiz->GetValue1() + wxT(",")
          + wiz->GetValue2() + wxT(")");
        MenuCommand(cmd);
      }
      wiz->Destroy();
    }
    break;
  case menu_list_interleave:
    {
      Gen2Wiz *wiz = new Gen2Wiz(_("List1"), _("List2"),
                                 expr, wxT("[1]"),
                                 m_console->m_configuration,
                                 this, -1,
                                 _("Interleave two lists"),
                                 true);
      //wiz->Centre(wxBOTH);
      if (wiz->ShowModal() == wxID_OK)
      {
        cmd = wxT("join(") + wiz->GetValue1() + wxT(",")
          + wiz->GetValue2() + wxT(")");
        MenuCommand(cmd);
      }
      wiz->Destroy();
    }
    break;
  }
}

void wxMaxima::SimplifyMenu(wxCommandEvent &event)
{
  if(m_console != NULL)
    m_console->CloseAutoCompletePopup();

  wxString expr = GetDefaultEntry();
  wxString cmd;
  switch (event.GetId())
  {
    case menu_nouns:
      cmd = wxT("ev(") + expr + wxT(", nouns);");
      MenuCommand(cmd);
      break;
    case button_ratsimp:
    case menu_ratsimp:
      cmd = wxT("ratsimp(") + expr + wxT(");");
      MenuCommand(cmd);
      break;
    case button_radcan:
    case menu_radsimp:
      cmd = wxT("radcan(") + expr + wxT(");");
      MenuCommand(cmd);
      break;
    case menu_to_fact:
      cmd = wxT("makefact(") + expr + wxT(");");
      MenuCommand(cmd);
      break;
    case menu_to_gamma:
      cmd = wxT("makegamma(") + expr + wxT(");");
      MenuCommand(cmd);
      break;
    case menu_factcomb:
      cmd = wxT("factcomb(") + expr + wxT(");");
      MenuCommand(cmd);
      break;
    case menu_factsimp:
      cmd = wxT("minfactorial(") + expr + wxT(");");
      MenuCommand(cmd);
      break;
    case menu_logcontract:
      cmd = wxT("logcontract(") + expr + wxT(");");
      MenuCommand(cmd);
      break;
    case menu_logexpand:
      cmd = expr + wxT(", logexpand=super;");
      MenuCommand(cmd);
      break;
    case button_expand:
    case menu_expand:
      cmd = wxT("expand(") + expr + wxT(");");
      MenuCommand(cmd);
      break;
    case button_factor:
    case menu_factor:
      cmd = wxT("factor(") + expr + wxT(");");
      MenuCommand(cmd);
      break;
    case menu_gfactor:
      cmd = wxT("gfactor(") + expr + wxT(");");
      MenuCommand(cmd);
      break;
    case button_trigreduce:
    case menu_trigreduce:
      cmd = wxT("trigreduce(") + expr + wxT(");");
      MenuCommand(cmd);
      break;
    case button_trigsimp:
    case menu_trigsimp:
      cmd = wxT("trigsimp(") + expr + wxT(");");
      MenuCommand(cmd);
      break;
    case button_trigexpand:
    case menu_trigexpand:
      cmd = wxT("trigexpand(") + expr + wxT(");");
      MenuCommand(cmd);
      break;
    case menu_trigrat:
    case button_trigrat:
      cmd = wxT("trigrat(") + expr + wxT(");");
      MenuCommand(cmd);
      break;
    case button_rectform:
    case menu_rectform:
      cmd = wxT("rectform(") + expr + wxT(");");
      MenuCommand(cmd);
      break;
    case menu_polarform:
      cmd = wxT("polarform(") + expr + wxT(");");
      MenuCommand(cmd);
      break;
    case menu_demoivre:
      cmd = wxT("demoivre(") + expr + wxT(");");
      MenuCommand(cmd);
      break;
    case menu_exponentialize:
      cmd = wxT("exponentialize(") + expr + wxT(");");
      MenuCommand(cmd);
      break;
    case menu_realpart:
      cmd = wxT("realpart(") + expr + wxT(");");
      MenuCommand(cmd);
      break;
    case menu_imagpart:
      cmd = wxT("imagpart(") + expr + wxT(");");
      MenuCommand(cmd);
      break;
    case menu_talg:
      cmd = wxT("algebraic : not(algebraic);");
      MenuCommand(cmd);
      break;
    case menu_tellrat:
      cmd = GetTextFromUser(_("Enter an equation for rational simplification:"),
                            _("Tellrat"),
                            m_console->m_configuration,
                            wxEmptyString, this);
      if (cmd.Length())
      {
        cmd = wxT("tellrat(") + cmd + wxT(");");
        MenuCommand(cmd);
      }
      break;
    case menu_modulus:
      cmd = GetTextFromUser(_("Calculate modulus:"),
                            _("Modulus"),
                            m_console->m_configuration,
                            wxT("false"), this);
      if (cmd.Length())
      {
        cmd = wxT("modulus : ") + cmd + wxT(";");
        MenuCommand(cmd);
      }
      break;
    default:
      break;
  }
}

void wxMaxima::CalculusMenu(wxCommandEvent &event)
{
  if(m_console != NULL)
    m_console->CloseAutoCompletePopup();

  wxString expr = GetDefaultEntry();
  wxString cmd;
  switch (event.GetId())
  {
    case menu_change_var:
    {
      Gen4Wiz *wiz = new Gen4Wiz(_("Integral/Sum:"), _("Old variable:"),
                                 _("New variable:"), _("Equation:"),
                                 expr, wxT("x"), wxT("y"), wxT("y=x"),
                                 m_console->m_configuration,
                                 this, -1, _("Change variable"), true);
      wiz->SetValue(expr);
      //wiz->Centre(wxBOTH);
      if (wiz->ShowModal() == wxID_OK)
      {
        wxString val = wxT("changevar(") + wiz->GetValue1() + wxT(", ") +
                       wiz->GetValue4() + wxT(", ") + wiz->GetValue3() + wxT(", ") +
                       wiz->GetValue2() + wxT(");");
        MenuCommand(val);
      }
      wiz->Destroy();
    }
      break;
    case menu_pade:
    {
      Gen3Wiz *wiz = new Gen3Wiz(_("Taylor series:"), _("Num. deg:"),
                                 _("Denom. deg:"), expr, wxT("4"), wxT("4"),
                                 m_console->m_configuration,
                                 this, -1, _("Pade approximation"));
      //wiz->Centre(wxBOTH);
      if (wiz->ShowModal() == wxID_OK)
      {
        wxString val = wxT("pade(") + wiz->GetValue1() + wxT(", ") +
                       wiz->GetValue2() + wxT(", ") + wiz->GetValue3() + wxT(");");
        MenuCommand(val);
      }
      wiz->Destroy();
    }
      break;
    case menu_continued_fraction:
      cmd += wxT("cfdisrep(cf(") + expr + wxT("));");
      MenuCommand(cmd);
      break;
    case menu_lcm:
    {
      Gen2Wiz *wiz = new Gen2Wiz(_("Polynomial 1:"), _("Polynomial 2:"),
                                 wxEmptyString, wxEmptyString,
                                 m_console->m_configuration,
                                 this, -1, _("LCM"), true);
      //wiz->Centre(wxBOTH);
      if (wiz->ShowModal() == wxID_OK)
      {
        cmd = wxT("lcm(") + wiz->GetValue1() + wxT(", ")
              + wiz->GetValue2() + wxT(");");
        MenuCommand(cmd);
      }
      wiz->Destroy();
    }
      break;
    case menu_gcd:
    {
      Gen2Wiz *wiz = new Gen2Wiz(_("Polynomial 1:"), _("Polynomial 2:"),
                                 wxEmptyString, wxEmptyString,
                                 m_console->m_configuration,
                                 this, -1, _("GCD"), true);
      //wiz->Centre(wxBOTH);
      if (wiz->ShowModal() == wxID_OK)
      {
        cmd = wxT("gcd(") + wiz->GetValue1() + wxT(", ")
              + wiz->GetValue2() + wxT(");");
        MenuCommand(cmd);
      }
      wiz->Destroy();
    }
      break;
    case menu_divide:
    {
      Gen2Wiz *wiz = new Gen2Wiz(_("Polynomial 1:"), _("Polynomial 2:"),
                                 expr, wxEmptyString,
                                 m_console->m_configuration,
                                 this, -1, _("Divide"), true);
      //wiz->Centre(wxBOTH);
      if (wiz->ShowModal() == wxID_OK)
      {
        cmd = wxT("divide(") + wiz->GetValue1() + wxT(", ") +
              wiz->GetValue2() + wxT(");");
        MenuCommand(cmd);
      }
      wiz->Destroy();
    }
      break;
    case menu_partfrac:
    {
      Gen2Wiz *wiz = new Gen2Wiz(_("Expression:"), _("Variable:"),
                                 expr, wxT("n"),
                                 m_console->m_configuration,
                                 this, -1, _("Partial fractions"));
      //wiz->Centre(wxBOTH);
      if (wiz->ShowModal() == wxID_OK)
      {
        cmd = wxT("partfrac(") + wiz->GetValue1() + wxT(", ")
              + wiz->GetValue2() + wxT(");");
        MenuCommand(cmd);
      }
      wiz->Destroy();
    }
      break;
    case menu_risch:
    {
      Gen2Wiz *wiz = new Gen2Wiz(_("Expression:"), _("Variable:"),
                                 expr, wxT("x"),
                                 m_console->m_configuration,
                                 this, -1, _("Integrate (risch)"));
      //wiz->Centre(wxBOTH);
      if (wiz->ShowModal() == wxID_OK)
      {
        cmd = wxT("risch(") + wiz->GetValue1() + wxT(", ")
              + wiz->GetValue2() + wxT(");");
        MenuCommand(cmd);
      }
      wiz->Destroy();
    }
      break;
    case button_integrate:
    case menu_integrate:
    {
      IntegrateWiz *wiz = new IntegrateWiz(this, -1, m_console->m_configuration, _("Integrate"));
      wiz->SetValue(expr);
      //wiz->Centre(wxBOTH);
      if (wiz->ShowModal() == wxID_OK)
      {
        wxString val = wiz->GetValue();
        MenuCommand(val);
      }
      wiz->Destroy();
    }
      break;
    case menu_laplace:
    {
      Gen3Wiz *wiz = new Gen3Wiz(_("Expression:"), _("Old variable:"),
                                 _("New variable:"), expr, wxT("t"), wxT("s"),
                                 m_console->m_configuration,
                                 this, -1, _("Laplace"));
      wiz->SetValue(expr);
      //wiz->Centre(wxBOTH);
      if (wiz->ShowModal() == wxID_OK)
      {
        wxString val = wxT("laplace(") + wiz->GetValue1() + wxT(", ")
                       + wiz->GetValue2() +
                       wxT(", ") + wiz->GetValue3() + wxT(");");
        MenuCommand(val);
      }
      wiz->Destroy();
    }
      break;
    case menu_ilt:
    {
      Gen3Wiz *wiz = new Gen3Wiz(_("Expression:"), _("Old variable:"),
                                 _("New variable:"), expr, wxT("s"), wxT("t"),
                                 m_console->m_configuration,
                                 this, -1, _("Inverse Laplace"));
      wiz->SetValue(expr);
      //wiz->Centre(wxBOTH);
      if (wiz->ShowModal() == wxID_OK)
      {
        wxString val = wxT("ilt(") + wiz->GetValue1() + wxT(", ") +
                       wiz->GetValue2() + wxT(", ") + wiz->GetValue3() + wxT(");");
        MenuCommand(val);
      }
      wiz->Destroy();
    }
      break;
    case button_diff:
    case menu_diff:
    {
      Gen3Wiz *wiz = new Gen3Wiz(_("Expression:"), _("Variable(s):"),
                                 _("Times:"), expr, wxT("x"), wxT("1"),
                                 m_console->m_configuration,
                                 this, -1, _("Differentiate"));
      wiz->SetValue(expr);
      //wiz->Centre(wxBOTH);
      if (wiz->ShowModal() == wxID_OK)
      {
        wxStringTokenizer vars(wiz->GetValue2(), wxT(","));
        wxStringTokenizer times(wiz->GetValue3(), wxT(","));

        wxString val = wxT("diff(") + wiz->GetValue1();

        while (vars.HasMoreTokens() && times.HasMoreTokens())
        {
          val += wxT(",") + vars.GetNextToken();
          val += wxT(",") + times.GetNextToken();
        }

        val += wxT(");");
        MenuCommand(val);
      }
      wiz->Destroy();
    }
      break;
    case button_taylor:
    case menu_series:
    {
      SeriesWiz *wiz = new SeriesWiz(this, -1, m_console->m_configuration, _("Series"));
      wiz->SetValue(expr);
      //wiz->Centre(wxBOTH);
      if (wiz->ShowModal() == wxID_OK)
      {
        wxString val = wiz->GetValue();
        MenuCommand(val);
      }
      wiz->Destroy();
    }
      break;
    case button_limit:
    case menu_limit:
    {
      LimitWiz *wiz = new LimitWiz(this, -1, m_console->m_configuration, _("Limit"));
      wiz->SetValue(expr);
      //wiz->Centre(wxBOTH);
      if (wiz->ShowModal() == wxID_OK)
      {
        wxString val = wiz->GetValue();
        MenuCommand(val);
      }
      wiz->Destroy();
    }
      break;
    case menu_lbfgs:
    {
      Gen4Wiz *wiz = new Gen4Wiz(_("Expression:"),
                                 _("Variables:"),
                                 _("Initial Estimates:"),
                                 _("Epsilon:"),
                                 expr, wxT("x"), wxT("1.0"), wxT("1e-4"),
                                 m_console->m_configuration,
                                 this, -1, _("Find minimum"));
      //wiz->Centre(wxBOTH);
      if (wiz->ShowModal() == wxID_OK)
      {
        cmd = wxT("lbfgs(") + wiz->GetValue1() + wxT(", [") +
              wiz->GetValue2() + wxT("], [") +
              wiz->GetValue3() + wxT("], ") +
              wiz->GetValue4() + wxT(", [-1,0]);");
        MenuCommand(cmd);
      }
      wiz->Destroy();
    }
      break;
    case button_sum:
    case menu_sum:
    {
      SumWiz *wiz = new SumWiz(this, -1, m_console->m_configuration, _("Sum"));
      wiz->SetValue(expr);
      //wiz->Centre(wxBOTH);
      if (wiz->ShowModal() == wxID_OK)
      {
        wxString val = wiz->GetValue();
        MenuCommand(val);
      }
      wiz->Destroy();
    }
      break;
    case button_product:
    case menu_product:
    {
      Gen4Wiz *wiz = new Gen4Wiz(_("Expression:"), _("Variable:"), _("From:"),
                                 _("To:"), expr, wxT("k"), wxT("1"), wxT("n"),
                                 m_console->m_configuration,
                                 this, -1, _("Product"));
      //wiz->Centre(wxBOTH);
      if (wiz->ShowModal() == wxID_OK)
      {
        cmd = wxT("product(") + wiz->GetValue1() + wxT(", ") +
              wiz->GetValue2() + wxT(", ") +
              wiz->GetValue3() + wxT(", ") +
              wiz->GetValue4() + wxT(");");
        MenuCommand(cmd);
      }
      wiz->Destroy();
    }
      break;
    default:
      break;
  }
}

void wxMaxima::PlotMenu(wxCommandEvent &event)
{
  if(m_console != NULL)
    m_console->CloseAutoCompletePopup();

  wxString expr = GetDefaultEntry();
  wxString cmd;
  switch (event.GetId())
  {
    case button_plot3:
    case gp_plot3:
    {
      Plot3DWiz *wiz = new Plot3DWiz(this, -1, m_console->m_configuration, _("Plot 3D"));
      wiz->SetValue(expr);
      //wiz->Centre(wxBOTH);
      if (wiz->ShowModal() == wxID_OK)
      {
        wxString val = wiz->GetValue();
        MenuCommand(val);
      }
      wiz->Destroy();
    }
      break;
    case menu_animationautostart:
      MenuCommand(wxT("if wxanimate_autoplay#false then wxanimate_autoplay:false else wxanimate_autoplay:true;"));
      break;
    case menu_animationframerate:
    {
      cmd = GetTextFromUser(_("Enter new animation frame rate [Hz, integer]:"), _("Frame rate"),
                            m_console->m_configuration,
                            wxT("2"), this);
      wxRegEx number("^[0-9][0-9]*$");

      if (number.Matches(cmd))
      {
        cmd = wxT("wxanimate_framerate : ") + cmd + wxT(";");
        MenuCommand(cmd);
      }
    }
    break;
    case button_plot2:
    case gp_plot2:
    {
      Plot2DWiz *wiz = new Plot2DWiz(this, -1, m_console->m_configuration, _("Plot 2D"));
      wiz->SetValue(expr);
      //wiz->Centre(wxBOTH);
      if (wiz->ShowModal() == wxID_OK)
      {
        wxString val = wiz->GetValue();
        MenuCommand(val);
      }
      wiz->Destroy();
    }
      break;
    case menu_plot_format:
    {
      PlotFormatWiz *wiz = new PlotFormatWiz(this, -1, m_console->m_configuration, _("Plot format"));
      wiz->Center(wxBOTH);
      if (wiz->ShowModal() == wxID_OK)
      {
        MenuCommand(wiz->GetValue());
      }
      wiz->Destroy();
      /*wxString format = GetTextFromUser(_("Enter new plot format:"),
      _("Plot format"),
      m_console->m_configuration,
      wxT("gnuplot"), this);
      if (format.Length())
      {
      MenuCommand(wxT("set_plot_option(['plot_format, '") + format +
      wxT("])$"));
      }*/
    }
    default:
      break;
  }
}

void wxMaxima::NumericalMenu(wxCommandEvent &event)
{
  if(m_console != NULL)
    m_console->CloseAutoCompletePopup();

  wxString expr = GetDefaultEntry();
  wxString cmd;
  switch (event.GetId())
  {
    case menu_to_float:
      cmd = wxT("float(") + expr + wxT("), numer;");
      MenuCommand(cmd);
      break;
    case menu_to_bfloat:
      cmd = wxT("bfloat(") + expr + wxT(");");
      MenuCommand(cmd);
      break;
    case menu_to_numer:
      cmd = expr + wxT(",numer;");
      MenuCommand(cmd);
      break;
    case menu_num_out:
      cmd = wxT("if numer#false then numer:false else numer:true;");
      MenuCommand(cmd);
      break;
    case menu_set_precision:
      cmd = GetTextFromUser(_("Enter new precision for bigfloats:"), _("Precision"),
                            m_console->m_configuration,
                            wxT("16"), this);
      if (cmd.Length())
      {
        cmd = wxT("fpprec : ") + cmd + wxT(";");
        MenuCommand(cmd);
      }
      break;
    case menu_set_displayprecision:
      cmd = GetTextFromUser(_("How many digits to show:"), _("Displayed Precision"),
                            m_console->m_configuration,
                            wxT("0"), this);
      if (cmd.Length())
      {
        cmd = wxT("fpprintprec : ") + cmd + wxT(";");
        MenuCommand(cmd);
      }
      break;
  case menu_engineeringFormat:
    MenuCommand(wxT("load(\"engineering-format\")$"));
    break;
  case menu_engineeringFormatSetup:
  {
    Gen4Wiz *wiz = new Gen4Wiz(_("Enable:"),
                               _("Minimum absolute value printed without exponent:"),
                               _("Maximum absolute value printed without exponent:"),
                               _("Maximum number of digits to be displayed:"),
                               wxT("true"), wxT(".01"), wxT("1000"), wxT("6"),
                               m_console->m_configuration,
                               this, -1, _("Engineering format setup"));
    //wiz->Centre(wxBOTH);
    if (wiz->ShowModal() == wxID_OK)
    {
      cmd = wxT("engineering_format_floats: ") + wiz->GetValue1() + wxT("$\n") +
        wxT("engineering_format_min: ") + wiz->GetValue2() + wxT("$\n") +
        wxT("engineering_format_max: ") + wiz->GetValue3() + wxT("$\n") +
        wxT("fpprintprec: ") + wiz->GetValue4() + wxT("$\n");
      MenuCommand(cmd);
    }
    wiz->Destroy();
  }
  break;
  default:
    break;
  }
}

#ifndef __WXGTK__

MyAboutDialog::MyAboutDialog(wxWindow *parent, int id, const wxString title, wxString description) :
        wxDialog(parent, id, title)
{

  wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);

  wxHtmlWindow *html_top = new wxHtmlWindow(this, -1, wxDefaultPosition, wxSize(380, 250), wxHW_SCROLLBAR_NEVER);
  html_top->SetBorders(5);

  wxHtmlWindow *html_bottom = new wxHtmlWindow(this, -1, wxDefaultPosition, wxSize(380, 280));
  html_bottom->SetBorders(5);

  wxString cwd = wxGetCwd();
#if defined __WXMAC__
  cwd = cwd + wxT("/") + wxT(MACPREFIX);
#else
  cwd.Replace(wxT("\\"), wxT("/"));
  cwd = cwd + wxT("/data/");
#endif

  wxString page_top = wxString::Format(
          wxT("<html>"
                      "<head>"
                      "</head>"
                      "<body>"
                      "<center>"
                      "<p>"
                      "<img src=\"%s/wxmaxima.png\">"
                      "</p>"
                      "<h1>wxMaxima</h1>"
                      "<p>%s</p>"
                      "<p><small>(C) 2004 - 2018 Andrej Vodopivec</small><br></p>"
                      "</center>"
                      "</body>"
                      "</html>"),
          cwd,
          wxT(GITVERSION));

  wxString page_bottom = wxString::Format(
          wxT("<html>"
                      "<head>"
                      "</head>"
                      "<body>"
                      "<center>"
                      "<p>"
                      "%s"
                      "</p>"
                      "<p><a href=\"https://andrejv.github.io/wxmaxima/\">wxMaxima</a><br>"
                      "   <a href=\"http://maxima.sourceforge.net/\">Maxima</a></p>"
                      "<h4>%s</h4>"
                      "<p>"
                      "wxWidgets: %d.%d.%d<br>"
                      "%s: %s<br>"
                      "%s"
                      "</p>"
                      "<h4>%s</h4>"
                      "<p>"
                      "Andrej Vodopivec<br>"
                      "Ziga Lenarcic<br>"
                      "Doug Ilijev<br>"
                      "Gunter Königsmann<br>"
                      "</p>"
                      "<h4>Patches</h4>"
                      "Sandro Montanar (SF-patch 2537150)"
                      "Wolfgang Dautermann"
                      "</p>"
                      "<h4>%s</h4>"
                      "<p>"
                      "%s: Sven Hodapp<br>"
                      "%s: <a href=\"http://tango.freedesktop.org/Tango_Desktop_Project\">TANGO project</a>"
                      "</p>"
                      "<h4>%s</h4>"
                      "<p>"
                      "Innocent De Marchi (ca)<br>"
                      "Josef Barak (cs)<br>"
                      "Robert Marik (cs)<br>"
                      "Jens Thostrup (da)<br>"
                      "Harald Geyer (de)<br>"
                      "Dieter Kaiser (de)<br>"
                      "Gunter Königsmann (de)<br>"
                      "Alkis Akritas (el)<br>"
                      "Evgenia Kelepesi-Akritas (el)<br>"
                      "Kostantinos Derekas (el)<br>"
                      "Mario Rodriguez Riotorto (es)<br>"
                      "Antonio Ullan (es)<br>"
                      "Eric Delevaux (fr)<br>"
                      "Michele Gosse (fr)<br>"
                      "Marco Ciampa (it)<br>"
                      "Blahota István (hu)<br>"
                      "Asbjørn Apeland (nb)<br>"
                      "Rafal Topolnicki (pl)<br>"
                      "Eduardo M. Kalinowski (pt_br)<br>"
                      "Alexey Beshenov (ru)<br>"
                      "Vadim V. Zhytnikov (ru)<br>"
                      "Sergey Semerikov (uk)<br>"
                      "Tufan Şirin (tr)<br>"
                      "Frank Weng (zh_TW)<br>"
                      "cw.ahbong (zh_TW)"
                      "  </p>"
                      "</center>"
                      "</body>"
                      "</html>"),
          _("wxMaxima is a graphical user interface for the computer algebra system MAXIMA based on wxWidgets."),
          _("System info"),
          wxMAJOR_VERSION, wxMINOR_VERSION, wxRELEASE_NUMBER,
          _("Unicode Support"),
          wxT("yes"),
          description.c_str(),
          _("Written by"),
          _("Artwork by"),
          _("wxMaxima icon"),
          _("Toolbar icons"),
          _("Translated by"));

  html_top->SetPage(page_top);
  html_bottom->SetPage(page_bottom);

  html_top->SetSize(wxDefaultCoord,
                    html_top->GetInternalRepresentation()->GetHeight());

  sizer->Add(html_top, 0, wxALL, 0);
  sizer->Add(html_bottom, 0, wxALL, 0);

  SetSizer(sizer);
  sizer->Fit(this);
  sizer->SetSizeHints(this);

  SetAutoLayout(true);
  Layout();
}

void MyAboutDialog::OnLinkClicked(wxHtmlLinkEvent &event)
{
  wxLaunchDefaultBrowser(event.GetLinkInfo().GetHref());
}

BEGIN_EVENT_TABLE(MyAboutDialog, wxDialog)
                EVT_HTML_LINK_CLICKED(wxID_ANY, MyAboutDialog::OnLinkClicked)
END_EVENT_TABLE()

#endif

void wxMaxima::HelpMenu(wxCommandEvent &event)
{
  if(m_console != NULL)
    m_console->CloseAutoCompletePopup();

  wxString expr = GetDefaultEntry();
  wxString cmd;
  wxString helpSearchString = wxT("%");
  if (m_console->CanCopy(true))
    helpSearchString = m_console->GetString();
  else if (m_console->GetActiveCell() != NULL)
  {
    helpSearchString = m_console->GetActiveCell()->SelectWordUnderCaret(false);
  }
  if (helpSearchString == wxT(""))
    helpSearchString = wxT("%");

  switch (event.GetId())
  {
    case wxID_ABOUT:
#if defined __WXGTK__
    {
      wxAboutDialogInfo info;
      wxString description;

      description = _("wxMaxima is a graphical user interface for the computer algebra system Maxima based on wxWidgets.");

#if defined(WXMAXIMA_GIT_VERSION)
      description += wxString::Format("\n(Build from Git version: " WXMAXIMA_GIT_VERSION ")");
#endif
      description += wxString::Format(
        _("\n\nwxWidgets: %d.%d.%d\nUnicode support: %s"),
        wxMAJOR_VERSION, wxMINOR_VERSION, wxRELEASE_NUMBER,
        _("yes")
        );

      if (m_maximaVersion != wxEmptyString)
        description += _("\nMaxima version: ") + m_maximaVersion + " ("+m_maximaArch+")";
      else
        description += _("\nNot connected.");
      if (m_lispVersion != wxEmptyString)
        description += _("\nLisp: ") + m_lispType + " " + m_lispVersion;

      info.SetIcon(wxMaximaIcon());
      info.SetDescription(description);
      info.SetName(_("wxMaxima"));
      info.SetVersion(wxT(GITVERSION));
      info.SetCopyright(wxT("(C) 2004-2018 Andrej Vodopivec"));
      info.SetWebSite(wxT("https://andrejv.github.io/wxmaxima/"));

      info.AddDeveloper(wxT("Andrej Vodopivec <andrej.vodopivec@gmail.com>"));
      info.AddDeveloper(wxT("Ziga Lenarcic <ziga.lenarcic@gmail.com>"));
      info.AddDeveloper(wxT("Doug Ilijev <doug.ilijev@gmail.com>"));
      info.AddDeveloper(wxT("Gunter Königsmann <wxMaxima@physikbuch.de>"));

      info.AddTranslator(wxT("Innocent De Marchi (ca)"));
      info.AddTranslator(wxT("Josef Barak (cs)"));
      info.AddTranslator(wxT("Robert Marik (cs)"));
      info.AddTranslator(wxT("Jens Thostrup (da)"));
      info.AddTranslator(wxT("Harald Geyer (de)"));
      info.AddTranslator(wxT("Dieter Kaiser (de)"));
      info.AddTranslator(wxT("Gunter Königsmann (de)"));
      info.AddTranslator(wxT("Alkis Akritas (el)"));
      info.AddTranslator(wxT("Evgenia Kelepesi-Akritas (el)"));
      info.AddTranslator(wxT("Kostantinos Derekas (el)"));
      info.AddTranslator(wxT("Mario Rodriguez Riotorto (es)"));
      info.AddTranslator(wxT("Antonio Ullan (es)"));
      info.AddTranslator(wxT("Eric Delevaux (fr)"));
      info.AddTranslator(wxT("Michele Gosse (fr)"));
      info.AddTranslator(wxT("Blahota István (hu)"));
      info.AddTranslator(wxT("Marco Ciampa (it)"));
      info.AddTranslator(wxT("Asbjørn Apeland (nb)"));
      info.AddTranslator(wxT("Rafal Topolnicki (pl)"));
      info.AddTranslator(wxT("Eduardo M. Kalinowski (pt_br)"));
      info.AddTranslator(wxT("Alexey Beshenov (ru)"));
      info.AddTranslator(wxT("Vadim V. Zhytnikov (ru)"));
      info.AddTranslator(wxT("Tufan Şirin (tr)"));
      info.AddTranslator(wxT("Sergey Semerikov (uk)"));
      info.AddTranslator(wxT("Frank Weng (zh_TW)"));
      info.AddTranslator(wxT("cw.ahbong (zh_TW)"));

      info.AddArtist(wxT("wxMaxima icon: Sven Hodapp"));
      info.AddArtist(wxT("Toolbar and config icons: The TANGO Project"));
      info.AddArtist(wxT("svg version of the icon: Gunter Königsmann"));

      wxAboutBox(info);
    }
#else
    {
      wxString description;

      if (m_maximaVersion != wxEmptyString)
        description += _("Maxima version: ") + m_maximaVersion + " ("+m_maximaArch+")";
      else
        description += _("Not connected.");
      if (m_lispVersion != wxEmptyString)
        description += _("<br>Lisp: ") + m_lispType + " " + m_lispVersion;

      MyAboutDialog dlg(this, wxID_ANY, wxString(_("About")), description);
      dlg.Center();
      dlg.ShowModal();
    }
#endif

      break;

    case wxID_HELP:
    case ToolBar::tb_help:
      if (helpSearchString == wxT("%"))
        ShowWxMaximaHelp();
      else
        ShowMaximaHelp(helpSearchString);
      break;

    case menu_maximahelp:
      ShowMaximaHelp(expr);
      break;

    case menu_example:
      if (expr == wxT("%"))
        cmd = GetTextFromUser(_("Show an example for the command:"), _("Example"),
                              m_console->m_configuration,
                              wxEmptyString, this);
      else
        cmd = expr;
      if (cmd.Length())
      {
        cmd = wxT("example(") + cmd + wxT(");");
        MenuCommand(cmd);
      }
      break;

    case menu_apropos:
      if (expr == wxT("%"))
        cmd = GetTextFromUser(_("Show all commands similar to:"), _("Apropos"),
                              m_console->m_configuration,
                              wxEmptyString, this);
      else
        cmd = expr;
      if (cmd.Length())
      {
        cmd = wxT("apropos(\"") + cmd + wxT("\");");
        MenuCommand(cmd);
      }
      break;

    case menu_show_tip:
      ShowTip(true);
      break;

    case menu_build_info:
      MenuCommand(wxT("wxbuild_info()$"));
      break;

    case menu_bug_report:
      MenuCommand(wxT("wxbug_report()$"));
      break;

    case menu_help_tutorials:
      wxLaunchDefaultBrowser(wxT("https://andrejv.github.io/wxmaxima/help.html"));
      break;

    case menu_check_updates:
      CheckForUpdates(true);
      break;

    default:
      break;
  }
}

void wxMaxima::StatsMenu(wxCommandEvent &ev)
{
  if(m_console != NULL)
    m_console->CloseAutoCompletePopup();

  wxString expr = GetDefaultEntry();

  switch (ev.GetId())
  {
    case menu_stats_histogram:
    {
      Gen2Wiz *wiz = new Gen2Wiz(_("Data:"), _("Classes:"),
                                 expr, wxT("10"),
                                 m_console->m_configuration,
                                 this, -1, _("Histogram"), false);
      //wiz->Centre(wxBOTH);
      if (wiz->ShowModal() == wxID_OK)
      {
        wxString cmd = wxT("wxhistogram(") + wiz->GetValue1() + wxT(", nclasses=") +
                       wiz->GetValue2() + wxT(");");
        MenuCommand(cmd);
      }
      wiz->Destroy();
    }
      break;
    case menu_stats_scatterplot:
    {
      Gen2Wiz *wiz = new Gen2Wiz(_("Data:"), _("Classes:"),
                                 expr, wxT("10"),
                                 m_console->m_configuration,
                                 this, -1, _("Scatterplot"), false);
      //wiz->Centre(wxBOTH);
      if (wiz->ShowModal() == wxID_OK)
      {
        wxString cmd = wxT("wxscatterplot(") + wiz->GetValue1() + wxT(", nclasses=") +
                       wiz->GetValue2() + wxT(");");
        MenuCommand(cmd);
      }
      wiz->Destroy();
    }
      break;
    case menu_stats_barsplot:
    {
      wxString data = GetTextFromUser(_("Data:"), _("Enter Data"),
                                      m_console->m_configuration,
                                      expr, this);
      if (data.Length() > 0)
        MenuCommand(wxT("wxbarsplot(") + data + wxT(");"));
    }
      break;
    case menu_stats_boxplot:
    {
      wxString data = GetTextFromUser(_("Data:"), _("Enter Data"),
                                      m_console->m_configuration,
                                      expr, this);
      if (data.Length() > 0)
        MenuCommand(wxT("wxboxplot([") + data + wxT("]);"));
    }
      break;
    case menu_stats_piechart:
    {
      wxString data = GetTextFromUser(_("Data:"), _("Enter Data"),
                                      m_console->m_configuration,
                                      expr, this);
      if (data.Length() > 0)
        MenuCommand(wxT("wxpiechart(") + data + wxT(");"));
    }
      break;
    case menu_stats_mean:
    {

      wxString data = GetTextFromUser(_("Data:"), _("Enter Data"),
                                      m_console->m_configuration,
                                      expr, this);
      if (data.Length() > 0)
        MenuCommand(wxT("mean(") + data + wxT(");"));
    }
      break;
    case menu_stats_median:
    {
      wxString data = GetTextFromUser(_("Data:"), _("Enter Data"),
                                      m_console->m_configuration,
                                      expr, this);
      if (data.Length() > 0)
        MenuCommand(wxT("median(") + data + wxT(");"));
    }
      break;
    case menu_stats_var:
    {
      wxString data = GetTextFromUser(_("Data:"), _("Enter Data"),
                                      m_console->m_configuration,
                                      expr, this);
      if (data.Length() > 0)
        MenuCommand(wxT("var(") + data + wxT(");"));
    }
      break;
    case menu_stats_dev:
    {
      wxString data = GetTextFromUser(_("Data:"), _("Enter Data"),
                                      m_console->m_configuration,
                                      expr, this);
      if (data.Length() > 0)
        MenuCommand(wxT("std(") + data + wxT(");"));
    }
      break;
    case menu_stats_tt1:
    {
      Gen2Wiz *wiz = new Gen2Wiz(_("Sample:"), _("Mean:"),
                                 expr, wxT("0"),
                                 m_console->m_configuration,
                                 this, -1, _("One sample t-test"), false);
      //wiz->Centre(wxBOTH);
      if (wiz->ShowModal() == wxID_OK)
      {
        wxString cmd = wxT("test_mean(") + wiz->GetValue1() + wxT(", mean=") +
                       wiz->GetValue2() + wxT(");");
        MenuCommand(cmd);
      }
      wiz->Destroy();
    }
      break;
    case menu_stats_tt2:
    {
      Gen2Wiz *wiz = new Gen2Wiz(_("Sample 1:"), _("Sample 2:"),
                                 wxEmptyString, wxEmptyString,
                                 m_console->m_configuration,
                                 this, -1,
                                 _("Two sample t-test"), true);
      //wiz->Centre(wxBOTH);
      if (wiz->ShowModal() == wxID_OK)
      {
        wxString cmd = wxT("test_means_difference(") + wiz->GetValue1() + wxT(", ") +
                       wiz->GetValue2() + wxT(");");
        MenuCommand(cmd);
      }
      wiz->Destroy();
    }
      break;
    case menu_stats_tnorm:
    {
      wxString data = GetTextFromUser(_("Data:"), _("Enter Data"),
                                      m_console->m_configuration,
                                      expr, this);
      if (data.Length() > 0)
        MenuCommand(wxT("test_normality(") + data + wxT(");"));
    }
      break;
    case menu_stats_linreg:
    {

      wxString data = GetTextFromUser(_("Data Matrix:"), _("Enter Data"),
                                      m_console->m_configuration,
                                      expr, this);
      if (data.Length() > 0)
        MenuCommand(wxT("simple_linear_regression(") + data + wxT(");"));
    }
      break;
    case menu_stats_lsquares:
    {
      Gen4Wiz *wiz = new Gen4Wiz(_("Data Matrix:"), _("Col. names:"),
                                 _("Equation:"), _("Variables:"),
                                 expr, wxT("x,y"), wxT("y=A*x+B"), wxT("A,B"),
                                 m_console->m_configuration,
                                 this, -1, _("Least Squares Fit"), true);
      //wiz->Centre(wxBOTH);
      if (wiz->ShowModal() == wxID_OK)
      {
        wxString cmd = wxT("lsquares_estimates(") + wiz->GetValue1() + wxT(", [") +
                       wiz->GetValue2() + wxT("], ") +
                       wiz->GetValue3() + wxT(", [") +
                       wiz->GetValue4() + wxT("], iprint=[-1,0]);");
        MenuCommand(cmd);
      }
      wiz->Destroy();
    }
      break;
    case menu_stats_readm:
    {
      wxString file = wxFileSelector(_("Open matrix"), m_lastPath,
                                     wxEmptyString, wxEmptyString,
                                     _("Data file (*.csv, *.tab, *.txt)|*.csv;*.tab;*.txt"),
                                     wxFD_OPEN);
      if (file != wxEmptyString)
      {
        m_lastPath = wxPathOnly(file);

#if defined __WXMSW__
        file.Replace(wxT("\\"), wxT("/"));
#endif

        wxString name = wxGetTextFromUser(wxT("Enter matrix name:"), wxT("Marix name"));
        wxString cmd;

        if (name != wxEmptyString)
          cmd << name << wxT(": ");

        wxString format;
        if (file.EndsWith(wxT(".csv")))
          format = wxT("csv");
        else if (file.EndsWith(wxT(".tab")))
          format = wxT("tab");

        if (format != wxEmptyString)
          MenuCommand(cmd + wxT("read_matrix(\"") + file + wxT("\", '") + format + wxT(");"));
        else
          MenuCommand(cmd + wxT("read_matrix(\"") + file + wxT("\");"));
      }
    }
      break;
    case menu_stats_subsample:
    {
      Gen4Wiz *wiz = new Gen4Wiz(_("Data Matrix:"), _("Condition:"),
                                 _("Include columns:"), _("Matrix name:"),
                                 expr, wxT("col[1]#'NA"),
                                 wxEmptyString, wxEmptyString,
                                 m_console->m_configuration,
                                 this, -1, _("Select Subsample"), true);
      //wiz->Centre(wxBOTH);
      if (wiz->ShowModal() == wxID_OK)
      {
        wxString name = wiz->GetValue4();

        wxString cmd;

        if (name != wxEmptyString)
          cmd << name << wxT(": ");

        cmd += wxT("subsample(\n   ") + wiz->GetValue1() + wxT(",\n   ") +
               wxT("lambda([col], is( ");

        if (wiz->GetValue2() != wxEmptyString)
          cmd += wiz->GetValue2() + wxT(" ))");
        else
          cmd += wxT("true ))");

        if (wiz->GetValue3() != wxEmptyString)
          cmd += wxT(",\n   ") + wiz->GetValue3();

        cmd += wxT(");");
        MenuCommand(cmd);
      }
      wiz->Destroy();
    }
      break;
  }
}

void wxMaxima::OnClose(wxCloseEvent &event)
{
  if (SaveNecessary())
  {
    int close = SaveDocumentP();

    if (close == wxID_CANCEL)
    {
      event.Veto();
      return;
    }

    if (close == wxID_YES)
    {
      if (!SaveFile())
      {
        event.Veto();
        return;
      }
    }
  }
  // We have saved the file now => No need to have the timer around any longer.
  m_autoSaveTimer.Stop();

  wxConfig *config = (wxConfig *) wxConfig::Get();
  wxSize size = GetSize();
  wxPoint pos = GetPosition();
  bool maximized = IsMaximized();
  config->Write(wxT("pos-x"), pos.x);
  config->Write(wxT("pos-y"), pos.y);
  config->Write(wxT("pos-w"), size.GetWidth());
  config->Write(wxT("pos-h"), size.GetHeight());
  if (maximized)
    config->Write(wxT("pos-max"), 1);
  else
    config->Write(wxT("pos-max"), 0);
  if (m_lastPath.Length() > 0)
    config->Write(wxT("lastPath"), m_lastPath);
  m_closing = true;
  CleanUp();
  m_maximaStdout = NULL;
  m_maximaStderr = NULL;
#if defined __WXMAC__
  wxGetApp().topLevelWindows.Erase(wxGetApp().topLevelWindows.Find(this));
#endif
  // Allow the operating system to keep the clipboard's contents even after we
  // exit - if that ioption is supported by the OS.
  wxTheClipboard->Flush();
  if (m_console->GetTree())
    m_console->DestroyTree();
  Destroy();

  RemoveTempAutosavefile();
}

void wxMaxima::PopupMenu(wxCommandEvent &event)
{
  if(m_console != NULL)
    m_console->CloseAutoCompletePopup();

  wxString selection = m_console->GetString();
  switch (event.GetId())
  {
    case MathCtrl::popid_fold:
    {
      if (m_console->GetActiveCell())
      {
        // This "if" is pure paranoia. But - since the costs of an "if" are low...
        GroupCell *group = dynamic_cast<GroupCell *>(m_console->GetActiveCell()->GetGroup());
        if (group->IsFoldable())
          group->Fold();
        else
          group->Hide(true);
        m_console->UpdateTableOfContents();
      }
      break;
    }
    case MathCtrl::popid_maxsizechooser:
      if(m_console->m_cellPointers.m_selectionStart != NULL)
      {
        MathCell *output = dynamic_cast<GroupCell *>(m_console->m_cellPointers.m_selectionStart->GetGroup())->GetLabel();
        if (output == NULL)
          return;
        if(output->GetType() != MC_TYPE_IMAGE)
          return;

        MaxSizeChooser *chooser = new MaxSizeChooser(this, -1,
                                                     dynamic_cast<ImgCell *>(output)->GetMaxWidth(),
                                                     dynamic_cast<ImgCell *>(output)->GetMaxHeight()
        );
        chooser->Centre(wxBOTH);
        if (chooser->ShowModal() == wxID_OK)
        {
          if(dynamic_cast<ImgCell *>(output)->GetMaxWidth() != chooser->GetMaxWidth())
            m_fileSaved = false;
          if(dynamic_cast<ImgCell *>(output)->GetMaxHeight() != chooser->GetMaxHeight())
            m_fileSaved = false;

          dynamic_cast<ImgCell *>(output)->SetMaxWidth(chooser->GetMaxWidth());
          dynamic_cast<ImgCell *>(output)->SetMaxHeight(chooser->GetMaxHeight());
        }
      }
      m_console->RecalculateForce();
      m_console->RequestRedraw();
      break;
    case MathCtrl::popid_unfold:
    {
      GroupCell *group = dynamic_cast<GroupCell *>(m_console->GetActiveCell()->GetGroup());
      if (group->IsFoldable())
        group->Unfold();
      else
        group->Hide(false);
      m_console->UpdateTableOfContents();
      break;
    }
    case TableOfContents::popid_Fold:
      if (m_console->m_tableOfContents != NULL)
      {
        // We only update the table of contents when there is time => no guarantee that the
        // cell that was clicked at actually still is part of the tree.
        if ((m_console->GetTree()) &&
            (m_console->GetTree()->Contains(m_console->m_tableOfContents->RightClickedOn())))
        {
          m_console->m_tableOfContents->RightClickedOn()->Fold();
          m_console->Recalculate();
          m_console->RequestRedraw();
          m_console->UpdateTableOfContents();
        }
      }
      break;
    case TableOfContents::popid_Unfold:
      if (m_console->m_tableOfContents != NULL)
      {
        // We only update the table of contents when there is time => no guarantee that the
        // cell that was clicked at actually still is part of the tree.
        if ((m_console->GetTree()) &&
            (m_console->GetTree()->Contains(m_console->m_tableOfContents->RightClickedOn())))
        {
          m_console->m_tableOfContents->RightClickedOn()->Unfold();
          m_console->Recalculate();
          m_console->RequestRedraw();
          m_console->UpdateTableOfContents();
        }
      }
      break;
    case TableOfContents::popid_SelectTocChapter:
      if (m_console->m_tableOfContents != NULL)
      {
        if (m_console->m_tableOfContents->RightClickedOn())
        {
          GroupCell *SelectionStart = m_console->m_tableOfContents->RightClickedOn();
          // We only update the table of contents when there is time => no guarantee that the
          // cell that was clicked at actually still is part of the tree.
          if((m_console->GetTree()) && (m_console->GetTree()->Contains(SelectionStart)))
          {
            GroupCell *SelectionEnd = SelectionStart;
            while (
              (SelectionEnd->m_next != NULL)
              && (dynamic_cast<GroupCell *>(SelectionEnd->m_next)->IsLesserGCType(SelectionStart->GetGroupType()))
              )
              SelectionEnd = dynamic_cast<GroupCell *>(SelectionEnd->m_next);
            m_console->SetActiveCell(NULL);
            m_console->SetHCaret(SelectionEnd);
            m_console->SetSelection(SelectionStart, SelectionEnd);
            m_console->RequestRedraw();
          }
        }
      }
      break;
    case TableOfContents::popid_EvalTocChapter:
    {
      GroupCell *SelectionStart = m_console->m_tableOfContents->RightClickedOn();
      // We only update the table of contents when there is time => no guarantee that the
      // cell that was clicked at actually still is part of the tree.
      if ((m_console->GetTree()) && (m_console->GetTree()->Contains(SelectionStart)))
      {
        bool evaluating = !m_console->m_evaluationQueue.Empty();
        m_console->AddSectionToEvaluationQueue(m_console->m_tableOfContents->RightClickedOn());
        if (!evaluating)
          TryEvaluateNextInQueue();
      }
      break;
    }
    case TableOfContents::popid_ToggleTOCshowsSectionNumbers:
    {
      m_console->m_configuration->TocShowsSectionNumbers(event.IsChecked());
      m_console->UpdateTableOfContents();
      break;
    }
    case MathCtrl::popid_evaluate_section:
    {
      bool evaluating = !m_console->m_evaluationQueue.Empty();
      GroupCell *group = NULL;
      if (m_console->GetActiveCell())
      {
        // This "if" is pure paranoia. But - since the costs of an "if" are low...
        if (m_console->GetActiveCell()->GetGroup())
          group = dynamic_cast<GroupCell *>(m_console->GetActiveCell()->GetGroup());
      }
      else if (m_console->HCaretActive())
      {
        if (m_console->GetHCaret())
        {
          group = m_console->GetHCaret();
/*        if(group->m_next)
          group = dynamic_cast<GroupCell*>(group->m_next);*/
        }
        else
          group = m_console->GetTree();
      }
      if (group)
      {
        m_console->AddSectionToEvaluationQueue(group);
        if (!evaluating)
          TryEvaluateNextInQueue();
      }
    }
      break;
    case MathCtrl::popid_evaluate:
      {
        wxCommandEvent *dummy = new wxCommandEvent;
        EvaluateEvent(*dummy);
      }
      break;
    case ToolBar::tb_evaluate_rest:
      m_console->AddRestToEvaluationQueue();
      EvaluationQueueLength(m_console->m_evaluationQueue.Size(), m_console->m_evaluationQueue.CommandsLeftInCell());
      TryEvaluateNextInQueue();
      break;
    case ToolBar::tb_evaltillhere:
      m_console->m_evaluationQueue.Clear();
      m_console->ResetInputPrompts();
      EvaluationQueueLength(0);
      if (m_console->m_configuration->RestartOnReEvaluation())
        StartMaxima();
      m_console->AddDocumentTillHereToEvaluationQueue();
      // Inform the user about the length of the evaluation queue.
      EvaluationQueueLength(m_console->m_evaluationQueue.Size(), m_console->m_evaluationQueue.CommandsLeftInCell());
      TryEvaluateNextInQueue();
      break;
    case MathCtrl::popid_copy:
      if (m_console->CanCopy(true))
        m_console->Copy();
      break;
    case MathCtrl::popid_copy_tex:
      if (m_console->CanCopy(true))
        m_console->CopyTeX();
      break;
    case MathCtrl::popid_copy_text:
      if (m_console->CanCopy(true))
        m_console->CopyText();
      break;
    case MathCtrl::popid_cut:
      if (m_console->CanCopy(true))
        m_console->CutToClipboard();
      break;
    case MathCtrl::popid_paste:
      m_console->PasteFromClipboard();
      break;
    case MathCtrl::popid_select_all:
    case ToolBar::tb_select_all:
      m_console->SelectAll();
      break;
    case MathCtrl::popid_comment_selection:
      m_console->CommentSelection();
      break;
    case MathCtrl::popid_divide_cell:
      m_console->DivideCell();
      break;
    case MathCtrl::popid_copy_image:
      if (m_console->CanCopy())
        m_console->CopyBitmap();
      break;
    case MathCtrl::popid_copy_animation:
      if (m_console->CanCopy())
        m_console->CopyAnimation();
      break;
    case MathCtrl::popid_copy_svg:
      if (m_console->CanCopy())
        m_console->CopySVG();
      break;
#if wxUSE_ENH_METAFILE
    case MathCtrl::popid_copy_emf:
      if (m_console->CanCopy())
        m_console->CopyEMF();
      break;
#endif
    case MathCtrl::popid_copy_rtf:
      if (m_console->CanCopy(true))
        m_console->CopyRTF();
      break;
    case MathCtrl::popid_simplify:
      MenuCommand(wxT("ratsimp(") + selection + wxT(");"));
      break;
    case MathCtrl::popid_expand:
      MenuCommand(wxT("expand(") + selection + wxT(");"));
      break;
    case MathCtrl::popid_factor:
      MenuCommand(wxT("factor(") + selection + wxT(");"));
      break;
    case MathCtrl::popid_solve:
    {
      Gen2Wiz *wiz = new Gen2Wiz(_("Equation(s):"), _("Variable(s):"),
                                 selection, wxT("x"),
                                 m_console->m_configuration,
                                 this, -1, _("Solve"), true,
                                 _("solve() will solve a list of equations only if for n "
                                   "independent equations there are n variables to solve to.\n"
                                   "If only one result variable is of interest the other result "
                                   "variables solve needs to do its work can be used to tell "
                                   "solve() which variables to eliminate in the solution "
                                   "for the interesting variable.")
        );
      //wiz->Centre(wxBOTH);
      wiz->SetLabel1ToolTip(_("Comma-separated equations"));
      wiz->SetLabel2ToolTip(_("Comma-separated variables"));
      if (wiz->ShowModal() == wxID_OK)
      {
        wxString cmd = wxT("solve([") + wiz->GetValue1() + wxT("], [") +
                       wiz->GetValue2() + wxT("]);");
        MenuCommand(cmd);
      }
      wiz->Destroy();
    }
      break;
    case MathCtrl::popid_solve_num:
    {
      Gen4Wiz *wiz = new Gen4Wiz(_("Equation:"), _("Variable:"),
                                 _("Lower bound:"), _("Upper bound:"),
                                 selection, wxT("x"), wxT("-1"), wxT("1"),
                                 m_console->m_configuration,
                                 this, -1, _("Find root"), true);
      //wiz->Centre(wxBOTH);
      if (wiz->ShowModal() == wxID_OK)
      {
        wxString cmd = wxT("find_root(") + wiz->GetValue1() + wxT(", ") +
                       wiz->GetValue2() + wxT(", ") +
                       wiz->GetValue3() + wxT(", ") +
                       wiz->GetValue4() + wxT(");");
        MenuCommand(cmd);
      }
      wiz->Destroy();
    }
      break;
    case MathCtrl::popid_integrate:
    {
      IntegrateWiz *wiz = new IntegrateWiz(this, -1, m_console->m_configuration, _("Integrate"));
      wiz->SetValue(selection);
      //wiz->Centre(wxBOTH);
      if (wiz->ShowModal() == wxID_OK)
      {
        wxString val = wiz->GetValue();
        MenuCommand(val);
      }
      wiz->Destroy();
    }
      break;
    case MathCtrl::popid_diff:
    {
      Gen3Wiz *wiz = new Gen3Wiz(_("Expression:"), _("Variable(s):"),
                                 _("Times:"), selection, wxT("x"), wxT("1"),
                                 m_console->m_configuration,
                                 this, -1, _("Differentiate"));
      wiz->SetValue(selection);
      //wiz->Centre(wxBOTH);
      if (wiz->ShowModal() == wxID_OK)
      {
        wxStringTokenizer vars(wiz->GetValue2(), wxT(","));
        wxStringTokenizer times(wiz->GetValue3(), wxT(","));

        wxString val = wxT("diff(") + wiz->GetValue1();

        while (vars.HasMoreTokens() && times.HasMoreTokens())
        {
          val += wxT(",") + vars.GetNextToken();
          val += wxT(",") + times.GetNextToken();
        }

        val += wxT(");");
        MenuCommand(val);
      }
      wiz->Destroy();
    }
      break;
    case MathCtrl::popid_subst:
    {
      SubstituteWiz *wiz = new SubstituteWiz(this, -1, m_console->m_configuration, _("Substitute"));
      wiz->SetValue(selection);
      //wiz->Centre(wxBOTH);
      if (wiz->ShowModal() == wxID_OK)
      {
        wxString val = wiz->GetValue();
        MenuCommand(val);
      }
      wiz->Destroy();
    }
      break;
    case MathCtrl::popid_plot2d:
    {
      Plot2DWiz *wiz = new Plot2DWiz(this, -1, m_console->m_configuration, _("Plot 2D"));
      wiz->SetValue(selection);
      //wiz->Centre(wxBOTH);
      if (wiz->ShowModal() == wxID_OK)
      {
        wxString val = wiz->GetValue();
        MenuCommand(val);
      }
      wiz->Destroy();
    }
      break;
    case MathCtrl::popid_plot3d:
    {
      Plot3DWiz *wiz = new Plot3DWiz(this, -1, m_console->m_configuration, _("Plot 3D"));
      wiz->SetValue(selection);
      //wiz->Centre(wxBOTH);
      if (wiz->ShowModal() == wxID_OK)
      {
        wxString val = wiz->GetValue();
        MenuCommand(val);
      }
      wiz->Destroy();
    }
      break;
    case MathCtrl::popid_float:
      MenuCommand(wxT("float(") + selection + wxT("), numer;"));
      break;
    case MathCtrl::popid_image:
    {
      wxString file = wxFileSelector(_("Save selection to file"), m_lastPath,
                                     wxT("image.png"), wxT("png"),
                                     _("PNG image (*.png)|*.png|"
                                               "JPEG image (*.jpg)|*.jpg|"
                                               "Windows bitmap (*.bmp)|*.bmp|"
                                               "Portable animap (*.pnm)|*.pnm|"
                                               "Tagged image file format (*.tif)|*.tif|"
                                               "X pixmap (*.xpm)|*.xpm"
                                     ),
                                     wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
      if (file.Length())
      {
        m_console->CopyToFile(file);
        m_lastPath = wxPathOnly(file);
      }
    }
      break;
    case MathCtrl::popid_animation_save:
    {
      wxString file = wxFileSelector(_("Save animation to file"), m_lastPath,
                                     wxT("animation.gif"), wxT("gif"),
                                     _("GIF image (*.gif)|*.gif"),
                                     wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
      if (file.Length())
      {
        MathCell *selection = m_console->GetSelectionStart();
        if (selection != NULL && selection->GetType() == MC_TYPE_SLIDE)
          dynamic_cast<SlideShow *>(selection)->ToGif(file);
      }
    }
      break;
    case MathCtrl::popid_merge_cells:
      m_console->MergeCells();
      break;
  }
}

void wxMaxima::OnRecentDocument(wxCommandEvent &event)
{
  if(m_console != NULL)
    m_console->CloseAutoCompletePopup();

  wxString file = m_recentDocuments.Get(event.GetId() - menu_recent_document_0);

  if (SaveNecessary() &&
      (
              (file.EndsWith(wxT(".wxmx"))) ||
              (file.EndsWith(wxT(".wxm")))
      )
          )
  {
    int close = SaveDocumentP();

    if (close == wxID_CANCEL)
      return;

    if (close == wxID_YES)
    {
      if (!SaveFile())
        return;
    }
  }

  if (wxFileExists(file))
    OpenFile(file);
  else
  {
    wxMessageBox(_("File you tried to open does not exist."), _("File not found"), wxOK);
  }
}

void wxMaxima::OnRecentPackage(wxCommandEvent &event)
{
  if(m_console != NULL)
    m_console->CloseAutoCompletePopup();

  wxString file = m_recentPackages.Get(event.GetId() - menu_recent_package_0);
  #ifdef __WXMSW__
  file.Replace(wxT("\\"),wxT("/"));
  #endif
  MenuCommand(wxT("load(\"") + file + wxT("\")$"));
}

void wxMaxima::OnUnsavedDocument(wxCommandEvent &event)
{
  if(m_console != NULL)
    m_console->CloseAutoCompletePopup();


  wxString file = m_unsavedDocuments.Get(event.GetId() - menu_unsaved_document_0);

  if(file == wxEmptyString)
    return;

  if (SaveNecessary() &&
      (
              (file.EndsWith(wxT(".wxmx"))) ||
              (file.EndsWith(wxT(".wxm")))
      )
          )
  {
    int close = SaveDocumentP();

    if (close == wxID_CANCEL)
      return;

    if (close == wxID_YES)
    {
      if (!SaveFile())
        return;
    }
  }

  if (wxFileExists(file))
  {
    OpenFile(file);
    m_fileSaved = false;
    m_isNamed   = false;
    m_tempfileName = file;
  }
  else
  {
    wxMessageBox(_("File you tried to open does not exist."), _("File not found"), wxOK);
  }
}

bool wxMaxima::SaveNecessary()
{
  // No need to save an empty document
  if(m_console->GetTree() == NULL)
    return false;

  // No need to save a document only consisting of an prompt
  if(m_console->GetTree()->Empty())
    return false;


  return ((!m_fileSaved) || (!m_isNamed));
}

void wxMaxima::EditInputMenu(wxCommandEvent &WXUNUSED(event))
{
  if(m_console != NULL)
    m_console->CloseAutoCompletePopup();

  if (!m_console->CanEdit())
    return;

  EditorCell *tmp = dynamic_cast<EditorCell *>(m_console->GetSelectionStart());

  if (tmp == NULL)
    return;

  m_console->SetActiveCell(tmp);
}

//! Handle the evaluation event
//
// User tried to evaluate, find out what is the case
// Normally just add the respective groupcells to evaluationqueue
// If there is a special case - eg sending from output section
// of the working group, handle it carefully.
void wxMaxima::EvaluateEvent(wxCommandEvent &WXUNUSED(event))
{
  if(m_console != NULL)
    m_console->CloseAutoCompletePopup();

  bool evaluating = !m_console->m_evaluationQueue.Empty();
  if (!evaluating)
    m_console->FollowEvaluation(true);
  EditorCell *tmp = m_console->GetActiveCell();
  if (m_console->QuestionPending())
    evaluating = true;

  if (tmp != NULL) // we have an active cell
  {
    if (tmp->GetType() == MC_TYPE_INPUT && !m_inLispMode)
      tmp->AddEnding();
    // if active cell is part of a working group, we have a special
    // case - answering 1a question. Manually send answer to Maxima.
    GroupCell *cell = dynamic_cast<GroupCell *>(tmp->GetGroup());
    if (m_console->GCContainsCurrentQuestion(cell))
    {
      wxString answer = tmp->ToString(true);
      // Add the answer to the current working cell or update the answer
      // that is stored within it.
      if(m_console->m_answersExhausted)
        cell->AddAnswer(answer);
      else
        m_console->UpdateAnswer(answer);
      SendMaxima(answer, true);
      StatusMaximaBusy(calculating);
      m_console->QuestionAnswered();
    }
    else
    { // normally just add to queue (and mark the cell as no more containing an error message)
      m_console->m_cellPointers.m_errorList.Remove(cell);
      m_console->AddCellToEvaluationQueue(cell);
    }
  }
  else
  { // no evaluate has been called on no active cell?
    m_console->AddSelectionToEvaluationQueue();
  }
  // Inform the user about the length of the evaluation queue.
  EvaluationQueueLength(m_console->m_evaluationQueue.Size(), m_console->m_evaluationQueue.CommandsLeftInCell());
  if (!evaluating)
    TryEvaluateNextInQueue();;
}

wxString wxMaxima::GetUnmatchedParenthesisState(wxString text,int &index)
{
  index = 0;

  std::list<wxChar> delimiters;

  if (text.Right(1) == wxT("\\"))
  {
    index = text.Length() - 1;
    return (_("Cell ends in a backslash"));
  }

  bool lisp = m_inLispMode;

  wxChar lastC = wxT(';');
  wxChar lastnonWhitespace = wxT(',');

  wxString::const_iterator it = text.begin();
  while (it < text.end())
  {
    wxChar c = *it;

    switch (c)
    {
      // Opening parenthesis
    case wxT('('):
      delimiters.push_back(wxT(')'));
      lastC = c;
      break;
    case wxT('['):
      delimiters.push_back(wxT(']'));
      lastC = c;
      break;
    case wxT('{'):
      delimiters.push_back(wxT('}'));
      lastC = c;
      break;

      // Closing parenthesis
    case wxT(')'):
    case wxT(']'):
    case wxT('}'):
      if (delimiters.empty()) return (_("Mismatched parenthesis"));
    if (c != delimiters.back()) return (_("Mismatched parenthesis"));
    delimiters.pop_back();
    lastC = c;
    if (lastnonWhitespace == wxT(','))
      return (_("Comma directly followed by a closing parenthesis"));
    break;

    // Escaped characters
    case wxT('\\'):
      ++it;++index;
      lastC = c;
      break;

      // Strings
    case wxT('\"'):
      ++it;++index;
      while ((it != text.end()) && (c = *it) != wxT('\"'))
      {
        if (c == wxT('\\'))
        {++it;++index;}

        if(it != text.end())
        {++it;++index;}
      }
      if ((it == text.end()) || (*it != wxT('\"'))) return (_("Unterminated string."));
      lastC = c;
      break;

    // a to_lisp command
    case wxT('t'):
    {
      // Extract 7 chars of the string.
      wxString command;
      wxString::const_iterator it2(it);
      if(it2 != text.end())
      {
        command += wxString(*it2);
        ++it2;
      }
      while((it2 != text.end()) && (wxIsalpha(*it2)))
      {
        command += wxString(*it2);
        ++it2;
      }
      if(command.StartsWith(wxT("to_lisp")))
         lisp = true;
      break;
    }

    // An eventual :lisp command
    case wxT(':'):
    {
      // Extract 5 chars of the string.
      wxString command;
      wxString::const_iterator it2(it);
      if(it2 != text.end())
      {
        command += wxString(*it2);
        ++it2;
      }
      while((it2 != text.end()) && (wxIsalpha(*it2)))
      {
        command += wxString(*it2);
        ++it2;
      }

      // Let's see if this is a :lisp-quiet or a :lisp
      if ((command == wxT(":lisp")) || (command == wxT(":lisp-quiet")))
        lisp = true;
      lastC = c;
      break;
    }
    case wxT(';'):
    case wxT('$'):
      if ((!lisp) && (!delimiters.empty()))
      {
        return _("Un-closed parenthesis on encountering ; or $");
      }
        lastC = c;
        break;

        // Comments
    case wxT('/'):
          if (it != text.end())
          {
            wxString::const_iterator it2(it);
            ++it2;
            if (*it2 == wxT('*'))
            {
              // Comment start. Let's search for the comment end.

              if (it != text.end())
              {++it;++index;}
              while(it != text.end())
              {
                wxChar last;
                last = *it;
                ++it;++index;

                // We reached the end of the string without finding a comment end.
                if(it == text.end())
                  return (_("Unterminated comment."));

                // A comment end.
                if((last == wxT('*')) && (*it == wxT('/')))
                  break;
              }
            }
            else lastC = c;
          }
          else lastC = c;
          break;

          default:
            if ((c != wxT('\n')) && (c != wxT(' ')) && (c != wxT('\t')))
              lastC = c;
    }

    if (
      (c != wxT(' ')) &&
      (c != wxT('\t')) &&
      (c != wxT('\n')) &&
      (c != wxT('\r'))
      )
      lastnonWhitespace = c;

    if(it < text.end())
    {
      ++it;++index;
    }
    }
    if (!delimiters.empty())
    {
      return _("Un-closed parenthesis");
    }

    if ((!lisp))
    {

      bool endingNeeded = true;
      text.Trim(true);
      text.Trim(false);

      // Cells ending in ";" or in "$" don't require us to add an ending.
      if (lastC == wxT(';'))
        endingNeeded = false;
      if (lastC == wxT('$'))
        endingNeeded = false;

      // Cells ending in "(to-maxima)" (with optional spaces around the "to-maxima")
      // don't require us to add an ending, neither.
      if(text.EndsWith(wxT(")")))
      {
        text = text.SubString(0,text.Length()-2);
        text.Trim();
        if (text.EndsWith(wxT("to-maxima")))
          endingNeeded = false;
      }

      if(endingNeeded)
        return _("No dollar ($) or semicolon (;) at the end of command");
    }
    return wxEmptyString;
}

//! Tries to evaluate next group cell in queue
//
// Calling this function should not do anything dangerous
void wxMaxima::TryEvaluateNextInQueue()
{
  // If we aren't connected yet this function will be triggered as soon as maxima
  // connects to wxMaxima
  if (!m_isConnected)
    return;

  // Maxima is connected. Let's test if the evaluation queue is empty.
  GroupCell *tmp = dynamic_cast<GroupCell *>(m_console->m_evaluationQueue.GetCell());
  if (tmp == NULL)
  {
    // Maxima is no more busy.
    StatusMaximaBusy(waiting);
    // Inform the user that the evaluation queue length now is 0.
    EvaluationQueueLength(0);
    // The cell from the last evaluation might still be shown in it's "evaluating" state
    // so let's refresh the console to update the display of this.
    m_console->RequestRedraw();

    // If the window isn't active we can inform the user that maxima in the meantime
    // has finished working.
    if(m_console->m_configuration->NotifyIfIdle())
      m_console->SetNotification(_("Maxima has finished calculating."));

    return; //empty queue
  }

  // Display the evaluation queue's status.
  EvaluationQueueLength(m_console->m_evaluationQueue.Size(), m_console->m_evaluationQueue.CommandsLeftInCell());

  // We don't want to evaluate a new cell if the user still has to answer
  // a question.
  if (m_console->QuestionPending())
    return;

  // Maxima is connected and the queue contains an item.

  // From now on we look every second if we got some output from a crashing
  // maxima: Is maxima is working correctly the stdout and stderr descriptors we
  // poll don't offer any data.
  ReadStdErr();
  m_maximaStdoutPollTimer.StartOnce(MAXIMAPOLLMSECS);

  if (m_console->m_evaluationQueue.m_workingGroupChanged)
  {
    // If the cell's output that we are about to remove contains the currently
    // selected cells we undo the selection.
    if (m_console->GetSelectionStart())
    {
      if (m_console->GetSelectionStart()->GetGroup() == tmp)
        m_console->SetSelection(NULL, NULL);
    }
    if (m_console->GetSelectionEnd())
    {
      if (m_console->GetSelectionEnd()->GetGroup() == tmp)
        m_console->SetSelection(NULL, NULL);
    }
    tmp->RemoveOutput();
    m_console->Recalculate(tmp);
    m_console->RequestRedraw();
  }

  wxString text = m_console->m_evaluationQueue.GetCommand();
  m_commandIndex = m_console->m_evaluationQueue.GetIndex();
  if ((text != wxEmptyString) && (text != wxT(";")) && (text != wxT("$")))
  {
    int index;
    wxString parenthesisError = GetUnmatchedParenthesisState(tmp->GetEditable()->ToString(true),index);
    if (parenthesisError == wxEmptyString)
    {
      if (m_console->FollowEvaluation())
      {
        m_console->SetSelection(tmp);
        if (!m_console->GetWorkingGroup())
        {
          m_console->SetHCaret(tmp);
          m_console->ScrollToCaret();
        }
      }

      m_console->m_cellPointers.SetWorkingGroup(tmp);
      tmp->GetPrompt()->SetValue(m_lastPrompt);
      // Clear the monitor that shows the xml representation of the output of the
      // current maxima command.
      if (m_xmlInspector)
        m_xmlInspector->Clear();

      SendMaxima(m_configCommands + text, true);
      m_configCommands = wxEmptyString;

      EvaluationQueueLength(m_console->m_evaluationQueue.Size(),
                            m_console->m_evaluationQueue.CommandsLeftInCell()
      );

      text.Trim(false);
      if (!m_hasEvaluatedCells)
      {
        if (text.StartsWith(wxT(":lisp")))
          SetStatusText(_("A \":lisp\" as the first command might fail to send a \"finished\" signal."));
      }

      // Mark the current maxima process as "no more in its initial condition".
      m_hasEvaluatedCells = true;
    }
    else
    {
      // Manually mark the current cell as the one that has caused an error.
      m_console->m_cellPointers.m_errorList.Add(tmp);
      tmp->GetEditable()->SetErrorIndex(m_commandIndex - 1);
      // Inform the user about the error (which automatically causes the worksheet
      // to the cell we marked as erroneous a few seconds ago.
      TextCell *cell = new TextCell(m_console->GetTree(), &(m_console->m_configuration),
                                    &m_console->m_cellPointers,
                                    _("Refusing to send cell to maxima: ") +
                                    parenthesisError + wxT("\n"));
      cell->SetType(MC_TYPE_ERROR);
      cell->SetGroup(tmp);
      tmp->SetOutput(cell);
      tmp->ResetSize();
      tmp->Recalculate();
      m_console->Recalculate();
      //m_console->RecalculateForce();
      tmp->GetInput()->SetCaretPosition(index);
      tmp->GetInput()->SetErrorIndex((m_commandIndex = index) - 1);


      if (m_console->FollowEvaluation())
        m_console->SetSelection(NULL);

      m_console->m_cellPointers.SetWorkingGroup(NULL);
      m_console->RequestRedraw();
      if(!AbortOnError())
      {
        m_console->m_evaluationQueue.RemoveFirst();
        m_outputCellsFromCurrentCommand = 0;
        TryEvaluateNextInQueue();
      }
      if((tmp)&&(tmp->GetEditable()))
        m_console->SetActiveCell(tmp->GetEditable());
      m_console->m_evaluationQueue.RemoveFirst();
    }
    m_console->Recalculate();
  }
  else
  {
    m_console->m_evaluationQueue.RemoveFirst();
    m_outputCellsFromCurrentCommand = 0;
    TryEvaluateNextInQueue();
  }
  m_console->m_answersExhausted = m_console->m_evaluationQueue.AnswersEmpty();

}

void wxMaxima::InsertMenu(wxCommandEvent &event)
{
  if(m_console != NULL)
    m_console->CloseAutoCompletePopup();

  int type = 0;
  bool output = false;
  switch (event.GetId())
  {
    case MathCtrl::popid_auto_answer:
      if((m_console->GetActiveCell() != NULL) &&
         (dynamic_cast<GroupCell *>(m_console->GetActiveCell()->GetGroup())->GetGroupType() == GC_TYPE_CODE))
        dynamic_cast<GroupCell *>(m_console->GetActiveCell()->GetGroup())->AutoAnswer(event.IsChecked());
      else if((m_console->GetSelectionStart() != NULL)&&
              (m_console->GetSelectionStart()->GetType() == MC_TYPE_GROUP))
      {
        GroupCell *gc = dynamic_cast<GroupCell *>(m_console->GetSelectionStart());
        while(gc != NULL)
        {
          if(gc->GetGroupType() == GC_TYPE_CODE)
            gc->AutoAnswer(event.IsChecked());

          if(gc == m_console->GetSelectionEnd())
            break;
          gc = dynamic_cast<GroupCell *>(gc->m_next);
        }
      }
      m_fileSaved = false;
      m_console->RequestRedraw();
      return;
      break;
    case menu_insert_previous_output:
      output = true;
    case MathCtrl::popid_insert_input:
    case menu_insert_input:
    case menu_insert_previous_input:
      type = GC_TYPE_CODE;
      break;
    case menu_autocomplete:
      m_console->Autocomplete();
      return;
      break;
    case menu_autocomplete_templates:
      m_console->Autocomplete(AutoComplete::tmplte);
      return;
      break;
    case menu_convert_to_code:
      if (m_console->GetActiveCell())
      {
        m_console->GetActiveCell()->GetGroup()->SetType(GC_TYPE_CODE);
        m_console->Recalculate(true);
        m_console->RequestRedraw();
      }
      break;
    case menu_convert_to_comment:
      if (m_console->GetActiveCell())
      {
        m_console->GetActiveCell()->GetGroup()->SetType(GC_TYPE_TEXT);
        m_console->Recalculate(true);
        m_console->RequestRedraw();
      }
      break;
    case menu_add_comment:
    case MathCtrl::popid_add_comment:
    case menu_format_text:
    case MathCtrl::popid_insert_text:
      type = GC_TYPE_TEXT;
      break;
    case menu_convert_to_title:
      if (m_console->GetActiveCell())
      {
        m_console->GetActiveCell()->GetGroup()->SetType(GC_TYPE_TITLE);
        m_console->Recalculate(true);
        m_console->RequestRedraw();
      }
      break;
    case menu_add_title:
    case menu_format_title:
    case MathCtrl::popid_insert_title:
      type = GC_TYPE_TITLE;
      break;
    case menu_convert_to_section:
      if (m_console->GetActiveCell())
      {
        m_console->GetActiveCell()->GetGroup()->SetType(GC_TYPE_SECTION);
        m_console->Recalculate(true);
        m_console->RequestRedraw();
      }
      break;
    case menu_add_section:
    case menu_format_section:
    case MathCtrl::popid_insert_section:
      type = GC_TYPE_SECTION;
      break;
    case menu_convert_to_subsection:
      if (m_console->GetActiveCell())
      {
        m_console->GetActiveCell()->GetGroup()->SetType(GC_TYPE_SUBSECTION);
        m_console->Recalculate(true);
        m_console->RequestRedraw();
      }
      break;
    case menu_add_subsection:
    case menu_format_subsection:
    case MathCtrl::popid_insert_subsection:
      type = GC_TYPE_SUBSECTION;
      break;
    case menu_convert_to_subsubsection:
      if (m_console->GetActiveCell())
      {
        m_console->GetActiveCell()->GetGroup()->SetType(GC_TYPE_SUBSUBSECTION);
        m_console->Recalculate(true);
        m_console->RequestRedraw();
      }
      break;
    case menu_add_subsubsection:
    case menu_format_subsubsection:
    case MathCtrl::popid_insert_subsubsection:
      type = GC_TYPE_SUBSUBSECTION;
      break;
    case menu_add_pagebreak:
    case menu_format_pagebreak:
      m_console->InsertGroupCells(
              new GroupCell(&(m_console->m_configuration), GC_TYPE_PAGEBREAK,
                            &m_console->m_cellPointers),
              m_console->GetHCaret());
      m_console->Recalculate();
      m_console->SetFocus();
      return;
      break;
    case menu_insert_image:
    case menu_format_image:
    {
      wxString file = wxFileSelector(_("Insert Image"), m_lastPath,
                                     wxEmptyString, wxEmptyString,
                                     _("Image files (*.png, *.jpg, *.bmp, *.xpm)|*.png;*.jpg;*.bmp;*.xpm"),
                                     wxFD_OPEN);
      if (file != wxEmptyString)
      {
        m_console->OpenHCaret(file, GC_TYPE_IMAGE);
      }
      m_console->SetFocus();
      return;
    }
      break;
    case menu_fold_all_cells:
      m_console->FoldAll();
      m_console->Recalculate(true);
      // send cursor to the top
      m_console->SetHCaret(NULL);
      break;
    case menu_unfold_all_cells:
      m_console->UnfoldAll();
      m_console->Recalculate(true);
      // refresh without moving cursor
      m_console->SetHCaret(m_console->GetHCaret());
      break;
  }

  m_console->SetFocus();

  if (event.GetId() == menu_insert_previous_input ||
      event.GetId() == menu_insert_previous_output)
  {
    wxString input;

    if (output == true)
      input = m_console->GetOutputAboveCaret();
    else
      input = m_console->GetInputAboveCaret();
    if (input != wxEmptyString)
      m_console->OpenHCaret(input, type);
  }
  else if (
    (event.GetId() == menu_unfold_all_cells) ||
    (event.GetId() == menu_fold_all_cells) ||
    (event.GetId() == menu_convert_to_subsubsection) ||
    (event.GetId() == menu_convert_to_subsection) ||
    (event.GetId() == menu_convert_to_section) ||
    (event.GetId() == menu_convert_to_comment) ||
    (event.GetId() == menu_convert_to_title) ||
    (event.GetId() == menu_convert_to_code)
    )
  {
    // don't do anything else
  }
  else
    m_console->OpenHCaret(wxEmptyString, type);
}

void wxMaxima::ResetTitle(bool saved, bool force)
{
  if(!m_isNamed)
  {
    SetRepresentedFilename(wxEmptyString);
    OSXSetModified(true);
    saved = false;
  }
  else
  {
    SetRepresentedFilename(m_console->m_currentFile);
    OSXSetModified((saved != m_fileSaved) || (force));
  }

  if ((saved != m_fileSaved) || (force))
  {
    m_fileSaved = saved;
    if (m_console->m_currentFile.Length() == 0)
    {
#ifndef __WXMAC__
      if (saved)
        SetTitle(wxString::Format(_("wxMaxima %s "), wxT(GITVERSION)) + _("[ unsaved ]"));
      else
        SetTitle(wxString::Format(_("wxMaxima %s "), wxT(GITVERSION)) + _("[ unsaved* ]"));
#endif
    }
    else
    {
      wxString name, ext;
      wxFileName::SplitPath(m_console->m_currentFile, NULL, NULL, &name, &ext);
#ifndef __WXMAC__
      if (m_fileSaved)
        SetTitle(wxString::Format(_("wxMaxima %s "), wxT(GITVERSION)) +
                 wxT(" [ ") + name + wxT(".") + ext + wxT(" ]"));
      else
        SetTitle(wxString::Format(_("wxMaxima %s "), wxT(GITVERSION)) +
                 wxT(" [ ") + name + wxT(".") + ext + wxT("* ]"));
#else
      SetTitle(name + wxT(".") + ext);
#endif
    }
#if defined __WXMAC__
#if defined __WXOSX_COCOA__
    OSXSetModified(!saved);
    if (m_console->m_currentFile != wxEmptyString)
      SetRepresentedFilename(m_console->m_currentFile);
#else
    WindowRef win = (WindowRef)MacGetTopLevelWindowRef();
    SetWindowModified(win,!saved);
    if (m_console->m_currentFile != wxEmptyString)
    {
      FSRef fsref;
      wxMacPathToFSRef(m_console->m_currentFile, &fsref);
      HIWindowSetProxyFSRef(win, &fsref);
    }
#endif
#endif
  }
}

///--------------------------------------------------------------------------------
///  Plot Slider
///--------------------------------------------------------------------------------

void wxMaxima::UpdateSlider(wxUpdateUIEvent &WXUNUSED(ev))
{
  if (m_console->m_mainToolBar)
  {
    if (m_console->m_mainToolBar->m_plotSlider)
    {
      if (m_console->IsSelected(MC_TYPE_SLIDE))
      {
        SlideShow *cell = dynamic_cast<SlideShow *>(m_console->GetSelectionStart());

        m_console->m_mainToolBar->UpdateSlider(cell);
      }
    }
  }
}

void wxMaxima::SliderEvent(wxScrollEvent &ev)
{
  SlideShow *slideShow = dynamic_cast<SlideShow *>(m_console->GetSelectionStart());

  if (slideShow != NULL)
  {
    slideShow->AnimationRunning(false);
    slideShow->SetDisplayedIndex(ev.GetPosition());

    wxRect rect = slideShow->GetRect();
    m_console->RequestRedraw(rect);
    if(m_console->m_mainToolBar)
      m_console->m_mainToolBar->UpdateSlider(slideShow);
  }
}

void wxMaxima::ShowPane(wxCommandEvent &ev)
{
  if(m_console != NULL)
    m_console->CloseAutoCompletePopup();

  int id = ev.GetId();

  if (id == menu_pane_hideall)
    wxMaximaFrame::ShowPane(static_cast<Event>(id), true);
  else
    wxMaximaFrame::ShowPane(static_cast<Event>(id),
                            !IsPaneDisplayed(static_cast<Event>(id)));

  if((id == menu_pane_structure) && (IsPaneDisplayed(static_cast<Event>(id))))
    m_console->UpdateTableOfContents();
}

void wxMaxima::OnChar(wxKeyEvent &event)
{
  if(m_console != NULL)
    m_console->OnChar(event);
  event.Skip();
}

void wxMaxima::OnKeyDown(wxKeyEvent &event)
{
  if(m_console != NULL)
    m_console->OnKeyDown(event);
  event.Skip();
}

void wxMaxima::NetworkDClick(wxCommandEvent &WXUNUSED(ev))
{
  m_manager.GetPane(wxT("XmlInspector")).Show(
          !m_manager.GetPane(wxT("XmlInspector")).IsShown()
  );
  m_manager.Update();
}

void wxMaxima::HistoryDClick(wxCommandEvent &ev)
{
  if(m_console != NULL)
    m_console->CloseAutoCompletePopup();

  m_console->OpenHCaret(ev.GetString(), GC_TYPE_CODE);
  m_console->SetFocus();
}

void wxMaxima::TableOfContentsSelection(wxListEvent &ev)
{
  GroupCell *selection = dynamic_cast<GroupCell *>(m_console->m_tableOfContents->GetCell(ev.GetIndex())->GetGroup());

  // We only update the table of contents when there is time => no guarantee that the
  // cell that was clicked at actually still is part of the tree.
  if ((m_console->GetTree()) && (m_console->GetTree()->Contains(selection)))
  {
    m_console->SetHCaret(selection);
    m_console->ScrollToCaret();
    m_console->SetFocus();
  }
}

void wxMaxima::OnFollow(wxCommandEvent &WXUNUSED(event))
{
  if(m_console != NULL)
    m_console->CloseAutoCompletePopup();

  m_console->OnFollow();
}

long *VersionToInt(wxString version)
{
  long *intV = new long[3];

  wxStringTokenizer tokens(version, wxT("."));

  for (int i = 0; i < 3 && tokens.HasMoreTokens(); i++)
    tokens.GetNextToken().ToLong(&intV[i]);

  return intV;
}

/***
 * Checks the file http://andrejv.github.io/wxmaxima/version.txt to
 * see if there is a newer version available.
 */
void wxMaxima::CheckForUpdates(bool reportUpToDate)
{
  wxHTTP connection;
  connection.SetHeader(wxT("Content-type"), wxT("text/html; charset=utf-8"));
  connection.SetTimeout(2);

  if (!connection.Connect(wxT("andrejv.github.io")))
  {
    wxMessageBox(_("Can not connect to the web server."), _("Error"),
                 wxOK | wxICON_ERROR);
    return;
  }

  wxInputStream *inputStream = connection.GetInputStream(_T("/wxmaxima/version.txt"));

  if (connection.GetError() == wxPROTO_NOERR)
  {
    wxString version;
    wxStringOutputStream outputStream(&version);
    inputStream->Read(outputStream);

    if (version.StartsWith(wxT("wxmaxima = ")))
    {
      version = version.Mid(11, version.Length()).Trim();
      long *myVersion = VersionToInt(wxT(GITVERSION));
      long *currVersion = VersionToInt(version);

      bool upgrade = myVersion[0] < currVersion[0] ||
                     (myVersion[0] == currVersion[0] && myVersion[1] < currVersion[1]) ||
                     (myVersion[0] == currVersion[0] &&
                      myVersion[1] == currVersion[1] &&
                      myVersion[2] < currVersion[2]);

      if (upgrade)
      {
        bool visit = wxMessageBox(wxString::Format(
                                          _("You have version %s. Current version is %s.\n\n"
                                                    "Select OK to visit the wxMaxima webpage."),
                                          wxT(GITVERSION), version.c_str()),
                                  _("Upgrade"),
                                  wxOK | wxCANCEL | wxICON_INFORMATION) == wxOK;

        if (visit)
          wxLaunchDefaultBrowser(wxT("https://andrejv.github.io/wxmaxima"));
      }
      else if (reportUpToDate)
        wxMessageBox(_("Your version of wxMaxima is up to date."), _("Upgrade"),
                     wxOK | wxICON_INFORMATION);

      delete[] myVersion;
      delete[] currVersion;
    }
    else
    {
      wxMessageBox(
              _("Unable to interpret the version info I got from http://andrejv.github.io//wxmaxima/version.txt: ") +
              version, _("Upgrade"),
              wxOK | wxICON_INFORMATION);

    }
  }
  else
  {
    wxMessageBox(_("Can not download version info."), _("Error"),
                 wxOK | wxICON_ERROR);
  }

  wxDELETE(inputStream);
  connection.Close();
}

int wxMaxima::SaveDocumentP()
{
  wxString file, ext;
  if ((m_console->m_currentFile == wxEmptyString) || (!m_isNamed))
  {
    // Check if we want to save modified untitled documents on exit
    bool save = true;
    wxConfig::Get()->Read(wxT("saveUntitled"), &save);
    if (!save)
      return wxID_NO;

#if defined __WXMAC__
    file = GetTitle();
#else
    file = _("unsaved");
#endif
  }
  else
  {
    if (m_console->m_configuration->AutoSaveInterval() > 10000)
      if (SaveFile())
        return wxID_NO;

    wxString ext;
    wxFileName::SplitPath(m_console->m_currentFile, NULL, NULL, &file, &ext);
    file += wxT(".") + ext;
  }

  wxMessageDialog dialog(this,
                         wxString::Format(_("Do you want to save the changes you made in the document \"%s\"?"),
                                          file),
                         "wxMaxima", wxCENTER | wxYES_NO | wxCANCEL);

  dialog.SetExtendedMessage(_("Your changes will be lost if you don't save them."));
  dialog.SetYesNoCancelLabels(_("Save"), _("Don't save"), _("Cancel"));

  return dialog.ShowModal();
}

void wxMaxima::OnActivate(wxActivateEvent &event)
{
  m_console->WindowActive(event.GetActive());
  event.Skip();
}

void wxMaxima::OnMinimize(wxIconizeEvent &event)
{
  m_console->WindowActive(!event.IsIconized());
  if(!event.IsIconized())
    m_console->SetFocus();
  event.Skip();
}

void wxMaxima::ChangeCellStyle(wxCommandEvent& WXUNUSED(event))
{
  if(m_console != NULL)
    m_console->CloseAutoCompletePopup();

  if ((m_console == NULL) || (m_console->m_mainToolBar == NULL))
    return;

  if(m_console->GetActiveCell())
  {
    GroupCell *group = dynamic_cast<GroupCell *>(m_console->GetActiveCell()->GetGroup());
    switch(group->GetStyle())
    {
    case GC_TYPE_CODE:
    case GC_TYPE_TEXT:
    case GC_TYPE_TITLE:
    case GC_TYPE_SECTION:
    case GC_TYPE_SUBSECTION:
    case GC_TYPE_SUBSUBSECTION:
      m_console->SetCellStyle(group, m_console->m_mainToolBar->GetCellStyle());
      break;
    default:
    {}
    }
    m_console->NumberSections();
    m_console->SetFocus();
  }
  else
    m_console->m_mainToolBar->SetDefaultCellStyle();
}

BEGIN_EVENT_TABLE(wxMaxima, wxFrame)

                EVT_MENU(mac_closeId, wxMaxima::FileMenu)
                EVT_MENU(menu_check_updates, wxMaxima::HelpMenu)
                EVT_TIMER(KEYBOARD_INACTIVITY_TIMER_ID, wxMaxima::OnTimerEvent)
                EVT_TIMER(MAXIMA_STDOUT_POLL_ID, wxMaxima::OnTimerEvent)
                EVT_TIMER(AUTO_SAVE_TIMER_ID, wxMaxima::OnTimerEvent)
                EVT_TIMER(wxID_ANY, wxMaxima::OnTimerEvent)
                EVT_COMMAND_SCROLL(ToolBar::plot_slider_id, wxMaxima::SliderEvent)
                EVT_MENU(MathCtrl::popid_copy, wxMaxima::PopupMenu)
                EVT_MENU(MathCtrl::popid_copy_image, wxMaxima::PopupMenu)
                EVT_MENU(MathCtrl::popid_copy_animation, wxMaxima::PopupMenu)
                EVT_MENU(MathCtrl::popid_copy_svg, wxMaxima::PopupMenu)
                EVT_MENU(MathCtrl::popid_copy_emf, wxMaxima::PopupMenu)
                EVT_MENU(MathCtrl::popid_copy_rtf, wxMaxima::PopupMenu)
                EVT_MENU(MathCtrl::popid_insert_text, wxMaxima::InsertMenu)
                EVT_MENU(MathCtrl::popid_insert_title, wxMaxima::InsertMenu)
                EVT_MENU(MathCtrl::popid_insert_section, wxMaxima::InsertMenu)
                EVT_MENU(MathCtrl::popid_insert_subsection, wxMaxima::InsertMenu)
                EVT_MENU(MathCtrl::popid_insert_subsubsection, wxMaxima::InsertMenu)
                EVT_MENU(MathCtrl::popid_popup_gnuplot, wxMaxima::EditMenu)
                EVT_MENU(MathCtrl::popid_delete, wxMaxima::EditMenu)
                EVT_MENU(MathCtrl::popid_simplify, wxMaxima::PopupMenu)
                EVT_MENU(MathCtrl::popid_factor, wxMaxima::PopupMenu)
                EVT_MENU(MathCtrl::popid_expand, wxMaxima::PopupMenu)
                EVT_MENU(MathCtrl::popid_solve, wxMaxima::PopupMenu)
                EVT_MENU(MathCtrl::popid_solve_num, wxMaxima::PopupMenu)
                EVT_MENU(MathCtrl::popid_subst, wxMaxima::PopupMenu)
                EVT_MENU(MathCtrl::popid_plot2d, wxMaxima::PopupMenu)
                EVT_MENU(MathCtrl::popid_plot3d, wxMaxima::PopupMenu)
                EVT_MENU(MathCtrl::popid_diff, wxMaxima::PopupMenu)
                EVT_MENU(MathCtrl::popid_integrate, wxMaxima::PopupMenu)
                EVT_MENU(MathCtrl::popid_float, wxMaxima::PopupMenu)
                EVT_MENU(MathCtrl::popid_copy_tex, wxMaxima::PopupMenu)
                EVT_MENU(MathCtrl::popid_copy_text, wxMaxima::PopupMenu)
                EVT_MENU(MathCtrl::popid_image, wxMaxima::PopupMenu)
                EVT_MENU(MathCtrl::popid_animation_save, wxMaxima::PopupMenu)
                EVT_MENU(MathCtrl::popid_animation_start, wxMaxima::FileMenu)
                EVT_BUTTON(button_integrate, wxMaxima::CalculusMenu)
                EVT_BUTTON(button_diff, wxMaxima::CalculusMenu)
                EVT_BUTTON(button_solve, wxMaxima::EquationsMenu)
                EVT_BUTTON(button_solve_ode, wxMaxima::EquationsMenu)
                EVT_BUTTON(button_sum, wxMaxima::CalculusMenu)
                EVT_BUTTON(button_expand, wxMaxima::SimplifyMenu)
                EVT_BUTTON(button_factor, wxMaxima::SimplifyMenu)
                EVT_BUTTON(button_taylor, wxMaxima::CalculusMenu)
                EVT_BUTTON(button_limit, wxMaxima::CalculusMenu)
                EVT_BUTTON(button_ratsimp, wxMaxima::SimplifyMenu)
                EVT_BUTTON(button_trigexpand, wxMaxima::SimplifyMenu)
                EVT_BUTTON(button_trigreduce, wxMaxima::SimplifyMenu)
                EVT_BUTTON(button_trigsimp, wxMaxima::SimplifyMenu)
                EVT_BUTTON(button_product, wxMaxima::CalculusMenu)
                EVT_BUTTON(button_radcan, wxMaxima::SimplifyMenu)
                EVT_BUTTON(button_subst, wxMaxima::MaximaMenu)
                EVT_BUTTON(button_plot2, wxMaxima::PlotMenu)
                EVT_BUTTON(button_plot3, wxMaxima::PlotMenu)
                EVT_BUTTON(button_map, wxMaxima::AlgebraMenu)
                EVT_BUTTON(button_rectform, wxMaxima::SimplifyMenu)
                EVT_BUTTON(button_trigrat, wxMaxima::SimplifyMenu)
                EVT_MENU(menu_polarform, wxMaxima::SimplifyMenu)
                EVT_MENU(ToolBar::menu_restart_id, wxMaxima::MaximaMenu)
#ifndef __WXMAC__
                EVT_MENU(wxID_EXIT, wxMaxima::FileMenu)
#endif
                EVT_MENU(wxID_ABOUT, wxMaxima::HelpMenu)
                EVT_MENU(menu_save_id, wxMaxima::FileMenu)
                EVT_MENU(menu_save_as_id, wxMaxima::FileMenu)
                EVT_MENU(menu_load_id, wxMaxima::FileMenu)
                EVT_MENU(menu_functions, wxMaxima::MaximaMenu)
                EVT_MENU(menu_variables, wxMaxima::MaximaMenu)
                EVT_MENU(wxID_PREFERENCES, wxMaxima::EditMenu)
                EVT_MENU(menu_sconsole_id, wxMaxima::FileMenu)
                EVT_MENU(menu_export_html, wxMaxima::FileMenu)
                EVT_MENU(wxID_HELP, wxMaxima::HelpMenu)
                EVT_MENU(menu_help_tutorials, wxMaxima::HelpMenu)
                EVT_MENU(menu_bug_report, wxMaxima::HelpMenu)
                EVT_MENU(menu_build_info, wxMaxima::HelpMenu)
                EVT_MENU(menu_interrupt_id, wxMaxima::Interrupt)
                EVT_MENU(menu_open_id, wxMaxima::FileMenu)
                EVT_MENU(menu_batch_id, wxMaxima::FileMenu)
                EVT_MENU(menu_ratsimp, wxMaxima::SimplifyMenu)
                EVT_MENU(menu_radsimp, wxMaxima::SimplifyMenu)
                EVT_MENU(menu_expand, wxMaxima::SimplifyMenu)
                EVT_MENU(menu_factor, wxMaxima::SimplifyMenu)
                EVT_MENU(menu_gfactor, wxMaxima::SimplifyMenu)
                EVT_MENU(menu_trigsimp, wxMaxima::SimplifyMenu)
                EVT_MENU(menu_trigexpand, wxMaxima::SimplifyMenu)
                EVT_MENU(menu_trigreduce, wxMaxima::SimplifyMenu)
                EVT_MENU(menu_rectform, wxMaxima::SimplifyMenu)
                EVT_MENU(menu_demoivre, wxMaxima::SimplifyMenu)
                EVT_MENU(menu_num_out, wxMaxima::NumericalMenu)
                EVT_MENU(menu_to_float, wxMaxima::NumericalMenu)
                EVT_MENU(menu_to_bfloat, wxMaxima::NumericalMenu)
                EVT_MENU(menu_to_numer, wxMaxima::NumericalMenu)
                EVT_MENU(menu_exponentialize, wxMaxima::SimplifyMenu)
                EVT_MENU(menu_invert_mat, wxMaxima::AlgebraMenu)
                EVT_MENU(menu_determinant, wxMaxima::AlgebraMenu)
                EVT_MENU(menu_eigen, wxMaxima::AlgebraMenu)
                EVT_MENU(menu_eigvect, wxMaxima::AlgebraMenu)
                EVT_MENU(menu_adjoint_mat, wxMaxima::AlgebraMenu)
                EVT_MENU(menu_transpose, wxMaxima::AlgebraMenu)
                EVT_MENU(menu_set_precision, wxMaxima::NumericalMenu)
                EVT_MENU(menu_set_displayprecision, wxMaxima::NumericalMenu)
                EVT_MENU(menu_engineeringFormat, wxMaxima::NumericalMenu)
                EVT_MENU(menu_engineeringFormatSetup, wxMaxima::NumericalMenu)
                EVT_MENU(menu_talg, wxMaxima::SimplifyMenu)
                EVT_MENU(menu_tellrat, wxMaxima::SimplifyMenu)
                EVT_MENU(menu_modulus, wxMaxima::SimplifyMenu)
                EVT_MENU(menu_allroots, wxMaxima::EquationsMenu)
                EVT_MENU(menu_bfallroots, wxMaxima::EquationsMenu)
                EVT_MENU(menu_realroots, wxMaxima::EquationsMenu)
                EVT_MENU(menu_solve, wxMaxima::EquationsMenu)
                EVT_MENU(menu_solve_to_poly, wxMaxima::EquationsMenu)
                EVT_MENU(menu_solve_num, wxMaxima::EquationsMenu)
                EVT_MENU(menu_solve_ode, wxMaxima::EquationsMenu)
                EVT_MENU(menu_map_mat, wxMaxima::AlgebraMenu)
                EVT_MENU(menu_enter_mat, wxMaxima::AlgebraMenu)
                EVT_MENU(menu_cpoly, wxMaxima::AlgebraMenu)
                EVT_MENU(menu_solve_lin, wxMaxima::EquationsMenu)
                EVT_MENU(menu_solve_algsys, wxMaxima::EquationsMenu)
                EVT_MENU(menu_eliminate, wxMaxima::EquationsMenu)
                EVT_MENU(menu_clear_var, wxMaxima::MaximaMenu)
                EVT_MENU(menu_clear_fun, wxMaxima::MaximaMenu)
                EVT_MENU(menu_ivp_1, wxMaxima::EquationsMenu)
                EVT_MENU(menu_ivp_2, wxMaxima::EquationsMenu)
                EVT_MENU(menu_bvp, wxMaxima::EquationsMenu)
                EVT_MENU(menu_bvp, wxMaxima::EquationsMenu)
                EVT_MENU(menu_fun_def, wxMaxima::MaximaMenu)
                EVT_MENU(menu_divide, wxMaxima::CalculusMenu)
                EVT_MENU(menu_gcd, wxMaxima::CalculusMenu)
                EVT_MENU(menu_lcm, wxMaxima::CalculusMenu)
                EVT_MENU(menu_continued_fraction, wxMaxima::CalculusMenu)
                EVT_MENU(menu_partfrac, wxMaxima::CalculusMenu)
                EVT_MENU(menu_risch, wxMaxima::CalculusMenu)
                EVT_MENU(menu_integrate, wxMaxima::CalculusMenu)
                EVT_MENU(menu_laplace, wxMaxima::CalculusMenu)
                EVT_MENU(menu_ilt, wxMaxima::CalculusMenu)
                EVT_MENU(menu_diff, wxMaxima::CalculusMenu)
                EVT_MENU(menu_series, wxMaxima::CalculusMenu)
                EVT_MENU(menu_limit, wxMaxima::CalculusMenu)
                EVT_MENU(menu_lbfgs, wxMaxima::CalculusMenu)
                EVT_MENU(menu_gen_mat, wxMaxima::AlgebraMenu)
                EVT_MENU(menu_gen_mat_lambda, wxMaxima::AlgebraMenu)
                EVT_MENU(menu_map, wxMaxima::AlgebraMenu)
                EVT_MENU(menu_sum, wxMaxima::CalculusMenu)
                EVT_MENU(menu_maximahelp, wxMaxima::HelpMenu)
                EVT_MENU(menu_example, wxMaxima::HelpMenu)
                EVT_MENU(menu_apropos, wxMaxima::HelpMenu)
                EVT_MENU(menu_show_tip, wxMaxima::HelpMenu)
                EVT_MENU(menu_trigrat, wxMaxima::SimplifyMenu)
                EVT_MENU(menu_solve_de, wxMaxima::EquationsMenu)
                EVT_MENU(menu_atvalue, wxMaxima::EquationsMenu)
                EVT_MENU(menu_lhs, wxMaxima::EquationsMenu)
                EVT_MENU(menu_rhs, wxMaxima::EquationsMenu)
                EVT_MENU(menu_sum, wxMaxima::CalculusMenu)
                EVT_MENU(menu_product, wxMaxima::CalculusMenu)
                EVT_MENU(menu_change_var, wxMaxima::CalculusMenu)
                EVT_MENU(menu_make_list, wxMaxima::AlgebraMenu)
                EVT_MENU(menu_apply, wxMaxima::AlgebraMenu)
                EVT_MENU(menu_time, wxMaxima::MaximaMenu)
                EVT_MENU(menu_factsimp, wxMaxima::SimplifyMenu)
                EVT_MENU(menu_factcomb, wxMaxima::SimplifyMenu)
                EVT_MENU(menu_realpart, wxMaxima::SimplifyMenu)
                EVT_MENU(menu_imagpart, wxMaxima::SimplifyMenu)
                EVT_MENU(menu_nouns, wxMaxima::SimplifyMenu)
                EVT_MENU(menu_logcontract, wxMaxima::SimplifyMenu)
                EVT_MENU(menu_logexpand, wxMaxima::SimplifyMenu)
                EVT_MENU(gp_plot2, wxMaxima::PlotMenu)
                EVT_MENU(gp_plot3, wxMaxima::PlotMenu)
                EVT_MENU(menu_animationautostart, wxMaxima::PlotMenu)
                EVT_MENU(menu_animationframerate, wxMaxima::PlotMenu)
                EVT_MENU(menu_plot_format, wxMaxima::PlotMenu)
                EVT_MENU(menu_soft_restart, wxMaxima::MaximaMenu)
                EVT_MENU(menu_jumptoerror, wxMaxima::MaximaMenu)
                EVT_MENU(menu_display, wxMaxima::MaximaMenu)
                EVT_MENU(menu_pade, wxMaxima::CalculusMenu)
                EVT_MENU(menu_add_path, wxMaxima::MaximaMenu)
                EVT_MENU(menu_copy_from_console, wxMaxima::EditMenu)
                EVT_MENU(menu_copy_text_from_console, wxMaxima::EditMenu)
                EVT_MENU(menu_copy_tex_from_console, wxMaxima::EditMenu)
                EVT_MENU(MathCtrl::popid_copy_mathml, wxMaxima::EditMenu)
                EVT_MENU(menu_undo, wxMaxima::EditMenu)
                EVT_MENU(menu_redo, wxMaxima::EditMenu)
                EVT_MENU(menu_texform, wxMaxima::MaximaMenu)
                EVT_MENU(menu_to_fact, wxMaxima::SimplifyMenu)
                EVT_MENU(menu_to_gamma, wxMaxima::SimplifyMenu)
                EVT_MENU(wxID_PRINT, wxMaxima::PrintMenu)
                EVT_TOOL(ToolBar::tb_print, wxMaxima::PrintMenu)
                EVT_MENU(MathCtrl::menu_zoom_in, wxMaxima::EditMenu)
                EVT_MENU(MathCtrl::menu_zoom_out, wxMaxima::EditMenu)
                EVT_MENU(menu_zoom_80, wxMaxima::EditMenu)
                EVT_MENU(menu_zoom_100, wxMaxima::EditMenu)
                EVT_MENU(menu_zoom_120, wxMaxima::EditMenu)
                EVT_MENU(menu_zoom_150, wxMaxima::EditMenu)
                EVT_MENU(menu_zoom_200, wxMaxima::EditMenu)
                EVT_MENU(menu_zoom_300, wxMaxima::EditMenu)
                EVT_MENU(menu_math_as_1D_ASCII, wxMaxima::EditMenu)
                EVT_MENU(menu_math_as_2D_ASCII, wxMaxima::EditMenu)
                EVT_MENU(menu_math_as_graphics, wxMaxima::EditMenu)
                EVT_MENU(menu_noAutosubscript, wxMaxima::EditMenu)
                EVT_MENU(menu_defaultAutosubscript, wxMaxima::EditMenu)
                EVT_MENU(menu_alwaysAutosubscript, wxMaxima::EditMenu)
                EVT_MENU(menu_roundedMatrixParensNo, wxMaxima::EditMenu)
                EVT_MENU(menu_roundedMatrixParensYes, wxMaxima::EditMenu)
                EVT_MENU(menu_fullscreen, wxMaxima::EditMenu)
                EVT_MENU(ToolBar::tb_hideCode, wxMaxima::EditMenu)
                EVT_MENU(menu_copy_as_bitmap, wxMaxima::EditMenu)
                EVT_MENU(menu_copy_as_svg, wxMaxima::EditMenu)
                EVT_MENU(menu_copy_as_emf, wxMaxima::EditMenu)
                EVT_MENU(menu_copy_as_rtf, wxMaxima::EditMenu)
                EVT_MENU(menu_copy_to_file, wxMaxima::EditMenu)
                EVT_MENU(menu_select_all, wxMaxima::EditMenu)
                EVT_MENU(menu_subst, wxMaxima::MaximaMenu)
                EVT_TOOL(ToolBar::tb_open, wxMaxima::FileMenu)
                EVT_TOOL(ToolBar::tb_save, wxMaxima::FileMenu)
                EVT_TOOL(ToolBar::tb_copy, wxMaxima::EditMenu)
                EVT_TOOL(ToolBar::tb_paste, wxMaxima::EditMenu)
                EVT_TOOL(ToolBar::tb_select_all, wxMaxima::PopupMenu)
                EVT_TOOL(ToolBar::tb_cut, wxMaxima::EditMenu)
                EVT_TOOL(ToolBar::tb_pref, wxMaxima::EditMenu)
                EVT_TOOL(ToolBar::tb_interrupt, wxMaxima::Interrupt)
                EVT_TOOL(ToolBar::tb_help, wxMaxima::HelpMenu)
                EVT_TOOL(ToolBar::tb_animation_startStop, wxMaxima::FileMenu)
                EVT_TOOL(ToolBar::tb_animation_start, wxMaxima::FileMenu)
                EVT_TOOL(ToolBar::tb_animation_stop, wxMaxima::FileMenu)
                EVT_TOOL(ToolBar::tb_find, wxMaxima::EditMenu)
                EVT_TOOL(ToolBar::tb_follow, wxMaxima::OnFollow)
                EVT_SOCKET(socket_server_id, wxMaxima::ServerEvent)
                EVT_SOCKET(socket_client_id, wxMaxima::ClientEvent)
/* These commands somehow caused the menu to be updated six times on every
   keypress and the tool bar to be updated six times on every menu update

   => Moved the update events to the idle loop.

EVT_UPDATE_UI(menu_interrupt_id, wxMaxima::UpdateMenus)
EVT_UPDATE_UI(ToolBar::plot_slider_id, wxMaxima::UpdateSlider)
EVT_UPDATE_UI(menu_copy_from_console, wxMaxima::UpdateMenus)
EVT_UPDATE_UI(menu_copy_text_from_console, wxMaxima::UpdateMenus)
EVT_UPDATE_UI(menu_copy_tex_from_console, wxMaxima::UpdateMenus)
EVT_UPDATE_UI(menu_copy_mathml_from_console, wxMaxima::UpdateMenus)
EVT_UPDATE_UI(MathCtrl::menu_zoom_in, wxMaxima::UpdateMenus)
EVT_UPDATE_UI(MathCtrl::menu_zoom_out, wxMaxima::UpdateMenus)
EVT_UPDATE_UI(wxID_PRINT, wxMaxima::UpdateMenus)
EVT_UPDATE_UI(menu_copy_as_bitmap, wxMaxima::UpdateMenus)
EVT_UPDATE_UI(menu_copy_as_svg, wxMaxima::UpdateMenus)
EVT_UPDATE_UI(menu_copy_to_file, wxMaxima::UpdateMenus)
EVT_UPDATE_UI(menu_evaluate, wxMaxima::UpdateMenus)
EVT_UPDATE_UI(menu_evaluate_all, wxMaxima::UpdateMenus)
EVT_UPDATE_UI(ToolBar::tb_evaltillhere, wxMaxima::UpdateMenus)
EVT_UPDATE_UI(menu_select_all, wxMaxima::UpdateMenus)
EVT_UPDATE_UI(menu_undo, wxMaxima::UpdateMenus)
EVT_UPDATE_UI(menu_pane_hideall, wxMaxima::UpdateMenus)
EVT_UPDATE_UI(menu_pane_math, wxMaxima::UpdateMenus)
EVT_UPDATE_UI(menu_pane_stats, wxMaxima::UpdateMenus)
EVT_UPDATE_UI(menu_pane_history, wxMaxima::UpdateMenus)
EVT_UPDATE_UI(menu_pane_structure, wxMaxima::UpdateMenus)
EVT_UPDATE_UI(menu_pane_format, wxMaxima::UpdateMenus)
EVT_UPDATE_UI(menu_remove_output, wxMaxima::UpdateMenus)
EVT_UPDATE_UI(ToolBar::tb_print, wxMaxima::UpdateToolBar)
EVT_UPDATE_UI(ToolBar::tb_follow, wxMaxima::UpdateToolBar)
EVT_UPDATE_UI(ToolBar::tb_copy, wxMaxima::UpdateToolBar)
EVT_UPDATE_UI(ToolBar::tb_cut, wxMaxima::UpdateToolBar)
EVT_UPDATE_UI(ToolBar::tb_interrupt, wxMaxima::UpdateToolBar)
EVT_UPDATE_UI(ToolBar::tb_save, wxMaxima::UpdateToolBar)
EVT_UPDATE_UI(ToolBar::tb_animation_startStop, wxMaxima::UpdateToolBar)
EVT_UPDATE_UI(ToolBar::tb_animation_start, wxMaxima::UpdateToolBar)
EVT_UPDATE_UI(ToolBar::tb_animation_stop, wxMaxima::UpdateToolBar)
EVT_UPDATE_UI(menu_save_id, wxMaxima::UpdateMenus)
EVT_UPDATE_UI(menu_show_toolbar, wxMaxima::UpdateMenus)
*/
                EVT_CLOSE(wxMaxima::OnClose)
                EVT_END_PROCESS(maxima_process_id, wxMaxima::OnProcessEvent)
                EVT_END_PROCESS(gnuplot_process_id, wxMaxima::OnGnuplotClose)
                EVT_MENU(MathCtrl::popid_edit, wxMaxima::EditInputMenu)
                EVT_MENU(menu_evaluate, wxMaxima::EvaluateEvent)
                EVT_MENU(menu_add_comment, wxMaxima::InsertMenu)
                EVT_MENU(menu_add_section, wxMaxima::InsertMenu)
                EVT_MENU(menu_add_subsection, wxMaxima::InsertMenu)
                EVT_MENU(menu_add_subsubsection, wxMaxima::InsertMenu)
                EVT_MENU(menu_add_title, wxMaxima::InsertMenu)
                EVT_MENU(menu_add_pagebreak, wxMaxima::InsertMenu)
                EVT_MENU(menu_fold_all_cells, wxMaxima::InsertMenu)
                EVT_MENU(menu_unfold_all_cells, wxMaxima::InsertMenu)
                EVT_MENU(MathCtrl::popid_add_comment, wxMaxima::InsertMenu)
                EVT_MENU(menu_insert_previous_input, wxMaxima::InsertMenu)
                EVT_MENU(menu_insert_previous_output, wxMaxima::InsertMenu)
                EVT_MENU(menu_autocomplete, wxMaxima::InsertMenu)
                EVT_MENU(menu_autocomplete_templates, wxMaxima::InsertMenu)
                EVT_MENU(menu_insert_input, wxMaxima::InsertMenu)
                EVT_MENU(MathCtrl::popid_insert_input, wxMaxima::InsertMenu)
                EVT_MENU(menu_history_previous, wxMaxima::EditMenu)
                EVT_MENU(menu_history_next, wxMaxima::EditMenu)
                EVT_MENU(menu_cut, wxMaxima::EditMenu)
                EVT_MENU(menu_paste, wxMaxima::EditMenu)
                EVT_MENU(menu_paste_input, wxMaxima::EditMenu)
                EVT_MENU(MathCtrl::popid_cut, wxMaxima::PopupMenu)
                EVT_MENU(MathCtrl::popid_paste, wxMaxima::PopupMenu)
                EVT_MENU(MathCtrl::popid_select_all, wxMaxima::PopupMenu)
                EVT_MENU(MathCtrl::popid_comment_selection, wxMaxima::PopupMenu)
                EVT_MENU(MathCtrl::popid_divide_cell, wxMaxima::PopupMenu)
                EVT_MENU(MathCtrl::popid_evaluate, wxMaxima::PopupMenu)
                EVT_MENU(MathCtrl::popid_evaluate_section, wxMaxima::PopupMenu)
                EVT_MENU(ToolBar::tb_evaluate_rest, wxMaxima::PopupMenu)
                EVT_MENU(ToolBar::tb_evaltillhere, wxMaxima::PopupMenu)
                EVT_MENU(MathCtrl::popid_merge_cells, wxMaxima::PopupMenu)
                EVT_MENU(MathCtrl::popid_maxsizechooser, wxMaxima::PopupMenu)
                EVT_MENU(TableOfContents::popid_Fold, wxMaxima::PopupMenu)
                EVT_MENU(TableOfContents::popid_Unfold, wxMaxima::PopupMenu)
                EVT_MENU(TableOfContents::popid_SelectTocChapter, wxMaxima::PopupMenu)
                EVT_MENU(TableOfContents::popid_EvalTocChapter, wxMaxima::PopupMenu)
                EVT_MENU(TableOfContents::popid_ToggleTOCshowsSectionNumbers, wxMaxima::PopupMenu)
                EVT_MENU(MathCtrl::popid_fold, wxMaxima::PopupMenu)
                EVT_MENU(MathCtrl::popid_unfold, wxMaxima::PopupMenu)
                EVT_MENU(menu_evaluate_all_visible, wxMaxima::MaximaMenu)
                EVT_MENU(menu_evaluate_all, wxMaxima::MaximaMenu)
                EVT_MENU(ToolBar::tb_evaltillhere, wxMaxima::MaximaMenu)
                EVT_MENU(menu_list_create_from_elements,wxMaxima::ListMenu)
                EVT_MENU(menu_list_create_from_rule,wxMaxima::ListMenu)
                EVT_MENU(menu_list_create_from_list,wxMaxima::ListMenu)
                EVT_MENU(menu_list_actual_values_storage,wxMaxima::ListMenu)
                EVT_MENU(menu_list_sort,wxMaxima::ListMenu)
                EVT_MENU(menu_list_length,wxMaxima::ListMenu)
                EVT_MENU(menu_list_push,wxMaxima::ListMenu)
                EVT_MENU(menu_list_pop,wxMaxima::ListMenu)
                EVT_MENU(menu_list_reverse,wxMaxima::ListMenu)
                EVT_MENU(menu_list_first,wxMaxima::ListMenu)
                EVT_MENU(menu_list_last,wxMaxima::ListMenu)
                EVT_MENU(menu_list_rest,wxMaxima::ListMenu)
                EVT_MENU(menu_list_restN,wxMaxima::ListMenu)
                EVT_MENU(menu_list_lastn,wxMaxima::ListMenu)
                EVT_MENU(menu_list_nth,wxMaxima::ListMenu)
                EVT_MENU(menu_list_map,wxMaxima::ListMenu)
                EVT_MENU(menu_list_use_actual_values,wxMaxima::ListMenu)
                EVT_MENU(menu_list_as_function_arguments,wxMaxima::ListMenu)
                EVT_MENU(menu_list_extract_value,wxMaxima::ListMenu)
                EVT_MENU(menu_list_do_for_each_element,wxMaxima::ListMenu)
                EVT_MENU(menu_list_remove_duplicates,wxMaxima::ListMenu)
                EVT_MENU(menu_list_remove_element,wxMaxima::ListMenu)
                EVT_MENU(menu_list_append_item,wxMaxima::ListMenu)
                EVT_MENU(menu_list_append_list,wxMaxima::ListMenu)
                EVT_MENU(menu_list_interleave,wxMaxima::ListMenu)
                EVT_MENU(menu_list_list2matrix,wxMaxima::ListMenu)
                EVT_MENU(menu_list_matrix2list,wxMaxima::ListMenu)
                EVT_MENU(menu_list_create_from_args,wxMaxima::ListMenu)
                EVT_MENU(menu_draw_2d,wxMaxima::DrawMenu)
                EVT_BUTTON(menu_draw_2d,wxMaxima::DrawMenu)
                EVT_MENU(menu_draw_3d,wxMaxima::DrawMenu)
                EVT_BUTTON(menu_draw_3d,wxMaxima::DrawMenu)
                EVT_MENU(menu_draw_fgcolor,wxMaxima::DrawMenu)
                EVT_BUTTON(menu_draw_fgcolor,wxMaxima::DrawMenu)
                EVT_MENU(menu_draw_fillcolor,wxMaxima::DrawMenu)
                EVT_BUTTON(menu_draw_fillcolor,wxMaxima::DrawMenu)
                EVT_MENU(menu_draw_title,wxMaxima::DrawMenu)
                EVT_BUTTON(menu_draw_title,wxMaxima::DrawMenu)
                EVT_MENU(menu_draw_key,wxMaxima::DrawMenu)
                EVT_BUTTON(menu_draw_key,wxMaxima::DrawMenu)
                EVT_MENU(menu_draw_explicit,wxMaxima::DrawMenu)
                EVT_BUTTON(menu_draw_explicit,wxMaxima::DrawMenu)
                EVT_MENU(menu_draw_implicit,wxMaxima::DrawMenu)
                EVT_BUTTON(menu_draw_implicit,wxMaxima::DrawMenu)
                EVT_MENU(menu_draw_parametric,wxMaxima::DrawMenu)
                EVT_BUTTON(menu_draw_parametric,wxMaxima::DrawMenu)
                EVT_MENU(menu_draw_points,wxMaxima::DrawMenu)
                EVT_BUTTON(menu_draw_points,wxMaxima::DrawMenu)
                EVT_MENU(menu_draw_axis,wxMaxima::DrawMenu)
                EVT_BUTTON(menu_draw_axis,wxMaxima::DrawMenu)
                EVT_MENU(menu_draw_contour,wxMaxima::DrawMenu)
                EVT_BUTTON(menu_draw_contour,wxMaxima::DrawMenu)
                EVT_MENU(menu_draw_accuracy,wxMaxima::DrawMenu)
                EVT_BUTTON(menu_draw_accuracy,wxMaxima::DrawMenu)
                EVT_MENU(menu_draw_grid,wxMaxima::DrawMenu)
                EVT_BUTTON(menu_draw_grid,wxMaxima::DrawMenu)
                EVT_IDLE(wxMaxima::OnIdle)
                EVT_MENU(menu_remove_output, wxMaxima::EditMenu)
                EVT_MENU_RANGE(menu_recent_document_0, menu_recent_document_29, wxMaxima::OnRecentDocument)
                EVT_MENU_RANGE(menu_recent_package_0, menu_recent_package_29, wxMaxima::OnRecentPackage)
                EVT_MENU_RANGE(menu_unsaved_document_0, menu_unsaved_document_29, wxMaxima::OnUnsavedDocument)
                EVT_MENU(menu_insert_image, wxMaxima::InsertMenu)
                EVT_MENU_RANGE(menu_pane_hideall, menu_pane_stats, wxMaxima::ShowPane)
                EVT_MENU(menu_show_toolbar, wxMaxima::EditMenu)
                EVT_MENU(MathCtrl::popid_auto_answer, wxMaxima::InsertMenu)
                EVT_LISTBOX_DCLICK(history_ctrl_id, wxMaxima::HistoryDClick)
                EVT_LIST_ITEM_ACTIVATED(structure_ctrl_id, wxMaxima::TableOfContentsSelection)
                EVT_BUTTON(menu_stats_histogram, wxMaxima::StatsMenu)
                EVT_BUTTON(menu_stats_piechart, wxMaxima::StatsMenu)
                EVT_BUTTON(menu_stats_scatterplot, wxMaxima::StatsMenu)
                EVT_BUTTON(menu_stats_barsplot, wxMaxima::StatsMenu)
                EVT_BUTTON(menu_stats_boxplot, wxMaxima::StatsMenu)
                EVT_BUTTON(menu_stats_mean, wxMaxima::StatsMenu)
                EVT_BUTTON(menu_stats_median, wxMaxima::StatsMenu)
                EVT_BUTTON(menu_stats_var, wxMaxima::StatsMenu)
                EVT_BUTTON(menu_stats_dev, wxMaxima::StatsMenu)
                EVT_BUTTON(menu_stats_tt1, wxMaxima::StatsMenu)
                EVT_BUTTON(menu_stats_tt2, wxMaxima::StatsMenu)
                EVT_BUTTON(menu_stats_tnorm, wxMaxima::StatsMenu)
                EVT_BUTTON(menu_stats_linreg, wxMaxima::StatsMenu)
                EVT_BUTTON(menu_stats_lsquares, wxMaxima::StatsMenu)
                EVT_BUTTON(menu_stats_readm, wxMaxima::StatsMenu)
                EVT_BUTTON(menu_stats_enterm, wxMaxima::AlgebraMenu)
                EVT_BUTTON(menu_stats_subsample, wxMaxima::StatsMenu)
                EVT_BUTTON(menu_format_title, wxMaxima::InsertMenu)
                EVT_BUTTON(menu_format_text, wxMaxima::InsertMenu)
                EVT_BUTTON(menu_format_subsubsection, wxMaxima::InsertMenu)
                EVT_BUTTON(menu_format_subsection, wxMaxima::InsertMenu)
                EVT_BUTTON(menu_format_section, wxMaxima::InsertMenu)
                EVT_BUTTON(menu_format_pagebreak, wxMaxima::InsertMenu)
                EVT_BUTTON(menu_format_image, wxMaxima::InsertMenu)
                EVT_CHAR(wxMaxima::OnChar)
                EVT_KEY_DOWN(wxMaxima::OnKeyDown)
                EVT_CHOICE(ToolBar::tb_changeStyle, wxMaxima::ChangeCellStyle)
                EVT_MENU(menu_edit_find, wxMaxima::EditMenu)
                EVT_FIND(wxID_ANY, wxMaxima::OnFind)
                EVT_FIND_NEXT(wxID_ANY, wxMaxima::OnFind)
                EVT_FIND_REPLACE(wxID_ANY, wxMaxima::OnReplace)
                EVT_FIND_REPLACE_ALL(wxID_ANY, wxMaxima::OnReplaceAll)
                EVT_FIND_CLOSE(wxID_ANY, wxMaxima::OnFindClose)
                EVT_ACTIVATE(wxMaxima::OnActivate)
                EVT_ICONIZE(wxMaxima::OnMinimize)
END_EVENT_TABLE()


/* Local Variables:       */
/* mode: text             */
/* c-file-style:  "linux" */
/* c-basic-offset: 2      */
/* indent-tabs-mode: nil  */
