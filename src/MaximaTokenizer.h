// -*- mode: c++; c-file-style: "linux"; c-basic-offset: 2; indent-tabs-mode: nil -*-
//
//  Copyright (C) 2004-2015 Andrej Vodopivec <andrej.vodopivec@gmail.com>
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
//  SPDX-License-Identifier: GPL-2.0+

#ifndef MAXIMATOKENIZER_H
#define MAXIMATOKENIZER_H

#include <wx/wx.h>
#include <wx/string.h>
#include <wx/arrstr.h>

/*!\file

  This file declares the class Maximatokenizer.

  Maximatokenizer breaks down maxima input to individual commands.
 */

/*! Maximatokenizer breaks down maxima input to individual commands.
 */

class MaximaTokenizer
{
public:
  MaximaTokenizer(wxString commands);

  static bool IsAlpha(wxChar ch);
  static bool IsNum(wxChar ch);
  static bool IsAlphaNum(wxChar ch);
  static const wxString Operators(){return wxString("+-*/^:=#'!;$");}

  wxArrayString GetTokens()
  {
    return m_tokens;
  }

  
protected:
  wxArrayString m_tokens;
};

#endif // MAXIMATOKENIZER_H
