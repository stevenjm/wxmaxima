// -*- mode: c++; c-file-style: "linux"; c-basic-offset: 2; indent-tabs-mode: nil -*-
//
//  Copyright (C) 2004-2015 Andrej Vodopivec <andrej.vodopivec@gmail.com>
//  Copyright (C) 2014-2018 Gunter Königsmann <wxMaxima@physikbuch.de>
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
  
  The definition of the base class of all cells the worksheet consists of.
 */

#ifndef MATHCELL_H
#define MATHCELL_H

#include <list>
#include <wx/wx.h>
#include <wx/xml/xml.h>
#if wxUSE_ACCESSIBILITY
#include "wx/access.h"
#include <wx/hashmap.h>
#include <wx/scrolwin.h>
#endif // wxUSE_ACCESSIBILITY
#include "Configuration.h"
#include "TextStyle.h"

/*! The supported types of math cells
 */
enum CellType
{
  MC_TYPE_DEFAULT,
  MC_TYPE_MAIN_PROMPT, //!< Input labels
  MC_TYPE_PROMPT,      //!< Maxima questions or lisp prompts
  MC_TYPE_LABEL,       //!< An output label generated by maxima
  MC_TYPE_INPUT,       //!< A cell containing code
  MC_TYPE_WARNING,     //!< A warning output by maxima
  MC_TYPE_ERROR,       //!< An error output by maxima
  MC_TYPE_TEXT,        //!< Text that isn't passed to maxima
  MC_TYPE_SUBSECTION,  //!< A subsection name
  MC_TYPE_SUBSUBSECTION,  //!< A subsubsection name
  MC_TYPE_HEADING5,  //!< A subsubsection name
  MC_TYPE_HEADING6,  //!< A subsubsection name
  MC_TYPE_SECTION,     //!< A section name
  MC_TYPE_TITLE,       //!< The title of the document
  MC_TYPE_IMAGE,       //!< An image
  MC_TYPE_SLIDE,       //!< An animation created by the with_slider_* maxima commands
  MC_TYPE_GROUP        //!< A group cells that bundles several individual cells together
};

/*!
  The base class all cell types the worksheet can consist of are derived from

  Every Cell is part of two double-linked lists:
   - A Cell does have a member m_previous that points to the previous item
     (or contains a NULL for the head node of the list) and a member named m_next 
     that points to the next cell (or contains a NULL if this is the end node of a list).
   - And there is m_previousToDraw and m_nextToDraw that contain fractions and similar 
     items as one element if they are drawn as a single 2D object that isn't divided by
     a line break, but will contain every single element of a fraction as a separate 
     object if the fraction is broken into several lines and therefore displayed in its
     a linear form.

  Also every list of Cells can be a branch of a tree since every math cell contains
  a pointer to its parent group cell.

  Besides the cell types that are directly user visible there are cells for several
  kinds of items that are displayed in a special way like abs() statements (displayed
  as horizontal rules), subscripts, superscripts and exponents.
  Another important concept realized by a class derived from this one is
  the group cell that groups all things that are foldable in the gui like:
   - A combination of maxima input with the output, the input prompt and the output 
     label.
   - A chapter or a section and
   - Images with their title (or the input cells that generated them)
   .

  \attention Derived classes must test if m_next equals NULL and if it doesn't
  they have to delete() it.

  On systems where wxWidget supports (and is compiled with)
  accessibility features Cell is derived from wxAccessible which
  allows every element in the worksheet to identify itself to an
  eventual screen reader.

 */
