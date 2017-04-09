// -*- mode: c++; c-file-style: "linux"; c-basic-offset: 2; indent-tabs-mode: nil -*-
//
//  Copyright (C) 2004-2015 Andrej Vodopivec <andrej.vodopivec@gmail.com>
//            (C) 2008-2009 Ziga Lenarcic <zigalenarcic@users.sourceforge.net>
//            (C) 2011-2011 cw.ahbong <cw.ahbong@gmail.com>
//            (C) 2012-2013 Doug Ilijev <doug.ilijev@gmail.com>
//            (C) 2014-2016 Gunter Königsmann <wxMaxima@physikbuch.de>
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

/*!\file
  This file defines the contents of the class wxMaxima that contains most of the program's logic. 

  The worksheet is defined in the class MathCtrl instead and 
  everything surrounding it in wxMaximaFrame.
 */

#include "RibbonBar.h"
#include "ToolBar.h"

RibbonBar::RibbonBar(wxWindow *parent, int id):wxRibbonBar(
  parent,id
  // ,wxDefaultPosition, wxDefaultSize,
  // wxRIBBON_BAR_FLOW_HORIZONTAL
  // | wxRIBBON_BAR_SHOW_PAGE_LABELS
  // | wxRIBBON_BAR_SHOW_PANEL_EXT_BUTTONS
  // | wxRIBBON_BAR_SHOW_TOGGLE_BUTTON
  // | wxRIBBON_BAR_SHOW_HELP_BUTTON
  )
{
  wxRibbonPage* home = new wxRibbonPage(this, wxID_ANY, _("Home"));
  
  wxRibbonPanel *list_panel = new wxRibbonPanel(home, wxID_ANY, _("List"));
  wxRibbonButtonBar *listitems = new wxRibbonButtonBar(list_panel);
  wxBitmap nullBitmap = wxBitmap(2,2);
  listitems->AddButton(ToolBar::tb_find, _("create"),nullBitmap);
  listitems->AddButton(ToolBar::tb_find, _("first"),nullBitmap);
  listitems->AddButton(ToolBar::tb_find, _("last"),nullBitmap);
  listitems->AddButton(ToolBar::tb_find, _("nth"),nullBitmap);
  listitems->AddButton(ToolBar::tb_find, _("append"),nullBitmap);
  listitems->AddButton(ToolBar::tb_find, _("sort"),nullBitmap);

  wxRibbonPanel *maxima_panel = new wxRibbonPanel(home, wxID_ANY, _("Maxima"));
  wxRibbonToolBar *maximaBar = new wxRibbonToolBar(maxima_panel);
  maximaBar->AddTool(ToolBar::menu_restart_id,
                     ToolBar::GetImage(wxT("view-refresh")),
                     _("Completely stop maxima and restart it"));
  maximaBar->AddTool(ToolBar::tb_interrupt,
                     ToolBar::GetImage(wxT("gtk-stop")),
                     _("Interrupt current computation. To completely restart maxima press the button left to this one."));
  maximaBar->AddSeparator();
  maximaBar->AddTool(ToolBar::tb_evaltillhere, 
                     ToolBar::GetImage(wxT("go-bottom")),
                     _("Evaluate the file from its beginning to the cell above the cursor"));
  maximaBar->AddSeparator();
  m_followIcon = ToolBar::GetImage(wxT("weather-clear"));
  m_needsInformationIcon = ToolBar::GetImage(wxT("software-update-urgent"));
  maximaBar->AddTool(ToolBar::tb_follow, m_followIcon,
                     _("Return to the cell that is currently being evaluated"));
  maximaBar->EnableTool(ToolBar::tb_follow, false);

  maximaBar->AddTool(ToolBar::tb_hideCode,
                     ToolBar::GetImage(wxT("weather-few-clouds")),
                     _("Toggle the visibility of code cells"));
  maximaBar->SetRows(2,3);
  wxRibbonPanel *edit_panel = new wxRibbonPanel(home, wxID_ANY, _("Edit"));
  wxRibbonToolBar *editBar = new wxRibbonToolBar(edit_panel);
  editBar->AddTool(ToolBar::tb_cut,
                   ToolBar::GetImage(wxT("gtk-cut")),
                   _("Cut selection"));
  editBar->AddTool(ToolBar::tb_copy,
                   ToolBar::GetImage(wxT("gtk-copy")),
                   _("Copy selection"));
  editBar->AddTool(ToolBar::tb_paste, 
                   ToolBar::GetImage(wxT("gtk-paste")),
                   _("Paste from clipboard"));
  editBar->AddSeparator();
  editBar->AddTool(ToolBar::tb_select_all,
                   ToolBar::GetImage(wxT("gtk-select-all")),
                   _("Select all"));
  editBar->AddTool(ToolBar::tb_find,
                   ToolBar::GetImage(wxT("gtk-find")),
                   _("Find and replace"));
  editBar->AddSeparator();
  editBar->SetRows(2,3);
  Realize();
}

