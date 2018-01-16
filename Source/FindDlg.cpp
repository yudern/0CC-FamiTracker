/*
** FamiTracker - NES/Famicom sound tracker
** Copyright (C) 2005-2015 Jonathan Liss
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

#include "FindDlg.h"
#include <map>
#include "FamiTracker.h"
#include "FamiTrackerDoc.h"
#include "FamiTrackerView.h"
#include "TrackerChannel.h"
#include "MainFrm.h"
#include "SoundGen.h"
#include "PatternEditor.h"
#include "PatternAction.h"
#include "CompoundAction.h"
#include "NumConv.h"

namespace {

enum {
	WC_NOTE = 0,
	WC_OCT,
	WC_INST,
	WC_VOL,
	WC_EFF,
	WC_PARAM
};

} // namespace

searchTerm::searchTerm() :
	Note(std::make_unique<CharRange>()),
	Oct(std::make_unique<CharRange>()),
	Inst(std::make_unique<CharRange>(0, MAX_INSTRUMENTS)),
	Vol(std::make_unique<CharRange>(0, MAX_VOLUME)),
	EffParam(std::make_unique<CharRange>()),
	Definite(),
	EffNumber()
{
}



CFindCursor::CFindCursor(CFamiTrackerDoc *pDoc, int Track, const CCursorPos &Pos, const CSelection &Scope) :
	CPatternIterator(pDoc, Track, Pos),
	m_Scope(Scope.GetNormalized()),
	m_cpBeginPos {Pos}
{
}

void CFindCursor::Move(direction_t Dir)
{
	if (!Contains()) {
		ResetPosition(Dir);
		return;
	}

	switch (Dir) {
	case direction_t::UP: operator-=(1); break;
	case direction_t::DOWN: operator+=(1); break;
	case direction_t::LEFT: --m_iChannel; break;
	case direction_t::RIGHT: ++m_iChannel; break;
	}

	if (Contains()) return;

	switch (Dir) {
	case direction_t::UP:
		m_iFrame = m_Scope.m_cpEnd.m_iFrame;
		m_iRow = m_Scope.m_cpEnd.m_iRow;
		if (--m_iChannel < m_Scope.m_cpStart.m_iChannel)
			m_iChannel = m_Scope.m_cpEnd.m_iChannel;
		break;
	case direction_t::DOWN:
		m_iFrame = m_Scope.m_cpStart.m_iFrame;
		m_iRow = m_Scope.m_cpStart.m_iRow;
		if (++m_iChannel > m_Scope.m_cpEnd.m_iChannel)
			m_iChannel = m_Scope.m_cpStart.m_iChannel;
		break;
	case direction_t::LEFT:
		m_iChannel = m_Scope.m_cpEnd.m_iChannel;
		if (--m_iRow < 0) {
			--m_iFrame;
			m_iRow = m_pDocument->GetCurrentPatternLength(m_iTrack, m_iFrame) - 1;
		}
		if (m_iFrame < m_Scope.m_cpStart.m_iFrame ||
			m_iFrame == m_Scope.m_cpStart.m_iFrame && m_iRow < m_Scope.m_cpStart.m_iRow) {
			m_iFrame = m_Scope.m_cpEnd.m_iFrame;
			m_iRow = m_Scope.m_cpEnd.m_iRow;
		}
		break;
	case direction_t::RIGHT:
		m_iChannel = m_Scope.m_cpStart.m_iChannel;
		if (++m_iRow >= static_cast<int>(m_pDocument->GetCurrentPatternLength(m_iTrack, m_iFrame))) {
			++m_iFrame;
			m_iRow = 0;
		}
		if (m_iFrame > m_Scope.m_cpEnd.m_iFrame ||
			m_iFrame == m_Scope.m_cpEnd.m_iFrame && m_iRow > m_Scope.m_cpEnd.m_iRow) {
			m_iFrame = m_Scope.m_cpStart.m_iFrame;
			m_iRow = m_Scope.m_cpStart.m_iRow;
		}
		break;
	}
}

bool CFindCursor::AtStart() const
{
	const int Frames = m_pDocument->GetFrameCount(m_iTrack);
	return !((m_iFrame - m_cpBeginPos.m_iFrame) % Frames) &&
		m_iRow == m_cpBeginPos.m_iRow && m_iChannel == m_cpBeginPos.m_iChannel;
}

const stChanNote &CFindCursor::Get() const
{
	return CPatternIterator::Get(m_iChannel);
}

void CFindCursor::Set(const stChanNote &Note)
{
	CPatternIterator::Set(m_iChannel, Note);
}

void CFindCursor::ResetPosition(direction_t Dir)
{
	const CCursorPos *Source;
	switch (Dir) {
	case direction_t::DOWN: case direction_t::RIGHT:
		m_cpBeginPos = m_Scope.m_cpEnd; Source = &m_Scope.m_cpStart; break;
	case direction_t::UP: case direction_t::LEFT:
		m_cpBeginPos = m_Scope.m_cpStart; Source = &m_Scope.m_cpEnd; break;
	}
	m_iFrame = Source->m_iFrame;
	m_iRow = Source->m_iRow;
	m_iChannel = Source->m_iChannel;
	ASSERT(Contains());
}

bool CFindCursor::Contains() const
{
	if (m_iChannel < m_Scope.m_cpStart.m_iChannel || m_iChannel > m_Scope.m_cpEnd.m_iChannel)
		return false;

	const int Frames = m_pDocument->GetFrameCount(m_iTrack);
	int Frame = m_iFrame;
	int fStart = m_Scope.m_cpStart.m_iFrame % Frames;
	if (fStart < 0) fStart += Frames;
	int fEnd = m_Scope.m_cpEnd.m_iFrame % Frames;
	if (fEnd < 0) fEnd += Frames;
	Frame %= Frames;
	if (Frame < 0) Frame += Frames;

	bool InStart = Frame > fStart || (Frame == fStart && m_iRow >= m_Scope.m_cpStart.m_iRow);
	bool InEnd = Frame < fEnd || (Frame == fEnd && m_iRow <= m_Scope.m_cpEnd.m_iRow);
	if (fStart > fEnd || (fStart == fEnd && m_Scope.m_cpStart.m_iRow > m_Scope.m_cpEnd.m_iRow))
		return InStart || InEnd;
	else
		return InStart && InEnd;
}



// CFileResultsBox dialog

IMPLEMENT_DYNAMIC(CFindResultsBox, CDialog)

CFindResultsBox::result_column_t CFindResultsBox::m_iLastsortColumn = ID;
bool CFindResultsBox::m_bLastSortDescending = false;
std::unordered_map<std::string, chan_id_t> CFindResultsBox::m_iChannelPositionCache = { };

CFindResultsBox::CFindResultsBox(CWnd* pParent) : CDialog(IDD_FINDRESULTS, pParent)
{
	m_iLastsortColumn = ID;
	m_bLastSortDescending = false;
	m_iChannelPositionCache.clear();
}

void CFindResultsBox::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}

void CFindResultsBox::AddResult(const stChanNote &Note, const CFindCursor &Cursor, bool Noise)
{
	int Pos = m_cListResults.GetItemCount();
	m_cListResults.InsertItem(Pos, conv::sv_from_int(Pos + 1).data());

	const auto pDoc = static_cast<CFamiTrackerDoc*>(((CFrameWnd*)AfxGetMainWnd())->GetActiveDocument());
	m_cListResults.SetItemText(Pos, CHANNEL, pDoc->GetChannel(Cursor.m_iChannel).GetChannelName());
	m_cListResults.SetItemText(Pos, PATTERN, conv::sv_from_int_hex(pDoc->GetPatternAtFrame(
		Cursor.m_iTrack, Cursor.m_iFrame, pDoc->TranslateChannel(Cursor.m_iChannel)), 2).data());

	m_cListResults.SetItemText(Pos, FRAME, conv::sv_from_int_hex(Cursor.m_iFrame, 2).data());
	m_cListResults.SetItemText(Pos, ROW, conv::sv_from_int_hex(Cursor.m_iRow, 2).data());

	switch (Note.Note) {
	case NONE:
		break;
	case HALT:
		m_cListResults.SetItemText(Pos, NOTE, _T("---")); break;
	case RELEASE:
		m_cListResults.SetItemText(Pos, NOTE, _T("===")); break;
	case ECHO:
		m_cListResults.SetItemText(Pos, NOTE, ("^-" + conv::from_int(Note.Octave)).data()); break;
	default:
		if (Noise)
			m_cListResults.SetItemText(Pos, NOTE, (conv::from_int_hex(MIDI_NOTE(Note.Octave, Note.Note) & 0x0F) + "-#").data());
		else
			m_cListResults.SetItemText(Pos, NOTE, Note.ToString().c_str());
	}

	if (Note.Instrument == HOLD_INSTRUMENT)		// // // 050B
		m_cListResults.SetItemText(Pos, INST, _T("&&"));
	else if (Note.Instrument != MAX_INSTRUMENTS)
		m_cListResults.SetItemText(Pos, INST, conv::sv_from_int_hex(Note.Instrument, 2).data());

	if (Note.Vol != MAX_VOLUME)
		m_cListResults.SetItemText(Pos, VOL, conv::sv_from_int_hex(Note.Vol).data());

	for (int i = 0; i < MAX_EFFECT_COLUMNS; ++i)
		if (Note.EffNumber[i] != EF_NONE) {
			auto str = EFF_CHAR[Note.EffNumber[i] - 1] + conv::from_int_hex(Note.EffParam[i], 2);
			m_cListResults.SetItemText(Pos, EFFECT + i, str.data());
		}

	UpdateCount();
}

void CFindResultsBox::ClearResults()
{
	m_cListResults.DeleteAllItems();
	m_iLastsortColumn = ID;
	m_bLastSortDescending = false;
	m_iChannelPositionCache.clear();
	UpdateCount();
}

void CFindResultsBox::SelectItem(int Index)
{
	const auto pDoc = static_cast<CFamiTrackerDoc*>(((CFrameWnd*)AfxGetMainWnd())->GetActiveDocument());

	const auto ToChannelID = [] (std::string_view x) {
		static const std::tuple<std::string_view, sound_chip_t, unsigned> HEADERS[] = {
			{"Pulse "     , sound_chip_t::APU , GetChannelSubIndex(chan_id_t::SQUARE1)},
			{"Triangle"   , sound_chip_t::APU , GetChannelSubIndex(chan_id_t::TRIANGLE)},
			{"Noise"      , sound_chip_t::APU , GetChannelSubIndex(chan_id_t::NOISE)},
			{"DPCM"       , sound_chip_t::APU , GetChannelSubIndex(chan_id_t::DPCM)},
			{"VRC6 Pulse ", sound_chip_t::VRC6, GetChannelSubIndex(chan_id_t::VRC6_PULSE1)},
			{"Sawtooth"   , sound_chip_t::VRC6, GetChannelSubIndex(chan_id_t::VRC6_SAWTOOTH)},
			{"MMC5 Pulse ", sound_chip_t::MMC5, GetChannelSubIndex(chan_id_t::MMC5_SQUARE1)},
			{"Namco "     , sound_chip_t::N163, GetChannelSubIndex(chan_id_t::N163_CH1)},
			{"FDS"        , sound_chip_t::FDS , GetChannelSubIndex(chan_id_t::FDS)},
			{"FM Channel ", sound_chip_t::VRC7, GetChannelSubIndex(chan_id_t::VRC7_CH1)},
			{"5B Square " , sound_chip_t::S5B,  GetChannelSubIndex(chan_id_t::S5B_CH1)},
		};
		for (auto [prefix, chip, subindex] : HEADERS)
			if (0 == x.compare(0, prefix.size(), prefix)) {
				auto s = subindex;
				if (prefix.size() != x.size())
					s += x.back() - '1';
				return MakeChannelIndex(chip, s);
			}
		return chan_id_t::NONE;
	};
	const auto Cache = [&] (const std::string &x) {
		auto it = m_iChannelPositionCache.find(x);
		if (it == m_iChannelPositionCache.end())
			return m_iChannelPositionCache[x] = ToChannelID(x);
		return it->second;
	};

	auto pView = static_cast<CFamiTrackerView*>(((CFrameWnd*)AfxGetMainWnd())->GetActiveView());
	int Channel = pDoc->GetChannelIndex(Cache(m_cListResults.GetItemText(Index, CHANNEL).GetString()));
	if (Channel != -1) pView->SelectChannel(Channel);
	pView->SelectFrame(strtol(m_cListResults.GetItemText(Index, FRAME), nullptr, 16));
	pView->SelectRow(strtol(m_cListResults.GetItemText(Index, ROW), nullptr, 16));
	AfxGetMainWnd()->SetFocus();
}

void CFindResultsBox::UpdateCount() const
{
	int Count = m_cListResults.GetItemCount();
	CString str;
	str.Format(_T("%d"), Count);
	AfxFormatString2(str, IDS_FINDRESULT_COUNT, str, Count == 1 ? _T("result") : _T("results"));
	GetDlgItem(IDC_STATIC_FINDRESULT_COUNT)->SetWindowText(str);
}


BEGIN_MESSAGE_MAP(CFindResultsBox, CDialog)
	ON_NOTIFY(NM_DBLCLK, IDC_LIST_FINDRESULTS, OnNMDblclkListFindresults)
	ON_NOTIFY(LVN_COLUMNCLICK, IDC_LIST_FINDRESULTS, OnLvnColumnClickFindResults)
END_MESSAGE_MAP()


// CFindResultsBox message handlers

BOOL CFindResultsBox::OnInitDialog()
{
	CDialog::OnInitDialog();

	m_cListResults.SubclassDlgItem(IDC_LIST_FINDRESULTS, this);
	m_cListResults.SetExtendedStyle(LVS_EX_GRIDLINES | LVS_EX_FULLROWSELECT);
	CRect r;
	m_cListResults.GetClientRect(&r);
	const int w = r.Width() - ::GetSystemMetrics(SM_CXHSCROLL);

	m_cListResults.InsertColumn(ID, _T("ID"), LVCFMT_LEFT, static_cast<int>(.085 * w));
	m_cListResults.InsertColumn(CHANNEL, _T("Channel"), LVCFMT_LEFT, static_cast<int>(.19 * w));
	m_cListResults.InsertColumn(PATTERN, _T("Pa."), LVCFMT_LEFT, static_cast<int>(.065 * w));
	m_cListResults.InsertColumn(FRAME, _T("Fr."), LVCFMT_LEFT, static_cast<int>(.065 * w));
	m_cListResults.InsertColumn(ROW, _T("Ro."), LVCFMT_LEFT, static_cast<int>(.065 * w));
	m_cListResults.InsertColumn(NOTE, _T("Note"), LVCFMT_LEFT, static_cast<int>(.08 * w));
	m_cListResults.InsertColumn(INST, _T("In."), LVCFMT_LEFT, static_cast<int>(.065 * w));
	m_cListResults.InsertColumn(VOL, _T("Vo."), LVCFMT_LEFT, static_cast<int>(.065 * w));
	for (int i = MAX_EFFECT_COLUMNS; i > 0; --i) {
		CString str;
		str.Format(_T("fx%d"), i);
		m_cListResults.InsertColumn(EFFECT, str, LVCFMT_LEFT, static_cast<int>(.08 * w));
	}

	UpdateCount();

	return TRUE;
}

BOOL CFindResultsBox::PreTranslateMessage(MSG *pMsg)
{
	if (GetFocus() == &m_cListResults) {
		if (pMsg->message == WM_KEYDOWN) {
			switch (pMsg->wParam) {
			case 'A':
				if ((::GetKeyState(VK_CONTROL) & 0x80) == 0x80) {
					m_cListResults.SetRedraw(FALSE);
					for (int i = m_cListResults.GetItemCount() - 1; i >= 0; --i)
						m_cListResults.SetItemState(i, LVIS_SELECTED, LVIS_SELECTED);
					m_cListResults.SetRedraw();
					m_cListResults.RedrawWindow();
				}
				break;
			case VK_DELETE:
				m_cListResults.SetRedraw(FALSE);
				for (int i = m_cListResults.GetItemCount() - 1; i >= 0; --i)
					if (m_cListResults.GetItemState(i, LVIS_SELECTED) == LVIS_SELECTED)
						m_cListResults.DeleteItem(i);
				m_cListResults.SetRedraw();
				m_cListResults.RedrawWindow();
				UpdateCount();
				break;
			case VK_RETURN:
				if (m_cListResults.GetSelectedCount() == 1) {
					POSITION p = m_cListResults.GetFirstSelectedItemPosition();
					ASSERT(p != nullptr);
					SelectItem(m_cListResults.GetNextSelectedItem(p));
				}
				return TRUE;
			}
		}
	}

	if (pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_RETURN) return TRUE;

	return CDialog::PreTranslateMessage(pMsg);
}

void CFindResultsBox::OnNMDblclkListFindresults(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMITEMACTIVATE pNMItemActivate = reinterpret_cast<LPNMITEMACTIVATE>(pNMHDR);
	LVHITTESTINFO lvhti;
	lvhti.pt = pNMItemActivate->ptAction;
	m_cListResults.SubItemHitTest(&lvhti);
	if (lvhti.iItem == -1) return;
	m_cListResults.SetItemState(lvhti.iItem, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
	SelectItem(lvhti.iItem);
}

void CFindResultsBox::OnLvnColumnClickFindResults(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMLISTVIEW pNMListView = reinterpret_cast<LPNMLISTVIEW>(pNMHDR);

	if (m_iLastsortColumn != pNMListView->iSubItem) {
		m_iLastsortColumn = static_cast<result_column_t>(pNMListView->iSubItem);
		m_bLastSortDescending = false;
	}
	else
		m_bLastSortDescending = !m_bLastSortDescending;

	switch (m_iLastsortColumn) {
	case ID:
		m_cListResults.SortItemsEx(IntCompareFunc, (LPARAM)&m_cListResults); break;
	case CHANNEL:
		m_cListResults.SortItemsEx(ChannelCompareFunc, (LPARAM)&m_cListResults); break;
	case NOTE:
		m_cListResults.SortItemsEx(NoteCompareFunc, (LPARAM)&m_cListResults); break;
//	case PATTERN: case FRAME: case ROW: case INST: case VOL:
//		m_cListResults.SortItemsEx(HexCompareFunc, (LPARAM)m_cListResults); break;
	default:
		if (m_iLastsortColumn >= ID && m_iLastsortColumn < EFFECT + MAX_EFFECT_COLUMNS)
			m_cListResults.SortItemsEx(StringCompareFunc, (LPARAM)&m_cListResults);
	}
}

int CFindResultsBox::IntCompareFunc(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort)
{
	CListCtrl *pList = reinterpret_cast<CListCtrl*>(lParamSort);
	long x = strtol(pList->GetItemText(lParam1, m_iLastsortColumn), nullptr, 10);
	long y = strtol(pList->GetItemText(lParam2, m_iLastsortColumn), nullptr, 10);

	int result = 0;
	if (x > y)
		result = 1;
	else if (x < y)
		result = -1;

	return m_bLastSortDescending ? -result : result;
}

int CFindResultsBox::HexCompareFunc(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort)
{
	CListCtrl *pList = reinterpret_cast<CListCtrl*>(lParamSort);
	long x = strtol(pList->GetItemText(lParam1, m_iLastsortColumn), nullptr, 16);
	long y = strtol(pList->GetItemText(lParam2, m_iLastsortColumn), nullptr, 16);

	int result = 0;
	if (x > y)
		result = 1;
	else if (x < y)
		result = -1;

	return m_bLastSortDescending ? -result : result;
}

int CFindResultsBox::StringCompareFunc(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort)
{
	CListCtrl *pList = reinterpret_cast<CListCtrl*>(lParamSort);
	CString x = pList->GetItemText(lParam1, m_iLastsortColumn);
	CString y = pList->GetItemText(lParam2, m_iLastsortColumn);

	int result = x.Compare(y);

	return m_bLastSortDescending ? -result : result;
}

int CFindResultsBox::ChannelCompareFunc(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort)
{
	CListCtrl *pList = reinterpret_cast<CListCtrl*>(lParamSort);
	CString x = pList->GetItemText(lParam1, m_iLastsortColumn);
	CString y = pList->GetItemText(lParam2, m_iLastsortColumn);

	const auto ToIndex = [] (const CString &x) {
		static const CString HEADER_STR[] = {
			_T("Pulse "), _T("Triangle"), _T("Noise"), _T("DPCM"),
			_T("VRC6 Pulse "), _T("Sawtooth"),
			_T("MMC5 Pulse "), _T("Namco "), _T("FDS"), _T("FM Channel "), _T("5B Square ")
		};
		int Pos = 0;
		for (const auto &n : HEADER_STR) {
			int Size = n.GetLength();
			if (x.Left(Size) == n) {
				if (x != n) Pos += x.GetAt(x.GetLength() - 1);
				return Pos;
			}
			Pos += 0x100;
		}
		return -1;
	};
	const auto Cache = [&] (const CString &x) {
		static std::map<CString, int> m;
		auto it = m.find(x);
		if (it == m.end())
			return m[x] = ToIndex(x);
		return it->second;
	};

	int result = Cache(x) - Cache(y);

	return m_bLastSortDescending ? -result : result;
}

int CFindResultsBox::NoteCompareFunc(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort)
{
	CListCtrl *pList = reinterpret_cast<CListCtrl*>(lParamSort);
	CString x = pList->GetItemText(lParam1, m_iLastsortColumn);
	CString y = pList->GetItemText(lParam2, m_iLastsortColumn);

	const auto ToIndex = [] (const CString &x) {
		if (x.Left(1) == _T("^"))
			return 0x400 + x.GetAt(x.GetLength() - 1);
		if (x == _T("==="))
			return 0x300;
		if (x == _T("---"))
			return 0x200;
		if (x.Right(1) == _T("#"))
			return 0x100 + x.GetAt(0);
		for (int i = 0; i < NOTE_RANGE; ++i) {
			const auto &n = stChanNote::NOTE_NAME[i];
			if (n == (LPCTSTR)x.Left(n.size()))
				return MIDI_NOTE(x.GetAt(x.GetLength() - 1) - '0', ++i);
		}
		return -1;
	};
	const auto Cache = [&] (const CString &x) {
		static std::map<CString, int> m;
		auto it = m.find(x);
		if (it == m.end())
			return m[x] = ToIndex(x);
		return it->second;
	};

	int result = Cache(x) - Cache(y);

	return m_bLastSortDescending ? -result : result;
}



// CFindDlg dialog

IMPLEMENT_DYNAMIC(CFindDlg, CDialog)

CFindDlg::CFindDlg(CWnd* pParent /*=NULL*/) : CDialog(CFindDlg::IDD, pParent),
	m_bFound(false),
	m_bSkipFirst(true),
	m_pFindCursor(nullptr),
	m_iSearchDirection(CFindCursor::direction_t::RIGHT)
{
}

