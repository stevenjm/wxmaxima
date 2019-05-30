// -*- mode: c++; c-file-style: "linux"; c-basic-offset: 2; indent-tabs-mode: nil -*-
//
//  Copyright (C) 2018 Gunter Königsmann <wxMaxima@physikbuch.de>
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

#include "VariablesPane.h"

Variablespane::Variablespane(wxWindow *parent, wxWindowID id) : wxGrid(parent, id)
{
  SetMinSize(wxSize(wxSystemSettings::GetMetric ( wxSYS_SCREEN_X )/10,
                    wxSystemSettings::GetMetric ( wxSYS_SCREEN_Y )/10));
  CreateGrid(1,2);
  wxGridCellAttr *attr0, *attr1;
  attr0 = new wxGridCellAttr;
  attr0->SetRenderer(new wxGridCellAutoWrapStringRenderer);
  SetColAttr(0,attr0);
  SetColLabelValue(0,_("Variable"));
  attr1 = new wxGridCellAttr;
  attr1->SetReadOnly();
  attr1->SetRenderer(new wxGridCellAutoWrapStringRenderer);
  SetColAttr(1,attr1);
  SetColLabelValue(1,_("Contents"));
  Connect(wxEVT_GRID_CELL_CHANGED,
          wxGridEventHandler(Variablespane::OnTextChange),
          NULL, this);
  HideRowLabels();
}

void Variablespane::OnTextChange(wxGridEvent &event)
{
  if((GetNumberRows() == 0) || (GetCellValue(GetNumberRows()-1,0) != wxEmptyString))
    AppendRows();
  else
    for(int i = 0; i < GetNumberRows() - 1; i++)
      if(GetCellValue(i,0) == wxEmptyString)
        DeleteRows(i);

  //while((GetNumberRows() > 1) && (GetCellValue(GetNumberRows()-2,0) == wxEmptyString))
  //    DeleteRows(GetNumberRows()-1);
}

Variablespane::~Variablespane()
{
}
