// -*- mode: c++; c-file-style: "linux"; c-basic-offset: 2; indent-tabs-mode: nil -*-
//
//  Copyright (C) 2007-2015 Andrej Vodopivec <andrej.vodopivec@gmail.com>
//            (C) 2014-2015 Gunter Königsmann <wxMaxima@physikbuch.de>
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

#include "SubSupCell.h"
#include <wx/config.h>
#include "wx/config.h"

#define SUBSUP_DEC 3

SubSupCell::SubSupCell() : MathCell()
{
  m_baseCell = NULL;
  m_indexCell = NULL;
  m_exptCell = NULL;
  m_presupCell = NULL;
  m_presupCell = NULL;

}
SubSupCell::~SubSupCell()
{
  if (m_baseCell != NULL)
    delete m_baseCell;
  if (m_indexCell != NULL)
    delete m_indexCell;
  if (m_exptCell != NULL)
    delete m_exptCell;
  if (m_next != NULL)
    delete m_next;
  if (m_presupCell != NULL)
    delete m_presupCell;
  if (m_presubCell != NULL)
    delete m_presubCell;
}

void SubSupCell::SetParent(MathCell *parent)
{
  m_group = parent;
  if (m_baseCell != NULL)
    m_baseCell->SetParentList(parent);
  if (m_indexCell != NULL)
    m_indexCell->SetParentList(parent);
  if (m_exptCell != NULL)
    m_exptCell->SetParentList(parent);
  if (m_presupCell != NULL)
    m_presupCell->SetParentList(parent);
  if (m_presubCell != NULL)
    m_presubCell->SetParentList(parent);
}

MathCell* SubSupCell::Copy()
{
  SubSupCell* tmp = new SubSupCell;
  CopyData(this, tmp);
  tmp->SetBase(m_baseCell->CopyList());
  tmp->SetIndex(m_indexCell->CopyList());
  tmp->SetExponent(m_exptCell->CopyList());
  if(m_presubCell != NULL)
    tmp->SetPreSub(m_presubCell->CopyList());
  if(m_presupCell != NULL)
    tmp->SetPreSup(m_presupCell->CopyList());

  return tmp;
}

void SubSupCell::Destroy()
{
  if (m_baseCell != NULL)
    delete m_baseCell;
  if (m_indexCell != NULL)
    delete m_indexCell;
  if (m_exptCell != NULL)
    delete m_exptCell;
  if (m_presupCell != NULL)
    delete m_presupCell;
  if (m_presubCell != NULL)
    delete m_presubCell;
  m_baseCell = NULL;
  m_indexCell = NULL;
  m_exptCell = NULL;
  m_presupCell = NULL;
  m_presubCell = NULL;
  m_next = NULL;
}

void SubSupCell::SetIndex(MathCell *index)
{
  if (index == NULL)
    return ;
  if (m_indexCell != NULL)
    delete m_indexCell;
  m_indexCell = index;
}

void SubSupCell::SetBase(MathCell *base)
{
  if (base == NULL)
    return ;
  if (m_baseCell != NULL)
    delete m_baseCell;
  m_baseCell = base;
}

void SubSupCell::SetExponent(MathCell *exp)
{
  if (exp == NULL)
    return ;
  if (m_exptCell != NULL)
    delete m_exptCell;
  m_exptCell = exp;
}

void SubSupCell::SetPreSup(MathCell *exp)
{
  if (exp == NULL)
    return ;
  if (m_presupCell != NULL)
    delete m_presupCell;
  m_presupCell = exp;
}

void SubSupCell::SetPreSub(MathCell *exp)
{
  if (exp == NULL)
    return ;
  if (m_presubCell != NULL)
    delete m_presubCell;
  m_presubCell = exp;
}