CFindDlg::~CFindDlg()
{
}

void CFindDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}


BEGIN_MESSAGE_MAP(CFindDlg, CDialog)
	ON_CONTROL_RANGE(BN_CLICKED, IDC_CHECK_FIND_NOTE, IDC_CHECK_FIND_EFF, OnUpdateFields)
	ON_CONTROL_RANGE(BN_CLICKED, IDC_CHECK_REPLACE_NOTE, IDC_CHECK_REPLACE_EFF, OnUpdateFields)
	ON_CONTROL_RANGE(EN_CHANGE, IDC_EDIT_FIND_NOTE, IDC_EDIT_FIND_EFF, OnUpdateFields)
	ON_CONTROL_RANGE(EN_CHANGE, IDC_EDIT_REPLACE_NOTE, IDC_EDIT_REPLACE_EFF, OnUpdateFields)
	ON_CBN_SELCHANGE(IDC_COMBO_FIND_IN, UpdateFields)
	ON_CBN_SELCHANGE(IDC_COMBO_EFFCOLUMN, UpdateFields)
	ON_BN_CLICKED(IDC_BUTTON_FIND_NEXT, OnBnClickedButtonFindNext)
	ON_BN_CLICKED(IDC_BUTTON_FIND_PREVIOUS, OnBnClickedButtonFindPrevious)
	ON_BN_CLICKED(IDC_BUTTON_FIND_ALL, OnBnClickedButtonFindAll)
	ON_BN_CLICKED(IDC_BUTTON_REPLACE, OnBnClickedButtonReplaceNext)
	ON_BN_CLICKED(IDC_BUTTON_REPLACE_PREVIOUS, OnBnClickedButtonReplacePrevious)
	ON_BN_CLICKED(IDC_BUTTON_FIND_REPLACEALL, OnBnClickedButtonReplaceall)