#if wxUSE_ACCESSIBILITY
class Cell: public wxAccessible
#else
class Cell
#endif
{
  public:
  Cell(Cell *group, Configuration **config);

  /*! Scale font sizes and line widths according to the zoom factor.

    Is used for displaying/printing/exporting of text/maths
   */
  int Scale_Px(double px){ return (*m_configuration)->Scale_Px(px);}
#if wxUSE_ACCESSIBILITY
  //! Accessibility: Describe the current cell to a Screen Reader
  virtual wxAccStatus GetDescription(int childId, wxString *description);
  //! Accessibility: Inform the Screen Reader which cell is the parent of this one
  wxAccStatus GetParent (wxAccessible ** parent);
  //! Accessibility: How many childs of this cell GetChild() can retrieve?
  virtual wxAccStatus GetChildCount (int *childCount);
  //! Accessibility: Retrieve a child cell. childId=0 is the current cell
  virtual wxAccStatus GetChild (int childId, Cell **child);
  //! Accessibility: Does this or a child cell currently own the focus?
  virtual wxAccStatus GetFocus (int *childId, Cell  **child);
  //! Accessibility: Where is this cell to be found?
  virtual wxAccStatus GetLocation (wxRect &rect, int elementId);
  //! Is pt inside this cell or a child cell?
  wxAccStatus HitTest (const wxPoint &pt,
                       int *childId, Cell **childObject);
  //! Accessibility: What is the contents of this cell?
  virtual wxAccStatus GetValue (int childId, wxString *strValue);
  virtual wxAccStatus GetRole (int childId, wxAccRole *role);
#endif
  
  wxString m_toolTip;

  /*! Returns the ToolTip this cell provides.

    wxEmptyString means: No ToolTip
   */
  virtual wxString GetToolTip(const wxPoint &point);

  //! Delete this list of cells.
  virtual ~Cell();

  //! How many cells does this cell contain?
  int CellsInListRecursive();

  /*! If the cell is moved to the undo buffer this function drops pointers to it
  
    Examples are the pointer to the start or the end of the selection.

    \attention If this method is overridden the overiding function needs to call
    this function.
  */
  virtual void MarkAsDeleted();
  
  //! The part of the rectangle rect that is in the region that is currently drawn
  wxRect CropToUpdateRegion(wxRect rect)
    {
      if(!(*m_configuration)->ClipToDrawRegion())
        return rect;
      else
        return rect.Intersect((*m_configuration)->GetUpdateRegion());
    }

  //! Is part of this rectangle in the region that is currently drawn?
  bool InUpdateRegion(const wxRect &rect);

  //! Is this cell inside the region that is currently drawn?
  bool InUpdateRegion() {return InUpdateRegion(GetRect());}

  /*! Add a cell to the end of the list this cell is part of
    
    \param p_next The cell that will be appended to the list.
   */
  void AppendCell(Cell *p_next);

  //! 0 for ordinary cells, 1 for slide shows and diagrams displayed with a 1-pixel border
  int m_imageBorderWidth;

  //! Do we want this cell to start with a linebreak?
  void SoftLineBreak(bool breakLine = true)
  { m_breakLine = breakLine; }

  //! Does this cell to start with a linebreak?
  bool LineBreakAtBeginning()
  { return m_breakLine || m_breakPage || m_forceBreakLine; }

  //! Do we want this cell to start with a pagebreak?
  void BreakPage(bool breakPage)
  { m_breakPage = breakPage; }

  //! Are we allowed to break a line here?
  bool BreakLineHere();

  //! Does this cell begin with a manual linebreak?
  bool HardLineBreak()
  { return m_forceBreakLine; }

  //! Does this cell begin with a manual page break?
  bool BreakPageHere()
  { return m_breakPage; }

  /*! Try to split this command into lines to make it fit on the screen

    \retval true = This cell was split into lines.
  */
  virtual bool BreakUp()
  { return false; }

  /*! Is a part of this cell inside a certain rectangle?

    \param big The rectangle to test for collision with this cell
    \param all
     - true means test this cell and the ones that are following it in the list
     - false means test this cell only.
   */
  bool ContainsRect(const wxRect &big, bool all = true);

  /*! Is a given point inside this cell?

    \param point The point to test for collision with this cell
   */
  bool ContainsPoint(const wxPoint &point)
  {
    return GetRect().Contains(point);
  }

  void CopyData(Cell *s, Cell *t);

  /*! Clears memory from cached items automatically regenerated when the cell is drawn
    
    The scaled version of the image will be recreated automatically once it is 
    needed.
   */
  virtual void ClearCache()
  {}

  /*! Clears the cache of the whole list of cells starting with this one.

    For details see ClearCache().
   */
  void ClearCacheList();

  /*! Draw this cell

    \param point The x and y position this cell is drawn at: All top-level cells get their
    position during recalculation. But for the cells within them the position needs a 
    second step after determining the dimension of the contents of the top-level cell.

    Example: The position of the denominator of a fraction can only be determined
    after the height of denominator and numerator are known.
   */
  virtual void Draw(wxPoint point);

  virtual void Draw(){Draw(m_currentPoint);}

  /*! Draw this list of cells

    \param point The x and y position this cell is drawn at
   */
  void DrawList(wxPoint point);
  void DrawList(){DrawList(m_currentPoint);}

  /*! Draw a rectangle that marks this cell or this list of cells as selected

    \param all
     - true:  Draw the bounding box around this list of cells
     - false: Draw the bounding box around this cell only
     \param dc The drawing context the box is drawn in.
  */
  virtual void DrawBoundingBox(wxDC &WXUNUSED(dc), bool all = false);

  bool DrawThisCell(wxPoint point);
  bool DrawThisCell(){return DrawThisCell(m_currentPoint);}

  /*! Insert (or remove) a forced linebreak at the beginning of this cell.

    \param force
     - true: Insert a forced linebreak
     - false: Remove the forced linebreak
   */
  void ForceBreakLine(bool force = true)
  { m_forceBreakLine = m_breakLine = force; }

  /*! Get the height of this cell

    This value is recalculated by RecalculateHeight; -1 means: Needs to be recalculated.
  */
  int GetHeight()
  { return m_height; }

  /*! Get the width of this cell

    This value is recalculated by RecalculateWidth; -1 means: Needs to be recalculated.
  */
  int GetWidth()
  { return m_width; }

  /*! Get the distance between the top and the center of this cell.

    Remember that (for example with double fractions) the center does not have to be in the 
    middle of a cell even if this object is --- by definition --- center-aligned.
   */
  int GetCenter()
  { return m_center; }

  /*! Get the distance between the center and the bottom of this cell


    Remember that (for example with double fractions) the center does not have to be in the 
    middle of an output cell even if the current object is --- by definition --- 
    center-aligned.

    This value is recalculated by RecalculateHeight; -1 means: Needs to be recalculated.
   */
  int GetDrop()
  { return m_height - m_center; }

  /*! 
    Returns the type of this cell.
   */
  CellType GetType()
  { return m_type; }

  /*! Returns the maximum distance between center and bottom of this line

    Note that the center doesn't need to be exactly in the middle of an object.
    For a fraction for example the center is exactly at the middle of the 
    horizontal line.
   */
  int GetMaxDrop();

  /*! Returns the maximum distance between top and center of this line

    Note that the center doesn't need to be exactly in the middle of an object.
    For a fraction for example the center is exactly at the middle of the 
    horizontal line.
  */
  int GetMaxCenter();

  /*! Returns the total height of this line

    Returns GetMaxCenter()+GetMaxDrop()
   */
  int GetMaxHeight();

  //! How many pixels is this list of cells wide?
  int GetFullWidth();

  /*! How many pixels is the current line of this list of cells wide?

    This command returns the real line width when all line breaks are really performed. 
    See GetFullWidth().
   */
  int GetLineWidth();

  /*! Get the x position of the top left of this cell

    See m_currentPoint for more details.
   */
  int GetCurrentX()
  { return m_currentPoint.x; }

  /*! Get the y position of the top left of this cell

    See m_currentPoint for more details.
   */
  int GetCurrentY()
  { return m_currentPoint.y; }

  /*! Get the smallest rectangle this cell fits in

    \param all
      - true: Get the rectangle for this cell and the ones that follow it in the list of cells
      - false: Get the rectangle for this cell only.
   */
  virtual wxRect GetRect(bool all = false);

  //! True, if something that affects the cell size has changed.
  virtual bool NeedsRecalculation();
  
  virtual wxString GetDiffPart();

  /*! Recalculate the height of the cell and the difference between top and center

    Must set: m_height, m_center.

    \param fontsize In exponents, super- and subscripts the font size is reduced.
    This cell therefore needs to know which font size it has to be drawn at.
  */
  virtual void RecalculateHeight(int fontsize)
    {m_fontSize = fontsize;}

  /*! Recalculate the height of this list of cells

    \param fontsize In exponents, super- and subscripts the font size is reduced.
    This cell therefore needs to know which font size it has to be drawn at.
   */
  void RecalculateHeightList(int fontsize);

  /*! Recalculate the width of this cell.

    Must set: m_width.

    \param fontsize In exponents, super- and subscripts the font size is reduced.
    This cell therefore needs to know which font size it has to be drawn at.
   */
  virtual void RecalculateWidths(int fontsize);

  /*! Recalculates all widths of this list of cells.

    \param fontsize In exponents, super- and subscripts the font size is reduced.
    This cell therefore needs to know which font size it has to be drawn at.
   */
  void RecalculateWidthsList(const int &fontsize);

  /*! Recalculate both width and height of this list of cells.

    Is faster than a <code>RecalculateHeightList();RecalculateWidths();</code>.
   */
  void RecalculateList(int fontsize);

  //! Mark all cached size information as "to be calculated".
  void ResetData();

  //! Mark the cached height information as "to be calculated".
  void ResetSize()
  { m_width = m_height = m_center = m_maxCenter = m_maxDrop = m_fullWidth = m_lineWidth = -1; }

  //! Mark the cached height information of the whole list of cells as "to be calculated".
  void ResetSizeList();

  void SetSkip(bool skip)
  { m_bigSkip = skip; }

  //! Sets the text style according to the type
  virtual void SetType(CellType type);

  TextStyle GetStyle()
  { return m_textStyle; }  //l'ho aggiunto io

  void SetPen(double lineWidth = 1.0);

  //! Mark this cell as highlighted (e.G. being in a maxima box)
  void SetHighlight(bool highlight)
  { m_highlight = highlight; }

  //! Is this cell highlighted (e.G. inside a maxima box)
  bool GetHighlight()
  { return m_highlight; }

  virtual void SetExponentFlag()
  {}

  virtual void SetValue(const wxString &WXUNUSED(text))
  {}

  virtual wxString GetValue()
  { return wxEmptyString; }

  //! Get the first cell in this list of cells
  Cell *first();

  //! Get the last cell in this list of cells
  Cell *last();

  /*! Select a rectangle using the mouse

    \param rect The rectangle to select
    \param first Returns the first cell of the rectangle
    \param last Returns the last cell of the rectangle
   */
  void SelectRect(wxRect &rect, Cell **first, Cell **last);

  /*! The top left of the rectangle the mouse has selected

    \param rect The rectangle the mouse selected
    \param first Returns the first cell of the rectangle
   */
  void SelectFirst(wxRect &rect, Cell **first);

  /*! The bottom right of the rectangle the mouse has selected

    \param rect The rectangle the mouse selected
    \param last Returns the last cell of the rectangle
   */
  void SelectLast(wxRect &rect, Cell **last);

  /*! Select the cells inside this cell described by the rectangle rect.
  */
  virtual void SelectInner(wxRect &rect, Cell **first, Cell **last);

  //! Is this cell an operator?
  virtual bool IsOperator();

  bool IsCompound();

  virtual bool IsShortNum()
  { return false; }

  //! Returns the group cell this cell belongs to
  Cell *GetGroup();

  //! For the bitmap export we sometimes want to know how big the result will be...
  struct SizeInMillimeters
  {
  public:
    double x, y;
  };

  //! Returns the list's representation as a string.
  virtual wxString ListToString();

  //! Convert this list to its LaTeX representation
  virtual wxString ListToMatlab();

  //! Convert this list to its LaTeX representation
  virtual wxString ListToTeX();

  //! Convert this list to an representation fit for saving in a .wxmx file
  virtual wxString ListToXML();

  //! Convert this list to a MathML representation
  virtual wxString ListToMathML(bool startofline = false);

  //! Convert this list to an OMML representation
  virtual wxString ListToOMML(bool startofline = false);

  //! Convert this list to an RTF representation
  virtual wxString ListToRTF(bool startofline = false);

  //! Returns the cell's representation as a string.
  virtual wxString ToString();

  /*! Returns the cell's representation as RTF.

    If this method returns wxEmptyString this might mean that this cell is 
    better handled in OMML.
   */
  virtual wxString ToRTF()
  { return wxEmptyString; }

  //! Converts an OMML tag to the corresponding RTF snippet
  wxString OMML2RTF(wxXmlNode *node);

  //! Converts OMML math to RTF math
  wxString OMML2RTF(wxString data);

  /*! Returns the cell's representation as OMML

    If this method returns wxEmptyString this might mean that this cell is 
    better handled in RTF; The OOML can later be translated to the 
    respective RTF maths commands using OMML2RTF.

    Don't know why OMML was implemented in a world that already knows MathML,
    though.
   */
  virtual wxString ToOMML()
  { return wxEmptyString; }

  //! Convert this cell to its Matlab representation
  virtual wxString ToMatlab();

  //! Convert this cell to its LaTeX representation
  virtual wxString ToTeX();

  //! Convert this cell to an representation fit for saving in a .wxmx file
  virtual wxString ToXML();

  //! Convert this cell to an representation fit for saving in a .wxmx file
  virtual wxString ToMathML();

  //! Escape a string for RTF
  static wxString RTFescape(wxString, bool MarkDown = false);

  //! Escape a string for XML
  static wxString XMLescape(wxString);

  void UnsetPen();

  /*! Undo breaking this cell into multiple lines

    Some cells have different representations when they contain a line break.
    Examples for this are fractions or a set of parenthesis.

    This function tries to return a cell to the single-line form.
   */
  virtual void Unbreak();

  /*! Unbreak this line

    Some cells have different representations when they contain a line break.
    Examples for this are fractions or a set of parenthesis.

    This function tries to return a list of cells to the single-line form.
  */
  virtual void UnbreakList();

  /*! The next cell in the list of cells

    Reads NULL, if this is the last cell of the list. See also m_nextToDraw, m_previous
    and m_previousToDraw
   */
  Cell *m_next;
  /*! The previous cell in the list of cells
    
    Reads NULL, if this is the first cell of the list. See also m_previousToDraw, 
    m_nextToDraw and m_next
   */
  Cell *m_previous;
  /*! The next cell to draw
    
    For cells that are drawn as an atomic construct this pointer points 
    to the cell that follows this one just like m_next would.
    
    In the case that a cell is broken into two or more lines the cell
    itself won't be drawn, but will be handled as a list of individual
    elements that can be drawn on individual lines, if needed.

    In the example of a SqrtCell if the SqrtCell isn't broken into lines
    m_nextToDraw will point to the cell following the SqrtCell. If the
    SqrtCell is broken into lines the list made up by m_nextToDraw and
    m_previousToDraw will point to the sqrtCell, then to a cell
    containing the word "sqrt", then one that represents the opening
    and closing parenthesis (that will again be followed by a list 
    containing the "(", the contents and the ")" of the parenthesis
    as individdual elements) and then the cell that follows the SqrtCell. 

    See also m_previousToDraw and m_next.
   */
  Cell *m_nextToDraw;
  /*! The previous cell to draw
    
    Normally cells are drawn one by one. But if a function is broken into several lines 
    the list made up by m_nextToDraw and m_previousToDraw will contain the function itself,
    the function name and its arguments as individual list elements so they can be drawn
    separately (and on separate lines).
   */
  Cell *m_previousToDraw;
  bool m_bigSkip;
  /*! true means:  This cell is broken into two or more lines.
    
    Long abs(), conjugate(), fraction and similar cells can be displayed as 2D objects,
    but will be displayed in their linear form (and therefore broken into lines) if they
    end up to be wider than the screen. In this case m_isBrokenIntoLines is true.
   */
  bool m_isBrokenIntoLines;
  /*! True means: This cell is not to be drawn.

    Currently the following items fall into this category:
     - parenthesis around fractions or similar things that clearly can be recognized as atoms
     - plus signs within numbers
     - most multiplication dots.
   */
  bool m_isHidden;

  /*! Determine if this cell contains text that isn't code

    \return true, if this is a text cell, a title cell, a section, a subsection or a sub(n)section cell.
   */
  bool IsComment()
  {
    return m_type == MC_TYPE_TEXT || m_type == MC_TYPE_SECTION ||
           m_type == MC_TYPE_SUBSECTION || m_type == MC_TYPE_SUBSUBSECTION ||
           m_type == MC_TYPE_HEADING5 || m_type == MC_TYPE_HEADING6 || m_type == MC_TYPE_TITLE;
  }

  bool IsEditable(bool input = false)
  {
    return (m_type == MC_TYPE_INPUT &&
            m_previous != NULL && m_previous->m_type == MC_TYPE_MAIN_PROMPT)
           || (!input && IsComment());
  }

  //! Processes a key event.
  virtual void ProcessEvent(wxKeyEvent &WXUNUSED(event))
  {}

  //! Add a semicolon to a cell, of needed.
  virtual bool AddEnding()
  { return false; }

  virtual void SelectPointText(const wxPoint &point);
      
  virtual void SelectRectText(const wxPoint &one, const wxPoint &two);
  
  virtual void PasteFromClipboard(const bool &primary = false);
  
  virtual bool CopyToClipboard()
  { return false; }

  virtual bool CutToClipboard()
  { return false; }

  virtual void SelectAll()
  {}

  virtual bool CanCopy()
  { return false; }

  virtual void SetMatchParens(bool WXUNUSED(match))
  {}

  virtual wxPoint PositionToPoint(int WXUNUSED(fontsize), int WXUNUSED(pos) = -1)
  { return wxPoint(-1, -1); }

  virtual bool IsDirty()
  { return false; }

  virtual void SwitchCaretDisplay()
  {}

  virtual void SetFocus(bool WXUNUSED(focus))
  {}

  void SetForeground();

  virtual bool IsActive()
  { return false; }

  /*! Define which GroupCell is the parent of this cell.
    
    By definition every math cell is part of a group cell.
    So this function has to be called on every math cell. Also if a
    derived class defines a cell type that does include sub-cells 
    (One example would be the argument of a sqrt() cell) the derived
    class has to take care that the subCell's SetGroup is called when
    the cell's SetGroup is called.
   */
  
  virtual void SetParent(Cell *parent)
    { m_parent = parent; }

  /*! Define which Cell is the GroupCell this list of cells belongs to

    Also automatically sets this cell as the "parent" of all cells of the list.
   */
  void SetGroupList(Cell *parent);

  //! Define which Sell is the GroupCell this list of cells belongs to
  virtual void SetGroup(Cell *parent);
  
  virtual void SetStyle(TextStyle style)
  {
    m_textStyle = style;
    ResetData();
  }

  bool IsMath();

  void SetAltCopyText(wxString text)
  { m_altCopyText = text; }

  /*! Attach a copy of the list of cells that follows this one to a cell
    
    Used by Cell::Copy() when the parameter <code>all</code> is true.
  */
  Cell *CopyList();

  /*! Copy this cell
    
    This method is used by CopyList() which creates a copy of a cell tree. 
    
    \return A copy of this cell without the rest of the list this cell is part 
    from.
  */
  virtual Cell *Copy() = 0;

  /*! Do we want to begin this cell with a center dot if it is part of a product?

    Maxima will represent a product like (a*b*c) by a list like the following:
    [*,a,b,c]. This would result us in converting (a*b*c) to the following LaTeX
    code: \\left(\\cdot a ß\\cdot b \\cdot c\\right) which obviously is one \\cdot too
    many => we need parenthesis cells to set this flag for the first cell in 
    their "inner cell" list.
   */
  bool m_SuppressMultiplicationDot;

  //! Set the tooltip of this math cell. wxEmptyString means: no tooltip.
  void SetToolTip(const wxString &tooltip){m_toolTip = tooltip;}
  void SetCurrentPoint(wxPoint point){m_currentPoint = point;}
  void SetCurrentPoint(int x, int y){m_currentPoint = wxPoint(x,y);}
  wxPoint GetCurrentPoint(){return m_currentPoint;}
  
protected:
  /*! The point in the work sheet at which this cell begins.

    The begin of a cell is defined as 
     - x=the left border of the cell
     - y=the vertical center of the cell. Which (per example in the case of a fraction)
       might not be the physical center but the vertical position of the horizontal line
       between nummerator and denominator.

    The current point is recalculated 
     - for GroupCells by GroupCell::RecalculateHeight
     - for EditorCells by it's GroupCell's RecalculateHeight and
     - for Cells when they are drawn.
  */
  wxPoint m_currentPoint;

  /*! The GroupCell this list of cells belongs to.
    
    Reads NULL, if no parent cell has been set - which is treated as an Error by GetGroup():
    every math cell has a GroupCell it belongs to.
  */
  Cell *m_group;

  //! The cell that contains the current cell
  Cell *m_parent;

  //! Does this cell begin with a forced page break?
  bool m_breakPage;
  //! Are we allowed to add a line break before this cell?
  bool m_breakLine;
  //! true means we force this cell to begin with a line break.  
  bool m_forceBreakLine;
  bool m_highlight;
  /* Text that should end up on the clipboard if this cell is copied as text.

     \attention  m_altCopyText is not check in all cell types!
  */
  wxString m_altCopyText;
  Configuration **m_configuration;

virtual std::list<Cell *> GetInnerCells() = 0;

protected:
  //! The height of this cell.
  int m_height;
  /*! The width of this cell.

    Is recalculated by RecalculateHeight.
   */
  int m_width;
  /*! Caches the width of the list starting with this cell.

    - Will contain -1, if it has not yet been calculated.
    - Won't be recalculated on appending new cells to the list.
  */
  int m_fullWidth;
  /*! Caches the width of the rest of the line this cell is part of.

    - Will contain -1, if it has not yet been calculated.
    - Won't be recalculated on appending new cells to the list.
  */
  int m_lineWidth;
  int m_center;
  int m_maxCenter;
  int m_maxDrop;
  CellType m_type;
  TextStyle m_textStyle;
  //! The font size is smaller in super- and subscripts.
  int m_fontSize;

public:
  /*! The storage for pointers to cells.
    
    If a cell is deleted it is necessary to remove all pointers that might
    allow to access the now-defunct cell. These pointers are kept in this 
    per-worksheet structure.
  */
  class CellPointers
  {
  public:
    void ScrollToCell(Cell *cell){m_cellToScrollTo = cell;}
    Cell *CellToScrollTo(){return m_cellToScrollTo;}
    CellPointers(wxScrolledCanvas *mathCtrl);
    /*! Returns the cell maxima currently works on. NULL if there isn't such a cell.
      
      \param resortToLast true = if we already have set the cell maxima works on to NULL
      use the last cell maxima was known to work on.
    */
    Cell *GetWorkingGroup(bool resortToLast = false)
      {
        if ((m_workingGroup != NULL) || (!resortToLast))
          return m_workingGroup;
        else
          return m_lastWorkingGroup;
      }

    //! Sets the cell maxima currently works on. NULL if there isn't such a cell.
    void SetWorkingGroup(Cell *group)
      {
        if(group != NULL)
          m_lastWorkingGroup = group;
        m_workingGroup = group;
      }
    
    void WXMXResetCounter()
      { m_wxmxImgCounter = 0; }
    
    wxString WXMXGetNewFileName();
    
    int WXMXImageCount()
      { return m_wxmxImgCounter; }

    //! A list of editor cells containing error messages.
    class ErrorList
    {
    public:
      ErrorList(){};
      //! Is the list of errors empty?
      bool Empty(){return m_errorList.empty();}
      //! Remove one specific GroupCell from the list of errors
      void Remove(Cell * cell){m_errorList.remove(cell);}
      //! Does the list of GroupCell with errors contain cell?
      bool Contains(Cell * cell);
      //! Mark this GroupCell as containing errors
      void Add(Cell * cell){m_errorList.push_back(cell);}
      //! The first GroupCell with error that is still in the list
      Cell *FirstError(){if(m_errorList.empty())return NULL; else return m_errorList.front();}
      //! The last GroupCell with errors in the list
      Cell *LastError(){if(m_errorList.empty())return NULL; else return m_errorList.back();}
      //! Empty the list of GroupCells with errors
      void Clear(){m_errorList.clear();}
    private:
      //! A list of GroupCells that contain errors
      std::list<Cell *> m_errorList;
    };

    //! The list of cells maxima has complained about errors in
    ErrorList m_errorList;
    //! The EditorCell the mouse selection has started in
    Cell *m_cellMouseSelectionStartedIn;
    //! The EditorCell the keyboard selection has started in
    Cell *m_cellKeyboardSelectionStartedIn;
    //! The EditorCell the search was started in
    Cell *m_cellSearchStartedIn;
    //! Which cursor position incremental search has started at?
    int m_indexSearchStartedAt;
    //! Which cell the blinking cursor is in?
    Cell *m_activeCell;
    //! The GroupCell that is under the mouse pointer 
    Cell *m_groupCellUnderPointer;
    //! The EditorCell that contains the currently active question from maxima 
    Cell *m_answerCell;
    //! The last group cell maxima was working on.
    Cell *m_lastWorkingGroup;
    //! The textcell the text maxima is sending us was ending in.
    Cell *m_currentTextCell;
    /*! The group cell maxima is currently working on.

      NULL means that maxima isn't currently evaluating a cell.
    */
    Cell *m_workingGroup;
    /*! The currently selected string. 

      Since this string is defined here it is available in every editor cell
      for highlighting other instances of the selected string.
    */
    wxString m_selectionString;

    //! Forget where the search was started
    void ResetSearchStart()
      {
        m_cellSearchStartedIn = NULL;
        m_indexSearchStartedAt = -1;
      }

    //! Forget where the mouse selection was started
    void ResetMouseSelectionStart()
      { m_cellMouseSelectionStartedIn = NULL; }

    //! Forget where the keyboard selection was started
    void ResetKeyboardSelectionStart()
      { m_cellKeyboardSelectionStartedIn = NULL; }
  
    /*! The first cell of the currently selected range of Cells.
    
      NULL, when no Cells are selected and NULL, if only stuff inside a EditorCell
      is selected and therefore the selection is handled by EditorCell; This cell is 
      always above m_selectionEnd.
    
      See also m_hCaretPositionStart and m_selectionEnd
    */
    Cell *m_selectionStart;
    /*! The last cell of the currently selected range of groupCells.
    
      NULL, when no GroupCells are selected and NULL, if only stuff inside a GroupCell
      is selected and therefore the selection is handled by EditorCell; This cell is 
      always below m_selectionStart.
    
      See also m_hCaretPositionEnd
    */

    //! The cell currently under the mouse pointer
    Cell *m_cellUnderPointer;
  
    /*! The last cell of the currently selected range of Cells.
    
      NULL, when no Cells are selected and NULL, if only stuff inside a EditorCell
      is selected and therefore the selection is handled by EditorCell; This cell is 
      always above m_selectionEnd.
    
      See also m_hCaretPositionStart, m_hCaretPositionEnd and m_selectionStart.
    */
    Cell *m_selectionEnd;
    WX_DECLARE_VOIDPTR_HASH_MAP( int, SlideShowTimersList);
    SlideShowTimersList m_slideShowTimers;

    wxScrolledCanvas *GetMathCtrl(){return m_mathCtrl;}

    //! Is scrolling to a cell scheduled?
    bool m_scrollToCell;
  private:
    //! If m_scrollToCell = true: Which cell do we need to scroll to?
    Cell *m_cellToScrollTo;
    //! The function to call if an animation has to be stepped.
    wxScrolledCanvas *m_mathCtrl;
    //! The image counter for saving .wxmx files
    int m_wxmxImgCounter;
  };

protected:
  CellPointers *m_cellPointers;

private:
  //! The client width at the time of the last recalculation.
  int m_clientWidth_old;
  //! The zoom factor at the time of the last recalculation.
  double m_lastZoomFactor;
};

#endif // MATHCELL_H


