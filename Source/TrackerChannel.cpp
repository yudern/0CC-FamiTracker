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

#include "TrackerChannel.h"
#include "Instrument.h"		// // //
#include "APU/Types.h"		// // //

/*
 * This class serves as the interface between the UI and the sound player for each channel
 * Thread synchronization should be done here
 *
 */

CTrackerChannel::CTrackerChannel(LPCTSTR pName, LPCTSTR pShort, sound_chip_t iChip, chan_id_t iID) :		// // //
	m_pShortName(pShort),		// // //
	m_pChannelName(pName),
	m_iChip(iChip),
	m_iChannelID(iID),
	m_bNewNote(false),
	m_iPitch(0),
	m_iNotePriority(NOTE_PRIO_0),
	m_iVolumeMeter(0)
{
}

CTrackerChannel::~CTrackerChannel(void)
{
}

LPCTSTR CTrackerChannel::GetChannelName() const
{
	return m_pChannelName;
}

LPCTSTR CTrackerChannel::GetShortName() const
{
	return m_pShortName;
}

sound_chip_t CTrackerChannel::GetChip() const
{
	return m_iChip;
}

chan_id_t CTrackerChannel::GetID() const		// // //
{
	return m_iChannelID;
}

void CTrackerChannel::SetNote(const stChanNote &Note, note_prio_t Priority)		// // //
{
	m_csNoteLock.Lock();

	if (Priority >= m_iNotePriority) {
		m_Note = Note;
		m_bNewNote = true;
		m_iNotePriority = Priority;
	}

	m_csNoteLock.Unlock();
}

stChanNote CTrackerChannel::GetNote()
{
	m_csNoteLock.Lock();

	stChanNote Note = m_Note;		// // //
	m_Note = stChanNote { };
	m_bNewNote = false;
	m_iNotePriority = NOTE_PRIO_0;

	m_csNoteLock.Unlock();

	return Note;
}

bool CTrackerChannel::NewNoteData() const
{
	return m_bNewNote;
}

void CTrackerChannel::Reset()
{
	m_csNoteLock.Lock();

	m_Note = stChanNote { };		// // //
	m_bNewNote = false;
	m_iVolumeMeter = 0;
	m_iNotePriority = NOTE_PRIO_0;

	m_csNoteLock.Unlock();
}

void CTrackerChannel::SetVolumeMeter(int Value)
{
	m_iVolumeMeter = Value;
}

int CTrackerChannel::GetVolumeMeter() const
{
	return m_iVolumeMeter;
}

void CTrackerChannel::SetPitch(int Pitch)
{
	m_iPitch = Pitch;
}

int CTrackerChannel::GetPitch() const
{
	return m_iPitch;
}

bool CTrackerChannel::IsInstrumentCompatible(int Instrument, inst_type_t Type) const
{
	switch (m_iChip) {
	case sound_chip_t::APU:
	case sound_chip_t::MMC5:
	case sound_chip_t::N163:		// // //
	case sound_chip_t::S5B:
	case sound_chip_t::VRC6:
	case sound_chip_t::FDS:
		switch (Type) {
		case INST_2A03:
		case INST_VRC6:
		case INST_N163:
		case INST_S5B:
		case INST_FDS:
			return true;
		default: return false;
		}
	case sound_chip_t::VRC7:
		return Type == INST_VRC7;
	}

	return false;
}

bool CTrackerChannel::IsEffectCompatible(int EffNumber, int EffParam) const		// // //
{
	switch (EffNumber) {
		case EF_NONE:
		case EF_SPEED: case EF_JUMP: case EF_SKIP: case EF_HALT:
		case EF_DELAY:
			return true;
		case EF_NOTE_CUT: case EF_NOTE_RELEASE:
			return EffParam <= 0x7F || m_iChannelID == chan_id_t::TRIANGLE;
		case EF_GROOVE:
			return EffParam < MAX_GROOVE;
		case EF_VOLUME:
			return ((m_iChip == sound_chip_t::APU && m_iChannelID != chan_id_t::DPCM) || m_iChip == sound_chip_t::MMC5) &&
				(EffParam <= 0x1F || (EffParam >= 0xE0 && EffParam <= 0xE3));
		case EF_PORTAMENTO: case EF_ARPEGGIO: case EF_VIBRATO: case EF_TREMOLO:
		case EF_PITCH: case EF_PORTA_UP: case EF_PORTA_DOWN: case EF_SLIDE_UP: case EF_SLIDE_DOWN:
		case EF_VOLUME_SLIDE: case EF_DELAYED_VOLUME: case EF_TRANSPOSE:
			return m_iChannelID != chan_id_t::DPCM;
		case EF_PORTAOFF:
			return false;
		case EF_SWEEPUP: case EF_SWEEPDOWN:
			return m_iChannelID == chan_id_t::SQUARE1 || m_iChannelID == chan_id_t::SQUARE2;
		case EF_DAC: case EF_SAMPLE_OFFSET: case EF_RETRIGGER: case EF_DPCM_PITCH:
			return m_iChannelID == chan_id_t::DPCM;
		case EF_DUTY_CYCLE:
			return m_iChannelID != chan_id_t::DPCM;		// // // 050B
		case EF_FDS_MOD_DEPTH:
			return m_iChip == sound_chip_t::FDS && (EffParam <= 0x3F || EffParam >= 0x80);
		case EF_FDS_MOD_SPEED_HI: case EF_FDS_MOD_SPEED_LO: case EF_FDS_MOD_BIAS:
			return m_iChip == sound_chip_t::FDS;
		case EF_SUNSOFT_ENV_LO: case EF_SUNSOFT_ENV_HI: case EF_SUNSOFT_ENV_TYPE:
		case EF_SUNSOFT_NOISE:		// // // 050B
			return m_iChip == sound_chip_t::S5B;
		case EF_N163_WAVE_BUFFER:
			return m_iChip == sound_chip_t::N163 && EffParam <= 0x7F;
		case EF_FDS_VOLUME:
			return m_iChip == sound_chip_t::FDS && (EffParam <= 0x7F || EffParam == 0xE0);
		case EF_VRC7_PORT: case EF_VRC7_WRITE:		// // // 050B
			return m_iChip == sound_chip_t::VRC7;
	}

	return false;
}

/*
int CTrackerChannel::GetEffect(int Letter) const
{
	if (m_iChip == sound_chip_t::FDS) {
	}

	return
}
*/