END_MESSAGE_MAP()


// CFindDlg message handlers

const CString CFindDlg::m_pNoteName[7] = {_T("C"), _T("D"), _T("E"), _T("F"), _T("G"), _T("A"), _T("B")};
const CString CFindDlg::m_pNoteSign[3] = {_T("b"), _T("-"), _T("#")};
const int CFindDlg::m_iNoteOffset[7] = {NOTE_C, NOTE_D, NOTE_E, NOTE_F, NOTE_G, NOTE_A, NOTE_B};



BOOL CFindDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	m_cResultsBox.Create(IDD_FINDRESULTS, this);

	m_cFindNoteField   .SubclassDlgItem(IDC_EDIT_FIND_NOTE, this);
	m_cFindNoteField2  .SubclassDlgItem(IDC_EDIT_FIND_NOTE2, this);
	m_cFindInstField   .SubclassDlgItem(IDC_EDIT_FIND_INST, this);
	m_cFindInstField2  .SubclassDlgItem(IDC_EDIT_FIND_INST2, this);
	m_cFindVolField    .SubclassDlgItem(IDC_EDIT_FIND_VOL, this);
	m_cFindVolField2   .SubclassDlgItem(IDC_EDIT_FIND_VOL2, this);
	m_cFindEffField    .SubclassDlgItem(IDC_EDIT_FIND_EFF, this);
	m_cReplaceNoteField.SubclassDlgItem(IDC_EDIT_REPLACE_NOTE, this);
	m_cReplaceInstField.SubclassDlgItem(IDC_EDIT_REPLACE_INST, this);
	m_cReplaceVolField .SubclassDlgItem(IDC_EDIT_REPLACE_VOL, this);
	m_cReplaceEffField .SubclassDlgItem(IDC_EDIT_REPLACE_EFF, this);
	m_cSearchArea      .SubclassDlgItem(IDC_COMBO_FIND_IN, this);
	m_cEffectColumn    .SubclassDlgItem(IDC_COMBO_EFFCOLUMN, this);

	m_cFindNoteField   .SetLimitText(3);
	m_cFindNoteField2  .SetLimitText(3);
	m_cFindInstField   .SetLimitText(2);
	m_cFindInstField2  .SetLimitText(2);
	m_cFindVolField    .SetLimitText(1);
	m_cFindVolField2   .SetLimitText(1);
	m_cFindEffField    .SetLimitText(3);
	m_cReplaceNoteField.SetLimitText(3);
	m_cReplaceInstField.SetLimitText(2);
	m_cReplaceVolField .SetLimitText(1);
	m_cReplaceEffField .SetLimitText(3);
	m_cSearchArea      .SetCurSel(0);
	m_cEffectColumn    .SetCurSel(0);

	return TRUE;  // return TRUE unless you set the focus to a control
}

