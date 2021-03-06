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
#include <cstdint>		// // //

class CSequence;
class CGraphEditor;
class CSizeEditor;
class CSequenceSetting;
class CSeqConversionBase;		// // //

// Sequence editor
class CSequenceEditor : public CWnd
{
	DECLARE_DYNAMIC(CSequenceEditor)
public:
	CSequenceEditor();		// // //
	virtual ~CSequenceEditor();

	BOOL CreateEditor(CWnd *pParentWnd, const RECT &rect);
	void SelectSequence(std::shared_ptr<CSequence> pSequence, int InstrumentType);		// // //
	void SetMaxValues(int MaxVol, int MaxDuty);
	void SetConversion(const CSeqConversionBase &Conv);		// // //

private:
	CWnd *m_pParent = nullptr;
	CDC m_BackDC;		// // //
	CBitmap m_Bitmap;		// // //
	CFont m_cFont;		// // //
	std::unique_ptr<CSizeEditor> m_pSizeEditor;
	std::unique_ptr<CGraphEditor> m_pGraphEditor;
	std::shared_ptr<CSequence> m_pSequence;		// // //
	std::unique_ptr<CSequenceSetting> m_pSetting;
	const CSeqConversionBase *m_pConversion = nullptr;		// // // does not own

	int m_iInstrumentType;
	int m_iMaxVol;
	int m_iMaxDuty;
	int m_iLastIndex = -1;		// // //
	int8_t m_iLastValue = 0;		// // //
private:
	void DestroyGraphEditor();
	void SequenceChangedMessage(bool Changed);
protected:
	DECLARE_MESSAGE_MAP()
public:
	afx_msg void OnPaint();
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);		// // //
	//virtual BOOL PreTranslateMessage(MSG* pMsg);
	virtual BOOL DestroyWindow();
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg LRESULT OnSizeChange(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnCursorChange(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnSequenceChanged(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnSettingChanged(WPARAM wParam, LPARAM lParam);		// // //
};