void SubSupCell::RecalculateWidths(CellParser& parser, int fontsize)
{
  double scale = parser.GetScale();
  m_baseCell->RecalculateWidthsList(parser, fontsize);
  m_indexCell->RecalculateWidthsList(parser, MAX(MC_MIN_SIZE, fontsize - SUBSUP_DEC));
  m_exptCell->RecalculateWidthsList(parser, MAX(MC_MIN_SIZE, fontsize - SUBSUP_DEC));

  int mpresupcellWidth = 0;    
  int mpresubcellWidth = 0;
  if(m_presubCell != NULL)
  {
    m_presubCell->RecalculateWidthsList(parser, MAX(MC_MIN_SIZE, fontsize - SUBSUP_DEC));
    mpresubcellWidth = m_presubCell->GetFullWidth(scale);
  }
  
  if(m_presupCell = NULL)
  {
    m_presupCell->RecalculateWidthsList(parser, MAX(MC_MIN_SIZE, fontsize - SUBSUP_DEC));
    mpresupcellWidth = m_presupCell->GetFullWidth(scale);
  }
  
  m_width = m_baseCell->GetFullWidth(scale) +
            MAX(mpresubcellWidth,mpresupcellWidth) +
            MAX(m_indexCell ->GetFullWidth(scale), m_exptCell  ->GetFullWidth(scale)) -
            SCALE_PX(2, parser.GetScale());
  
  ResetData();
}

void SubSupCell::RecalculateSize(CellParser& parser, int fontsize)
{
  double scale = parser.GetScale();

  m_baseCell  ->RecalculateSizeList(parser, fontsize);
  m_indexCell ->RecalculateSizeList(parser, MAX(MC_MIN_SIZE, fontsize - SUBSUP_DEC));
  m_exptCell  ->RecalculateSizeList(parser, MAX(MC_MIN_SIZE, fontsize - SUBSUP_DEC));

  int presupcellHeight = 0;
  int presubcellHeight = 0;
  int presubcellCenter = 0;
  if(m_presubCell != NULL)
  {
    m_presubCell->RecalculateSizeList(parser, MAX(MC_MIN_SIZE, fontsize - SUBSUP_DEC));
    presubcellHeight = m_presubCell->GetMaxHeight();
    presubcellCenter = m_presubCell->GetMaxCenter();
  }
  if(m_presupCell != NULL)
  {
    m_presupCell->RecalculateSizeList(parser, MAX(MC_MIN_SIZE, fontsize - SUBSUP_DEC));
    presupcellHeight = m_presupCell->GetMaxHeight();
  }

  
  m_presupCell->RecalculateSizeList(parser, MAX(MC_MIN_SIZE, fontsize - SUBSUP_DEC));

  int m_height = m_baseCell->GetMaxHeight() +
                 MAX(m_indexCell->GetMaxHeight(),presubcellHeight) +
                 MAX(m_exptCell ->GetMaxHeight(),presubcellHeight) -
                 2*SCALE_PX((8 * fontsize) / 10 + MC_EXP_INDENT, parser.GetScale());

  int m_center   = MAX(m_exptCell->GetMaxHeight(),presupcellHeight) +
                   MAX(m_baseCell->GetMaxCenter(),presubcellCenter) -
                   SCALE_PX((8 * fontsize) / 10 + MC_EXP_INDENT, scale);
}

void SubSupCell::Draw(CellParser& parser, wxPoint point, int fontsize)
{
  if (DrawThisCell(parser, point))
  {
    double scale = parser.GetScale();
    wxPoint bs, in;

  if ((m_presupCell != NULL) || (m_presubCell != NULL))
  {

    wxPoint pre;
    pre = point;
    
    int presubcellWidth = 0;
    int presupcellWidth = 0;

    if(m_presubCell != NULL)
    {
      pre.y = point.y + m_presubCell->GetMaxDrop() +
        m_presubCell->GetMaxCenter() -
        SCALE_PX((8 * fontsize) / 10 + MC_EXP_INDENT, scale);
      m_presubCell->DrawList(parser, pre, MAX(MC_MIN_SIZE, fontsize - SUBSUP_DEC));
      pre.y  = point.y - m_presubCell->GetMaxCenter();
      presubcellWidth = m_presubCell->GetWidth();
    }
    
    if(m_presupCell != NULL)
    {
      pre.y -= m_presupCell->GetMaxHeight() -
      SCALE_PX((8 * fontsize) / 10 + MC_EXP_INDENT, scale);
      m_presupCell->DrawList(parser, pre, MAX(MC_MIN_SIZE, fontsize - SUBSUP_DEC));
      presupcellWidth = m_presupCell->GetWidth();      
    }
    
    point.x += MAX ( presubcellWidth, presupcellWidth) - SCALE_PX(2, scale);

  }
  
  bs = point;
  
  m_baseCell->DrawList(parser, bs, fontsize);
  
  in.x = point.x + m_baseCell->GetFullWidth(scale) - SCALE_PX(2, scale);
  in.y = point.y + m_baseCell->GetMaxDrop() +
    m_indexCell->GetMaxCenter() -
    SCALE_PX((8 * fontsize) / 10 + MC_EXP_INDENT, scale);
  m_indexCell->DrawList(parser, in, MAX(MC_MIN_SIZE, fontsize - SUBSUP_DEC));
  
  in.y = point.y - m_baseCell->GetMaxCenter() - m_exptCell->GetMaxHeight()
    + m_exptCell->GetMaxCenter() +
    SCALE_PX((8 * fontsize) / 10 + MC_EXP_INDENT, scale);
    m_exptCell->DrawList(parser, in, MAX(MC_MIN_SIZE, fontsize - SUBSUP_DEC));
  }
  MathCell::Draw(parser, point, fontsize);
}