void CFindDlg::UpdateFields()
{
	m_cFindNoteField   .EnableWindow(IsDlgButtonChecked(IDC_CHECK_FIND_NOTE));
	m_cFindNoteField2  .EnableWindow(IsDlgButtonChecked(IDC_CHECK_FIND_NOTE));
	m_cFindInstField   .EnableWindow(IsDlgButtonChecked(IDC_CHECK_FIND_INST));
	m_cFindInstField2  .EnableWindow(IsDlgButtonChecked(IDC_CHECK_FIND_INST));
	m_cFindVolField    .EnableWindow(IsDlgButtonChecked(IDC_CHECK_FIND_VOL));
	m_cFindVolField2   .EnableWindow(IsDlgButtonChecked(IDC_CHECK_FIND_VOL));
	m_cFindEffField    .EnableWindow(IsDlgButtonChecked(IDC_CHECK_FIND_EFF));
	m_cReplaceNoteField.EnableWindow(IsDlgButtonChecked(IDC_CHECK_REPLACE_NOTE));
	m_cReplaceInstField.EnableWindow(IsDlgButtonChecked(IDC_CHECK_REPLACE_INST));
	m_cReplaceVolField .EnableWindow(IsDlgButtonChecked(IDC_CHECK_REPLACE_VOL));
	m_cReplaceEffField .EnableWindow(IsDlgButtonChecked(IDC_CHECK_REPLACE_EFF));

	Reset();
	m_bFound = false;
	m_bReplacing = false;
}

void CFindDlg::OnUpdateFields(UINT nID)
{
	UpdateFields();
}

