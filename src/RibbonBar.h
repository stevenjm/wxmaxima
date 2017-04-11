// -*- mode: c++; c-file-style: "linux"; c-basic-offset: 2; indent-tabs-mode: nil -*-
//  Copyright (C) 2004-2015 Andrej Vodopivec <andrej.vodopivec@gmail.com>
//            (C) 2013 Doug Ilijev <doug.ilijev@gmail.com>
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

/*!\file
  This file declares the class RibbonBar that provides a ribbon as an alternative to the main toolbar 
 */

#ifndef _RIBBONBAR_
#define _RIBBONBAR_

#include <wx/ribbon/bar.h>
#include <wx/ribbon/buttonbar.h>
#include <wx/ribbon/gallery.h>
#include <wx/ribbon/toolbar.h>
#include "ToolBar.h"
#include "Dirstructure.h"


class RibbonBar : public wxRibbonBar
{
public:
  RibbonBar(wxWindow *parent, int id);
private:

#if defined __WXGTK__
  static wxBitmap GetImage(wxString img);
  wxBitmap  m_followIcon;
  wxBitmap  m_needsInformationIcon;
  wxBitmap  m_PlayButton;
  wxBitmap  m_StopButton;
#else
  static wxImage GetImage(wxString img);
  wxImage m_followIcon;
  wxImage m_needsInformationIcon;
  wxImage m_PlayButton;
  wxImage m_StopButton;
#endif
};

#endif