wxString SubSupCell::ToString()
{
  wxString s;
  if (m_baseCell->IsCompound())
    s += wxT("(") + m_baseCell->ListToString() + wxT(")");
  else
    s += m_baseCell->ListToString();
  s += wxT("[") + m_indexCell->ListToString() + wxT("]");
  s += wxT("^");
  if (m_exptCell->IsCompound())
    s += wxT("(");
  s += m_exptCell->ListToString();
  if (m_exptCell->IsCompound())
    s += wxT(")");
  return s;
}

wxString SubSupCell::ToTeX()
{
  wxConfigBase *config = wxConfig::Get();

  bool TeXExponentsAfterSubscript=false;
  
  config->Read(wxT("TeXExponentsAfterSubscript"),&TeXExponentsAfterSubscript);

  wxString s;

  if(TeXExponentsAfterSubscript &&((m_presupCell == NULL) && (m_presubCell == NULL)))
    s = wxT("{{{") + m_baseCell->ListToTeX() + wxT("}_{") +
      m_indexCell->ListToTeX() + wxT("}}^{") +
      m_exptCell->ListToTeX() + wxT("}}");
  else
  {
    if ((m_presupCell != NULL) || (m_presubCell != NULL))
    {
      s = wxT("{}");
      if (m_presubCell != NULL)
        s+= wxT("_{") + m_presubCell->ListToTeX() + wxT("}_{") +
                        m_presubCell->ListToTeX() + wxT("}");
      if (m_presupCell != NULL)
        s+= wxT("^{") + m_presupCell->ListToTeX() + wxT("}_{") +
                        m_presubCell->ListToTeX() + wxT("}");
    }
  }
  return s;
}
  
wxString SubSupCell::ToXML()
{
  wxString presubsuptext;

  if ((m_presupCell != NULL) || (m_presubCell != NULL))
  {
    if((m_presubCell != NULL))
      presubsuptext = _T("</r><r>") + m_presubCell->ListToXML()
                    + _T("</r><r>") + m_presupCell->ListToXML();
    else
      presubsuptext = _T("</r><r></r><r>") + m_presupCell->ListToXML();
  }
  
  return _T("<ie><r>") + m_baseCell->ListToXML()
    + _T("</r><r>") + m_indexCell->ListToXML()
    + _T("</r><r>") + m_exptCell->ListToXML()
    + presubsuptext
    + _T("</r></ie>");
}
    
void SubSupCell::SelectInner(wxRect& rect, MathCell **first, MathCell **last)
{
  *first = NULL;
  *last = NULL;
  if (m_indexCell->ContainsRect(rect))
    m_indexCell->SelectRect(rect, first, last);
  else if (m_baseCell->ContainsRect(rect))
    m_baseCell->SelectRect(rect, first, last);
  else if (m_exptCell->ContainsRect(rect))
    m_exptCell->SelectRect(rect, first, last);
  else if ((m_presubCell != NULL) && (m_presubCell->ContainsRect(rect)))
    m_presubCell->SelectRect(rect, first, last);
  else if ((m_presupCell != NULL) && (m_presupCell->ContainsRect(rect)))
    m_presupCell->SelectRect(rect, first, last);
  if (*first == NULL || *last == NULL)
  {
    *first = this;
    *last = this;
  }
}