void CFindDlg::ParseNote(searchTerm &Term, CString str, bool Half)
{
	if (!Half) Term.Definite[WC_NOTE] = Term.Definite[WC_OCT] = false;

	if (str.IsEmpty()) {
		if (!Half) {
			Term.Definite[WC_NOTE] = true;
			Term.Definite[WC_OCT] = true;
			Term.Note->Set(NONE);
			Term.Oct->Set(0);
		}
		else {
			//Term.Note->Max = Term.Note->Min;
			//Term.Oct->Max = Term.Oct->Min;
		}
		return;
	}

	RaiseIf(Half && (!Term.Note->IsSingle() || !Term.Oct->IsSingle()),
		_T("Cannot use wildcards in a range search query."));

	if (str == _T("-") || str == _T("---")) {
		RaiseIf(Half, _T("Cannot use note cut in a range search query."));
		Term.Definite[WC_NOTE] = true;
		Term.Definite[WC_OCT] = true;
		Term.Note->Set(HALT);
		Term.Oct->Min = 0; Term.Oct->Max = 7;
		return;
	}

	if (str == _T("=") || str == _T("===")) {
		RaiseIf(Half, _T("Cannot use note release in a range search query."));
		Term.Definite[WC_NOTE] = true;
		Term.Definite[WC_OCT] = true;
		Term.Note->Set(RELEASE);
		Term.Oct->Min = 0; Term.Oct->Max = 7;
		return;
	}

	if (str == _T(".")) {
		RaiseIf(Half, _T("Cannot use wildcards in a range search query."));
		Term.Definite[WC_NOTE] = true;
		Term.Note->Min = NONE + 1;
		Term.Note->Max = ECHO;
		return;
	}

	if (str.GetAt(0) == _T('^')) {
		RaiseIf(Half && !Term.Definite[WC_OCT], _T("Cannot use wildcards in a range search query."));
		Term.Definite[WC_NOTE] = true;
		Term.Definite[WC_OCT] = true;
		Term.Note->Set(ECHO);
		if (str.Delete(0)) {
			if (str.GetAt(0) == _T('-'))
				str.Delete(0);
			int BufPos = conv::to_int((LPCTSTR)str).value_or(ECHO_BUFFER_LENGTH + 1);
			RaiseIf(BufPos > ECHO_BUFFER_LENGTH,
				_T("Echo buffer access \"^%s\" is out of range, maximum is %d."), str, ECHO_BUFFER_LENGTH);
			Term.Oct->Set(BufPos, Half);
		}
		else {
			Term.Oct->Min = 0; Term.Oct->Max = ECHO_BUFFER_LENGTH;
		}
		return;
	}

	if (str.Mid(1, 2) != _T("-#")) for (int i = 0; i < 7; i++) {
		if (str.Left(1).MakeUpper() == m_pNoteName[i]) {
			Term.Definite[WC_NOTE] = true;
			int Note = m_iNoteOffset[i];
			int Oct = 0;
			for (int j = 0; j < 3; j++) if (str[1] == m_pNoteSign[j]) {
				Note += j - 1;
				str.Delete(0); break;
			}
			if (str.Delete(0)) {
				Term.Definite[WC_OCT] = true;
				RaiseIf(str.SpanIncluding("0123456789") != str, _T("Unknown note octave."));
				Oct = conv::to_int((LPCTSTR)str).value_or(-1);
				RaiseIf(Oct >= OCTAVE_RANGE || Oct < 0,
					_T("Note octave \"%s\" is out of range, maximum is %d."), str, OCTAVE_RANGE - 1);
				Term.Oct->Set(Oct, Half);
			}
			else RaiseIf(Half, _T("Cannot use wildcards in a range search query."));
			while (Note > NOTE_RANGE) { Note -= NOTE_RANGE; if (Term.Definite[WC_OCT]) Term.Oct->Set(++Oct, Half); }
			while (Note < NOTE_C) { Note += NOTE_RANGE; if (Term.Definite[WC_OCT]) Term.Oct->Set(--Oct, Half); }
			Term.Note->Set(Note, Half);
			RaiseIf(Term.Definite[WC_OCT] && (Oct >= OCTAVE_RANGE || Oct < 0),
				_T("Note octave \"%s\" is out of range, check if the note contains Cb or B#."), str);
			return;
		}
	}

	if (str.Right(2) == _T("-#") && str.GetLength() == 3) {
		int NoteValue = GetHex(str.Left(1));
		Term.Definite[WC_NOTE] = true;
		Term.Definite[WC_OCT] = true;
		if (str.Left(1) == _T(".")) {
			Term.Note->Min = 1; Term.Note->Max = 4;
			Term.Oct->Min = 0; Term.Oct->Max = 1;
		}
		else {
			Term.Note->Set(NoteValue % NOTE_RANGE + 1, Half);
			Term.Oct->Set(NoteValue / NOTE_RANGE, Half);
		}
		Term.NoiseChan = true;
		return;
	}

	if (str.SpanIncluding("0123456789") == str) {
		int NoteValue = conv::to_int((LPCTSTR)str).value_or(-1);
		RaiseIf(NoteValue == 0 && str.GetAt(0) != _T('0'), _T("Invalid note \"%s\"."), str);
		RaiseIf(NoteValue >= NOTE_COUNT || NoteValue < 0,
			_T("Note value \"%s\" is out of range, maximum is %d."), str, NOTE_COUNT - 1);
		Term.Definite[WC_NOTE] = true;
		Term.Definite[WC_OCT] = true;
		Term.Note->Set(NoteValue % NOTE_RANGE + 1, Half);
		Term.Oct->Set(NoteValue / NOTE_RANGE, Half);
		return;
	}

	RaiseIf(true, _T("Unknown note query."));
}

void CFindDlg::ParseInst(searchTerm &Term, CString str, bool Half)
{
	Term.Definite[WC_INST] = true;
	if (str.IsEmpty()) {
		if (!Half)
			Term.Inst->Set(MAX_INSTRUMENTS);
		return;
	}
	RaiseIf(Half && !Term.Inst->IsSingle(), _T("Cannot use wildcards in a range search query."));

	if (str == _T(".")) {
		RaiseIf(Half, _T("Cannot use wildcards in a range search query."));
		Term.Inst->Min = 0;
		Term.Inst->Max = MAX_INSTRUMENTS - 1;
	}
	else if (str == _T("&&")) {		// // // 050B
		RaiseIf(Half, _T("Cannot use && in a range search query."));
		Term.Inst->Set(HOLD_INSTRUMENT);
	}
	else if (!str.IsEmpty()) {
		unsigned char Val = GetHex(str);
		RaiseIf(Val >= MAX_INSTRUMENTS,
			_T("Instrument \"%s\" is out of range, maximum is %X."), str, MAX_INSTRUMENTS - 1);
		Term.Inst->Set(Val, Half);
	}
}

void CFindDlg::ParseVol(searchTerm &Term, CString str, bool Half)
{
	Term.Definite[WC_VOL] = true;
	if (str.IsEmpty()) {
		if (!Half)
			Term.Vol->Set(MAX_VOLUME);
		return;
	}
	RaiseIf(Half && !Term.Vol->IsSingle(), _T("Cannot use wildcards in a range search query."));

	if (str == _T(".")) {
		RaiseIf(Half, _T("Cannot use wildcards in a range search query."));
		Term.Vol->Min = 0;
		Term.Vol->Max = MAX_VOLUME - 1;
	}
	else if (!str.IsEmpty()) {
		unsigned char Val = GetHex(str);
		RaiseIf(Val >= MAX_VOLUME,
			_T("Channel volume \"%s\" is out of range, maximum is %X."), str, MAX_VOLUME - 1);
		Term.Vol->Set(Val, Half);
	}
}

