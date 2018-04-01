/*
** FamiTracker - NES/Famicom sound tracker
** Copyright (C) 2005-2014  Jonathan Liss
**
** 0CC-FamiTracker is (C) 2014-2018 HertzDevil
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
** Library General Public License for more details.  To obtain a
** copy of the GNU Library General Public License, write to the Free
** Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
**
** Any permitted reproduction of these routines, in whole or in part,
** must bear this legend.
*/


#pragma once

#include "stdafx.h"		// // //
#include <memory>		// // //

class CSequence;		// // //

class CSequenceSetting : public CWnd
{
	DECLARE_DYNAMIC(CSequenceSetting)
	CSequenceSetting(CWnd *pParent);
	virtual ~CSequenceSetting();
public:
	void Setup(CFont *pFont);
	void SelectSequence(std::shared_ptr<CSequence> pSequence, int InstrumentType);		// // //
	void Paint(CDC &dc, CPoint orig);		// // //

private:
	CWnd *m_pParent;
	CMenu m_menuPopup;
	CFont *m_pFont;
	std::shared_ptr<CSequence> m_pSequence;

	static const UINT MENU_ID_BASE, MENU_ID_MAX;		// // //

	int m_iInstType;
	bool m_bMouseOver;

protected:
	DECLARE_MESSAGE_MAP()
public:
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnMenuSettingChanged(UINT ID);		// // //
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnMouseLeave();
};
