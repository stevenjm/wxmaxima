// -*- mode: c++; c-file-style: "linux"; c-basic-offset: 2; indent-tabs-mode: nil -*-
//
//  Copyright (C) 2004-2015 Andrej Vodopivec <andrej.vodopivec@gmail.com>
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

/*! \file
  This file defines the class ExptCell

  ExptCell is the Cell type that represents exponents.
 */

#include "MaximaTokenizer.h"
#include <wx/wx.h>
#include <wx/string.h>

MaximaTokenizer::MaximaTokenizer(wxString commands)
{
  wxString token;

  wxString::const_iterator it = commands.begin();

  while (it < commands.end())
  {
    // Determine the current char and the one that will follow it
    wxChar Ch = *it;
    wxString::const_iterator it2(it);
    if(it2 < commands.end())
      ++it2;
    wxChar nextChar;

    if(it2 < commands.end())
      nextChar = *it2;
    else
      nextChar = wxT(' ');

    // Check for newline characters (hard+soft line break)
    if ((Ch == wxT('\n')) || (Ch == wxT('\r')))
    {
      if (token != wxEmptyString)
      {
        m_tokens.Add(token);
        token = wxEmptyString;
      }
      m_tokens.Add(wxT("\n"));
      ++it;
    }
    // A minus and a plus are special tokens as they can be both
    // operators or part of a number.
    else if (
            (Ch == wxT('+')) ||
            (Ch == wxT('-')) ||
            (Ch == wxT('\x2212')) // An unicode minus sign
            )
    {
      if (token != wxEmptyString)
        m_tokens.Add(token);
      token = wxString(Ch);
      m_tokens.Add(token);
      ++it;
      token = wxEmptyString;
    }
    // Check for "comment start" or "comment end" markers
    else if (((Ch == '/') && ((nextChar == wxT('*')) || (nextChar == wxT('\xB7')))) ||
             (((Ch == wxT('*')) || (Ch == wxT('\xB7'))) && ((nextChar == wxT('/')))))
    {
      if (token != wxEmptyString)
      {
        m_tokens.Add(token);
        token = wxEmptyString;
      }
      m_tokens.Add(wxString(Ch) + nextChar);
      ++it;
      if(it < commands.end())
        ++it;
    }

    // Find operators that start at the current position
    else if (Operators().Find(Ch) != wxNOT_FOUND)
    {
      if (token != wxEmptyString)
      {
        m_tokens.Add(token);
        token = wxEmptyString;
      }
      m_tokens.Add(wxString(Ch));
      ++it;
    }
    // Find a keyword that starts at the current position
    else if (IsAlpha(Ch) || (Ch == '\\') || (Ch == '?'))
    {
      if (token != wxEmptyString)
      {
        m_tokens.Add(token);
        token = wxEmptyString;
      }

      if(Ch == '?')
      {
        token += Ch;
        it++;
        Ch = *it;
      }
      
      while ((it < commands.end()) && (IsAlphaNum(Ch = *it)))
      {
        token += Ch;

        if (Ch == wxT('\\'))
        {
          ++it;
          if (it < commands.end())
          {
            Ch = *it;
            if (Ch != wxT('\n'))
              token += Ch;
            else
            {
              m_tokens.Add(token);
              token = wxEmptyString;

              break;
            }
          }
        }
        if(it < commands.end())
          ++it;
      }
      m_tokens.Add(token);
      token = wxEmptyString;
    }
    // Find a string that starts at the current position
    else if (Ch == wxT('\"'))
    {
      if (token != wxEmptyString)
        m_tokens.Add(token);

      // Add the opening quote
      token = Ch;
      ++it;

      // Add the string contents
      while (it < commands.end())
      {
        Ch = *it;
        token += Ch;
        ++it;
        if(Ch == wxT('\\'))
        {
          if(it < commands.end())
          {
            token += *it;
            ++it;
          }
        }
        else if(Ch == wxT('\"'))
          break;
      }
      m_tokens.Add(token);
      token = wxEmptyString;
    }
    // Find a number
    else if (IsNum(Ch))
    {
      if (token != wxEmptyString)
      {
        m_tokens.Add(token);
        token = wxEmptyString;
      }

      while ((it < commands.end()) &&
             (IsNum(Ch) ||
              ((Ch >= wxT('a')) && (Ch <= wxT('z'))) ||
              ((Ch >= wxT('A')) && (Ch <= wxT('Z')))
             )
              )
      {
        token += Ch;
        if (++it < commands.end())
        {
          Ch = *it;
        }
      }

      m_tokens.Add(token);
      token = wxEmptyString;
    }
    // Merge consecutive spaces into one single token
    else if (Ch == wxT(' '))
    {
      while ((it < commands.end()) &&
             (Ch == wxT(' '))
              )
      {
        token += Ch;
        if (++it < commands.end()) {
          Ch = *it;
        }
      }

      m_tokens.Add(token);
      token = wxEmptyString;
    }
    else
    {
      token = token + Ch;
      ++it;
    }
  }

  // Add the last token we detected to the token list
  m_tokens.Add(token);
}

bool MaximaTokenizer::IsAlpha(wxChar ch)
{
  static const wxString alphas = wxT("\\_%");

  if (wxIsalpha(ch))
    return true;

  return alphas.Find(ch) != wxNOT_FOUND;

}

bool MaximaTokenizer::IsNum(wxChar ch)
{
  return ch >= '0' && ch <= '9';
}

bool MaximaTokenizer::IsAlphaNum(wxChar ch)
{
  return IsAlpha(ch) || IsNum(ch);
}