void CFindDlg::ParseEff(searchTerm &Term, CString str, bool Half)
{
	RaiseIf(str.GetLength() == 2, _T("Effect \"%s\" is too short."), str.Left(1));

	if (str.IsEmpty()) {
		Term.Definite[WC_EFF] = true;
		Term.Definite[WC_PARAM] = true;
		Term.EffNumber[EF_NONE] = true;
		Term.EffParam->Set(0);
	}
	else if (str == _T(".")) {
		Term.Definite[WC_EFF] = true;
		for (size_t i = 1; i < EF_COUNT; i++)
			Term.EffNumber[i] = true;
	}
	else {
		char Name = str[0];
		bool found = false;
		for (size_t i = 1; i < EF_COUNT; i++) {
			if (Name == EFF_CHAR[i - 1]) {
				Term.Definite[WC_EFF] = true;
				Term.EffNumber[i] = true;
				found = true;
			}
		}
		RaiseIf(!found, _T("Unknown effect \"%s\" found in search query."), str.Left(1));
	}
	if (str.GetLength() > 1) {
		Term.Definite[WC_PARAM] = true;
		Term.EffParam->Set(GetHex(str.Right(2)));
	}
}

void CFindDlg::GetFindTerm()
{
	RaiseIf(m_cSearchArea.GetCurSel() == 4 && !m_pView->GetPatternEditor()->IsSelecting(),
			_T("Cannot use \"Selection\" as the search scope if there is no active pattern selection."));

	CString str = _T("");
	searchTerm newTerm;

	if (IsDlgButtonChecked(IDC_CHECK_FIND_NOTE)) {
		m_cFindNoteField.GetWindowText(str);
		bool empty = str.IsEmpty();
		ParseNote(newTerm, str, false);
		m_cFindNoteField2.GetWindowText(str);
		ParseNote(newTerm, str, !empty);
		RaiseIf((newTerm.Note->Min == ECHO && newTerm.Note->Max >= NOTE_C && newTerm.Note->Max <= NOTE_B ||
			newTerm.Note->Max == ECHO && newTerm.Note->Min >= NOTE_C && newTerm.Note->Min <= NOTE_B) &&
			newTerm.Definite[WC_OCT],
			_T("Cannot use both notes and echo buffer in a range search query."));
	}
	if (IsDlgButtonChecked(IDC_CHECK_FIND_INST)) {
		m_cFindInstField.GetWindowText(str);
		bool empty = str.IsEmpty();
		ParseInst(newTerm, str, false);
		m_cFindInstField2.GetWindowText(str);
		ParseInst(newTerm, str, !empty);
	}
	if (IsDlgButtonChecked(IDC_CHECK_FIND_VOL)) {
		m_cFindVolField.GetWindowText(str);
		bool empty = str.IsEmpty();
		ParseVol(newTerm, str, false);
		m_cFindVolField2.GetWindowText(str);
		ParseVol(newTerm, str, !empty);
	}
	if (IsDlgButtonChecked(IDC_CHECK_FIND_EFF)) {
		m_cFindEffField.GetWindowText(str);
		ParseEff(newTerm, str, false);
	}

	for (int i = 0; i <= 6; i++) {
		RaiseIf(i == 6, _T("Search query is empty."));
		if (newTerm.Definite[i]) break;
	}

	m_searchTerm = std::move(newTerm);
}

void CFindDlg::GetReplaceTerm()
{
	CString str = _T("");
	searchTerm newTerm;

	if (IsDlgButtonChecked(IDC_CHECK_REPLACE_NOTE)) {
		m_cReplaceNoteField.GetWindowText(str);
		ParseNote(newTerm, str, false);
	}
	if (IsDlgButtonChecked(IDC_CHECK_REPLACE_INST)) {
		m_cReplaceInstField.GetWindowText(str);
		ParseInst(newTerm, str, false);
	}
	if (IsDlgButtonChecked(IDC_CHECK_REPLACE_VOL)) {
		m_cReplaceVolField.GetWindowText(str);
		ParseVol(newTerm, str, false);
	}
	if (IsDlgButtonChecked(IDC_CHECK_REPLACE_EFF)) {
		m_cReplaceEffField.GetWindowText(str);
		ParseEff(newTerm, str, false);
	}

	for (int i = 0; i <= 6; i++) {
		RaiseIf(i == 6, _T("Replacement query is empty."));
		if (newTerm.Definite[i]) break;
	}

	if ((newTerm.Note->Min == HALT || newTerm.Note->Min == RELEASE) && newTerm.Note->Min == newTerm.Note->Max)
		newTerm.Oct->Min = newTerm.Oct->Max = 0;

	RaiseIf(newTerm.Definite[WC_NOTE] && !newTerm.Note->IsSingle() ||
			newTerm.Definite[WC_OCT] && !newTerm.Oct->IsSingle() ||
			newTerm.Definite[WC_INST] && !newTerm.Inst->IsSingle() ||
			newTerm.Definite[WC_VOL] && !newTerm.Vol->IsSingle() ||
			newTerm.Definite[WC_PARAM] && !newTerm.EffParam->IsSingle(),
			_T("Replacement query cannot contain wildcards."));

	if (IsDlgButtonChecked(IDC_CHECK_FIND_REMOVE)) {
		RaiseIf(newTerm.Definite[WC_NOTE] && !newTerm.Definite[WC_OCT],
				_T("Replacement query cannot contain a note with an unspecified octave if ")
				_T("the option \"Remove original data\" is enabled."));
		RaiseIf(newTerm.Definite[WC_EFF] && !newTerm.Definite[WC_PARAM],
				_T("Replacement query cannot contain an effect with an unspecified parameter if ")
				_T("the option \"Remove original data\" is enabled."));
	}

	m_replaceTerm = toReplace(newTerm);
}

unsigned CFindDlg::GetHex(LPCTSTR str) {
	auto val = conv::to_int(str, 16);
	RaiseIf(!val, _T("Invalid hexadecimal \"%s\"."), str);
	return *val;
}

replaceTerm CFindDlg::toReplace(const searchTerm &x) const
{
	replaceTerm Term;
	Term.Note.Note = x.Note->Min;
	Term.Note.Octave = x.Oct->Min;
	Term.Note.Instrument = x.Inst->Min;
	Term.Note.Vol = x.Vol->Min;
	Term.NoiseChan = x.NoiseChan;
	for (size_t i = 0; i < EF_COUNT; i++)
		if (x.EffNumber[i]) {
			Term.Note.EffNumber[0] = static_cast<effect_t>(i);
			break;
		}
	Term.Note.EffParam[0] = x.EffParam->Min;
	memcpy(Term.Definite, x.Definite, sizeof(bool) * 6);

	return Term;
}

bool CFindDlg::CompareFields(const stChanNote &Target, bool Noise, int EffCount)
{
	int EffColumn = m_cEffectColumn.GetCurSel();
	if (EffColumn > EffCount && EffColumn != 4) EffColumn = EffCount;
	bool Negate = IsDlgButtonChecked(IDC_CHECK_FIND_NEGATE) == BST_CHECKED;
	bool EffectMatch = false;

	bool Melodic = m_searchTerm.Note->Min >= NOTE_C && m_searchTerm.Note->Min <= NOTE_B && // ||
				   m_searchTerm.Note->Max >= NOTE_C && m_searchTerm.Note->Max <= NOTE_B &&
				   m_searchTerm.Definite[WC_OCT];

	if (m_searchTerm.Definite[WC_NOTE]) {
		if (m_searchTerm.NoiseChan) {
			if (!Noise && Melodic) return false;
			if (m_searchTerm.Note->Min < NOTE_C || m_searchTerm.Note->Min > NOTE_B ||
				m_searchTerm.Note->Max < NOTE_C || m_searchTerm.Note->Max > NOTE_B) {
				if (!m_searchTerm.Note->IsMatch(Target.Note)) return Negate;
			}
			else {
				int NoiseNote = MIDI_NOTE(Target.Octave, Target.Note) % 16;
				int Low = MIDI_NOTE(m_searchTerm.Oct->Min, m_searchTerm.Note->Min) % 16;
				int High = MIDI_NOTE(m_searchTerm.Oct->Max, m_searchTerm.Note->Max) % 16;
				if ((NoiseNote < Low && NoiseNote < High) || (NoiseNote > Low && NoiseNote > High))
					return Negate;
			}
		}
		else {
			if (Noise && Melodic) return false;
			if (Melodic) {
				if (Target.Note < NOTE_C || Target.Note > NOTE_B)
					return Negate;
				int NoteValue = MIDI_NOTE(Target.Octave, Target.Note);
				int Low = MIDI_NOTE(m_searchTerm.Oct->Min, m_searchTerm.Note->Min);
				int High = MIDI_NOTE(m_searchTerm.Oct->Max, m_searchTerm.Note->Max);
				if ((NoteValue < Low && NoteValue < High) || (NoteValue > Low && NoteValue > High))
					return Negate;
			}
			else {
				if (!m_searchTerm.Note->IsMatch(Target.Note)) return Negate;
				if (m_searchTerm.Definite[WC_OCT] && !m_searchTerm.Oct->IsMatch(Target.Octave))
					return Negate;
			}
		}
	}
	if (m_searchTerm.Definite[WC_INST] && !m_searchTerm.Inst->IsMatch(Target.Instrument)) return Negate;
	if (m_searchTerm.Definite[WC_VOL] && !m_searchTerm.Vol->IsMatch(Target.Vol)) return Negate;
	int Limit = MAX_EFFECT_COLUMNS - 1;
	if (EffCount < Limit) Limit = EffCount;
	if (EffColumn < Limit) Limit = EffColumn;
	for (int i = EffColumn % MAX_EFFECT_COLUMNS; i <= Limit; i++) {
		if ((!m_searchTerm.Definite[WC_EFF] || m_searchTerm.EffNumber[Target.EffNumber[i]])
		&& (!m_searchTerm.Definite[WC_PARAM] || m_searchTerm.EffParam->IsMatch(Target.EffParam[i])))
			EffectMatch = true;
	}
	if (!EffectMatch) return Negate;

	return !Negate;
}

template <typename... T>
void CFindDlg::RaiseIf(bool Check, LPCTSTR Str, T... args)
{
	if (!Check)
		return;
	TCHAR buf[512];
	_sntprintf_s(buf, sizeof(buf), _TRUNCATE, Str, args...);
	throw CFindException {buf};
}

bool CFindDlg::Find(bool ShowEnd)
{
	const int Track = static_cast<CMainFrame*>(AfxGetMainWnd())->GetSelectedTrack();
	const int Frames = m_pDocument->GetFrameCount(Track);

	if (ShowEnd)
		m_pFindCursor.reset();
	PrepareCursor(false);
	if (!m_bFound) {
		if (!m_pFindCursor->Contains())
			m_pFindCursor->ResetPosition(m_iSearchDirection);
		m_bSkipFirst = false;
	}

	do {
		if (m_bSkipFirst) {
			m_bSkipFirst = false;
			m_pFindCursor->Move(m_iSearchDirection);
		}
		const auto &Target = m_pFindCursor->Get();
		if (CompareFields(Target, m_pDocument->TranslateChannel(m_pFindCursor->m_iChannel) == chan_id_t::NOISE,
							m_pDocument->GetEffColumns(Track, m_pDocument->TranslateChannel(m_pFindCursor->m_iChannel)))) {
			auto pCursor = std::move(m_pFindCursor);
			m_pView->SelectFrame(pCursor->m_iFrame % Frames);
			m_pView->SelectRow(pCursor->m_iRow);
			m_pView->SelectChannel(pCursor->m_iChannel);
			m_pFindCursor = std::move(pCursor);
			m_bSkipFirst = true;
			return m_bFound = true;
		}
		m_pFindCursor->Move(m_iSearchDirection);
	} while (!m_pFindCursor->AtStart());

	if (ShowEnd)
		AfxMessageBox(IDS_FIND_NONE, MB_ICONINFORMATION);
	m_bSkipFirst = true;
	return m_bFound = false;
}

bool CFindDlg::Replace(CCompoundAction *pAction)
{
	if (m_bFound) {
		ASSERT(m_pFindCursor != nullptr);

		stChanNote Target;
		if (!IsDlgButtonChecked(IDC_CHECK_FIND_REMOVE))
			Target = m_pFindCursor->Get();

		if (m_replaceTerm.Definite[WC_NOTE])
			Target.Note = m_replaceTerm.Note.Note;

		if (m_replaceTerm.Definite[WC_OCT])
			Target.Octave = m_replaceTerm.Note.Octave;

		if (m_replaceTerm.Definite[WC_INST])
			Target.Instrument = m_replaceTerm.Note.Instrument;

		if (m_replaceTerm.Definite[WC_VOL])
			Target.Vol = m_replaceTerm.Note.Vol;

		if (m_replaceTerm.Definite[WC_EFF] || m_replaceTerm.Definite[WC_PARAM]) {
			std::vector<int> MatchedColumns;
			if (m_cEffectColumn.GetCurSel() < MAX_EFFECT_COLUMNS)
				MatchedColumns.push_back(m_cEffectColumn.GetCurSel());
			else {
				const int c = m_pDocument->GetEffColumns(m_pFindCursor->m_iTrack, m_pDocument->TranslateChannel(m_pFindCursor->m_iChannel));
				for (int i = 0; i <= c; ++i)
					if ((!m_searchTerm.Definite[WC_EFF] || m_searchTerm.EffNumber[Target.EffNumber[i]]) &&
						(!m_searchTerm.Definite[WC_PARAM] || m_searchTerm.EffParam->IsMatch(Target.EffParam[i])))
						MatchedColumns.push_back(i);
			}

			if (m_replaceTerm.Definite[WC_EFF]) {
				effect_t fx = GetEffectFromChar(EFF_CHAR[m_replaceTerm.Note.EffNumber[0] - 1],
												m_pDocument->GetChipType(m_pFindCursor->m_iChannel));
				for (const int &i : MatchedColumns)
					Target.EffNumber[i] = fx;
			}

			if (m_replaceTerm.Definite[WC_PARAM])
				for (const int &i : MatchedColumns)
					Target.EffParam[i] = m_replaceTerm.Note.EffParam[0];
		}

		if (pAction)
			pAction->JoinAction(std::make_unique<CPActionReplaceNote>(Target,
								m_pFindCursor->m_iFrame, m_pFindCursor->m_iRow, m_pFindCursor->m_iChannel));
		else
			m_pView->EditReplace(Target);
		m_bFound = false;
		return true;
	}
	else {
		m_bSkipFirst = false;
		return false;
	}
}

bool CFindDlg::PrepareFind()
{
	m_pDocument = static_cast<CFamiTrackerDoc*>(((CFrameWnd*)AfxGetMainWnd())->GetActiveDocument());
	m_pView = static_cast<CFamiTrackerView*>(((CFrameWnd*)AfxGetMainWnd())->GetActiveView());
	if (!m_pDocument || !m_pView) {
		AfxMessageBox(_T("Unknown error."), MB_ICONERROR); return false;
	}

	try {
		GetFindTerm();
	}
	catch (CFindException e) {
		AfxMessageBox(e.what(), MB_OK | MB_ICONSTOP);
		return false;
	}

	return true;
}

bool CFindDlg::PrepareReplace()
{
	if (!PrepareFind()) return false;

	try {
		GetReplaceTerm();
	}
	catch (CFindException e) {
		AfxMessageBox(e.what(), MB_OK | MB_ICONSTOP);
		return false;
	}

	return (m_pView->GetEditMode() && !(theApp.GetSoundGenerator()->IsPlaying() && m_pView->GetFollowMode()));
}

void CFindDlg::PrepareCursor(bool ReplaceAll)
{
	if (ReplaceAll)
		Reset();
	if (m_pFindCursor)
		return;

	const int Track = static_cast<CMainFrame*>(AfxGetMainWnd())->GetSelectedTrack();
	const int Frames = m_pDocument->GetFrameCount(Track);
	const CPatternEditor *pEditor = m_pView->GetPatternEditor();
	CCursorPos Cursor = pEditor->GetCursor();
	CSelection Scope;

	if (m_cSearchArea.GetCurSel() == 4) { // Selection
		Scope = pEditor->GetSelection().GetNormalized();
		if (Scope.m_cpStart.m_iFrame < 0 || Scope.m_cpEnd.m_iFrame < 0) {
			Scope.m_cpStart.m_iFrame += Frames;
			Scope.m_cpEnd.m_iFrame += Frames;
		}
	}
	else {
		switch (m_cSearchArea.GetCurSel()) {
		case 0: case 1: // Track, Channel
			Scope.m_cpStart.m_iFrame = 0;
			Scope.m_cpEnd.m_iFrame = Frames - 1; break;
		case 2: case 3: // Frame, Pattern
			Scope.m_cpStart.m_iFrame = Scope.m_cpEnd.m_iFrame = Cursor.m_iFrame; break;
		}

		switch (m_cSearchArea.GetCurSel()) {
		case 0: case 2: // Track, Frame
			Scope.m_cpStart.m_iChannel = 0;
			Scope.m_cpEnd.m_iChannel = m_pDocument->GetChannelCount() - 1; break;
		case 1: case 3: // Channel, Pattern
			Scope.m_cpStart.m_iChannel = Scope.m_cpEnd.m_iChannel = Cursor.m_iChannel; break;
		}

		Scope.m_cpStart.m_iRow = 0;
		Scope.m_cpEnd.m_iRow = pEditor->GetCurrentPatternLength(Scope.m_cpEnd.m_iFrame) - 1;
	}
	m_pFindCursor = std::make_unique<CFindCursor>(m_pDocument, Track, ReplaceAll ? Scope.m_cpStart : Cursor, Scope);
}

void CFindDlg::OnBnClickedButtonFindNext()
{
	if (!PrepareFind()) return;

	m_iSearchDirection = IsDlgButtonChecked(IDC_CHECK_VERTICAL_SEARCH) ?
		CFindCursor::direction_t::DOWN : CFindCursor::direction_t::RIGHT;
	Find(true);
	m_pView->SetFocus();
}

void CFindDlg::OnBnClickedButtonFindPrevious()
{
	if (!PrepareFind()) return;

	m_iSearchDirection = IsDlgButtonChecked(IDC_CHECK_VERTICAL_SEARCH) ?
		CFindCursor::direction_t::UP : CFindCursor::direction_t::LEFT;
	Find(true);
	m_pView->SetFocus();
}

void CFindDlg::OnBnClickedButtonReplaceNext()
{
	if (!PrepareReplace()) return;

	m_iSearchDirection = IsDlgButtonChecked(IDC_CHECK_VERTICAL_SEARCH) ?
		CFindCursor::direction_t::DOWN : CFindCursor::direction_t::RIGHT;
	if (!m_bReplacing)
		Reset();
	Replace();
	bool Found = Find(false);
	m_bReplacing = true;

	auto pCursor = std::move(m_pFindCursor);
	m_pView->SetFocus();
	m_pFindCursor = std::move(pCursor);
	m_bFound = Found;
}

void CFindDlg::OnBnClickedButtonReplacePrevious()
{
	if (!PrepareReplace()) return;

	m_iSearchDirection = IsDlgButtonChecked(IDC_CHECK_VERTICAL_SEARCH) ?
		CFindCursor::direction_t::UP : CFindCursor::direction_t::LEFT;
	if (!m_bReplacing)
		Reset();
	Replace();
	bool Found = Find(false);
	m_bReplacing = true;

	auto pCursor = std::move(m_pFindCursor);
	m_pView->SetFocus();
	m_pFindCursor = std::move(pCursor);
	m_bFound = Found;
}

void CFindDlg::OnBnClickedButtonFindAll()
{
	if (!PrepareFind()) return;

	const int Track = static_cast<CMainFrame*>(AfxGetMainWnd())->GetSelectedTrack();
	m_iSearchDirection = IsDlgButtonChecked(IDC_CHECK_VERTICAL_SEARCH) ?
		CFindCursor::direction_t::DOWN : CFindCursor::direction_t::RIGHT;

	PrepareCursor(true);
	m_cResultsBox.SetRedraw(FALSE);
	m_cResultsBox.ClearResults();
	do {
		const auto &Target = m_pFindCursor->Get();
		chan_id_t ch = m_pDocument->TranslateChannel(m_pFindCursor->m_iChannel);
		if (CompareFields(Target, ch == chan_id_t::NOISE, m_pDocument->GetEffColumns(Track, ch)))
			m_cResultsBox.AddResult(Target, *m_pFindCursor, ch == chan_id_t::NOISE);
		m_pFindCursor->Move(m_iSearchDirection);
	} while (!m_pFindCursor->AtStart());

	m_cResultsBox.SetRedraw();
	m_cResultsBox.ShowWindow(SW_SHOW);
	m_cResultsBox.RedrawWindow();
	m_cResultsBox.SetFocus();
}

void CFindDlg::OnBnClickedButtonReplaceall()
{
	if (!PrepareReplace()) return;

	const int Track = static_cast<CMainFrame*>(AfxGetMainWnd())->GetSelectedTrack();
	unsigned int Count = 0;

	m_iSearchDirection = IsDlgButtonChecked(IDC_CHECK_VERTICAL_SEARCH) ?
		CFindCursor::direction_t::DOWN : CFindCursor::direction_t::RIGHT;

	auto pAction = std::make_unique<CCompoundAction>();
	PrepareCursor(true);
	do {
		const auto &Target = m_pFindCursor->Get();
		chan_id_t ch = m_pDocument->TranslateChannel(m_pFindCursor->m_iChannel);
		if (CompareFields(Target, ch == chan_id_t::NOISE, m_pDocument->GetEffColumns(Track, ch))) {
			m_bFound = true;
			Replace(static_cast<CCompoundAction *>(pAction.get()));
			++Count;
		}
		m_pFindCursor->Move(m_iSearchDirection);
	} while (!m_pFindCursor->AtStart());

	static_cast<CMainFrame*>(AfxGetMainWnd())->AddAction(std::move(pAction));
	m_pView->SetFocus();
	CString str;
	str.Format(_T("%d occurrence(s) replaced."), Count);
	AfxMessageBox(str, MB_OK | MB_ICONINFORMATION);
}

void CFindDlg::Reset()
{
	m_bFound = false;
	m_pFindCursor.reset();
}
