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

// This file handles playing of 2A03 channels

#include "Channels2A03.h"
#include "APU/Types.h"		// // //
#include "APU/APUInterface.h"		// // //
#include "APU/2A03.h"		// // // for DPCM
#include "ft0cc/doc/dpcm_sample.hpp"		// // //
#include "Instrument.h"		// // //
#include "InstHandler.h"		// // //
#include "SeqInstHandler.h"		// // //
#include "InstHandlerDPCM.h"		// // //
#include "SongState.h"		// // //
#ifndef FT0CC_EXT_BUILD
#include "FamiTrackerEnv.h"		// // //
#include "Settings.h"
#include "SoundGen.h"		// // //
#endif

//#define NOISE_PITCH_SCALE

CChannelHandler2A03::CChannelHandler2A03(stChannelID ch) :		// // //
	CChannelHandler(ch, 0x7FF, 0x0F),
	m_bHardwareEnvelope(false),
	m_bEnvelopeLoop(true),
	m_bResetEnvelope(false),
	m_iLengthCounter(1)
{
}

void CChannelHandler2A03::HandleNoteData(stChanNote &NoteData)		// // //
{
	// // //
	CChannelHandler::HandleNoteData(NoteData);

	if (is_note(NoteData.Note) || NoteData.Note == note_t::echo) {
		if (!m_bEnvelopeLoop || m_bHardwareEnvelope)		// // //
			m_bResetEnvelope = true;
	}
}

bool CChannelHandler2A03::HandleEffect(stEffectCommand cmd)
{
	switch (cmd.fx) {
	case effect_t::VOLUME:
		if (cmd.param < 0x20) {		// // //
			m_iLengthCounter = cmd.param;
			m_bEnvelopeLoop = false;
			m_bResetEnvelope = true;
		}
		else if (cmd.param >= 0xE0 && cmd.param < 0xE4) {
			if (!m_bEnvelopeLoop || !m_bHardwareEnvelope)
				m_bResetEnvelope = true;
			m_bHardwareEnvelope = ((cmd.param & 0x01) == 0x01);
			m_bEnvelopeLoop = ((cmd.param & 0x02) != 0x02);
		}
		break;
	case effect_t::DUTY_CYCLE:
		m_iDefaultDuty = m_iDutyPeriod = cmd.param;
		break;
	default: return CChannelHandler::HandleEffect(cmd);
	}

	return true;
}

void CChannelHandler2A03::HandleEmptyNote()
{
	// // //
}

void CChannelHandler2A03::HandleCut()
{
	CutNote();
}

void CChannelHandler2A03::HandleRelease()
{
	if (!m_bRelease)
		ReleaseNote();
/*
	if (!m_bSweeping && (m_cSweep != 0 || m_iSweep != 0)) {
		m_iSweep = 0;
		m_cSweep = 0;
		m_iLastPeriod = 0xFFFF;
	}
	else if (m_bSweeping) {
		m_cSweep = m_iSweep;
		m_iLastPeriod = 0xFFFF;
	}
	*/
}

bool CChannelHandler2A03::CreateInstHandler(inst_type_t Type)
{
	switch (Type) {
	case INST_2A03: case INST_VRC6: case INST_N163: case INST_S5B: case INST_FDS:
		switch (m_iInstTypeCurrent) {
		case INST_2A03: case INST_VRC6: case INST_N163: case INST_S5B: case INST_FDS: break;
		default:
			m_pInstHandler = std::make_unique<CSeqInstHandler>(this, 0x0F, Type == INST_S5B ? 0x40 : 0);
			return true;
		}
	}
	return false;
}

void CChannelHandler2A03::ResetChannel()
{
	CChannelHandler::ResetChannel();
	m_bEnvelopeLoop = true;		// // //
	m_bHardwareEnvelope = false;
	m_iLengthCounter = 1;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////
// // // 2A03 Square
///////////////////////////////////////////////////////////////////////////////////////////////////////////

C2A03Square::C2A03Square(stChannelID ch) :		// // //
	CChannelHandler2A03(ch),
	m_cSweep(0),
	m_bSweeping(0),
	m_iSweep(0)
{
}

void C2A03Square::RefreshChannel()
{
	int Period = CalculatePeriod();
	int Volume = CalculateVolume();
	char DutyCycle = (m_iDutyPeriod & 0x03);

	unsigned char HiFreq = (Period & 0xFF);
	unsigned char LoFreq = (Period >> 8);

	int Address = 0x4000 + GetChannelID().Subindex * 4;		// // //
	if (m_bGate)		// // //
		m_pAPU->Write(Address, (DutyCycle << 6) | (m_bEnvelopeLoop << 5) | (!m_bHardwareEnvelope << 4) | Volume);		// // //
	else {
		m_pAPU->Write(Address, 0x30);
		m_iLastPeriod = 0xFFFF;
		return;
	}

	if (m_cSweep) {
		if (m_cSweep & 0x80) {
			m_pAPU->Write(Address + 1, m_cSweep);
			m_cSweep &= 0x7F;
			m_pAPU->Write(0x4017, 0x80);	// Clear sweep unit
			m_pAPU->Write(0x4017, 0x00);
			m_pAPU->Write(Address + 2, HiFreq);
			m_pAPU->Write(Address + 3, LoFreq + (m_iLengthCounter << 3));		// // //
			m_iLastPeriod = 0xFFFF;
		}
	}
	else {
		m_pAPU->Write(Address + 1, 0x08);
		//m_pAPU->Write(0x4017, 0x80);	// Manually execute one APU frame sequence to kill the sweep unit
		//m_pAPU->Write(0x4017, 0x00);
		m_pAPU->Write(Address + 2, HiFreq);

		if (LoFreq != (m_iLastPeriod >> 8) || m_bResetEnvelope)		// // //
			m_pAPU->Write(Address + 3, LoFreq + (m_iLengthCounter << 3));
	}

	m_iLastPeriod = Period;
	m_bResetEnvelope = false;		// // //
}

int C2A03Square::ConvertDuty(int Duty) const		// // //
{
	switch (m_iInstTypeCurrent) {
	case INST_VRC6:	return DUTY_2A03_FROM_VRC6[Duty & 0x07];
	case INST_S5B:	return 0x02;
	default:		return Duty;
	}
}

void C2A03Square::ClearRegisters()
{
	int Address = 0x4000 + GetChannelID().Subindex * 4;		// // //
	m_pAPU->Write(Address + 0, 0x30);
	m_pAPU->Write(Address + 1, 0x08);
	m_pAPU->Write(Address + 2, 0x00);
	m_pAPU->Write(Address + 3, 0x00);
	m_iLastPeriod = 0xFFFF;
}

void C2A03Square::HandleNoteData(stChanNote &NoteData)		// // //
{
	m_iSweep = 0;
	m_bSweeping = false;
	CChannelHandler2A03::HandleNoteData(NoteData);
}

bool C2A03Square::HandleEffect(stEffectCommand cmd)
{
	switch (cmd.fx) {
	case effect_t::SWEEPUP:
		m_iSweep = 0x88 | (cmd.param & 0x77);
		m_iLastPeriod = 0xFFFF;
		m_bSweeping = true;
		break;
	case effect_t::SWEEPDOWN:
		m_iSweep = 0x80 | (cmd.param & 0x77);
		m_iLastPeriod = 0xFFFF;
		m_bSweeping = true;
		break;
	default: return CChannelHandler2A03::HandleEffect(cmd);
	}

	return true;
}

void C2A03Square::HandleEmptyNote()
{
	if (m_bSweeping)
		m_cSweep = m_iSweep;
}

void C2A03Square::HandleNote(int MidiNote)		// // //
{
	CChannelHandler2A03::HandleNote(MidiNote);

	if (!m_bSweeping && (m_cSweep != 0 || m_iSweep != 0)) {
		m_iSweep = 0;
		m_cSweep = 0;
		m_iLastPeriod = 0xFFFF;
	}
	else if (m_bSweeping) {
		m_cSweep = m_iSweep;
		m_iLastPeriod = 0xFFFF;
	}
}

std::string C2A03Square::GetCustomEffectString() const		// // //
{
	std::string str;

	if (!m_bEnvelopeLoop)
		str += MakeCommandString({effect_t::VOLUME, static_cast<uint8_t>(m_iLengthCounter)});
	if (!m_bEnvelopeLoop || m_bHardwareEnvelope)
		str += MakeCommandString({effect_t::VOLUME, static_cast<uint8_t>(0xE0 + !m_bEnvelopeLoop * 2 + m_bHardwareEnvelope)});

	return str;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////
// Triangle
///////////////////////////////////////////////////////////////////////////////////////////////////////////

CTriangleChan::CTriangleChan(stChannelID ch) :		// // //
	CChannelHandler2A03(ch),
	m_iLinearCounter(-1)
{
}

void CTriangleChan::RefreshChannel()
{
	int Freq = CalculatePeriod();

	unsigned char HiFreq = (Freq & 0xFF);
	unsigned char LoFreq = (Freq >> 8);

	if (m_iInstVolume > 0 && m_iVolume > 0 && m_bGate) {
		m_pAPU->Write(0x4008, (m_bEnvelopeLoop << 7) | (m_iLinearCounter & 0x7F));		// // //
		m_pAPU->Write(0x400A, HiFreq);
		if (m_bEnvelopeLoop || m_bResetEnvelope)		// // //
			m_pAPU->Write(0x400B, LoFreq + (m_iLengthCounter << 3));
	}
	else
		m_pAPU->Write(0x4008, 0);

	m_bResetEnvelope = false;		// // //
}

void CTriangleChan::ResetChannel()
{
	CChannelHandler2A03::ResetChannel();
	m_iLinearCounter = -1;
}

int CTriangleChan::GetChannelVolume() const
{
	return m_iVolume ? VOL_COLUMN_MAX : 0;
}

bool CTriangleChan::HandleEffect(stEffectCommand cmd)
{
	switch (cmd.fx) {
	case effect_t::VOLUME:
		if (cmd.param < 0x20) {		// // //
			m_iLengthCounter = cmd.param;
			m_bEnvelopeLoop = false;
			m_bResetEnvelope = true;
			if (m_iLinearCounter == -1)	m_iLinearCounter = 0x7F;
		}
		else if (cmd.param >= 0xE0 && cmd.param < 0xE4) {
			if (!m_bEnvelopeLoop)
				m_bResetEnvelope = true;
			m_bEnvelopeLoop = ((cmd.param & 0x01) != 0x01);
		}
		break;
	case effect_t::NOTE_CUT:
		if (cmd.param >= 0x80) {
			m_iLinearCounter = cmd.param - 0x80;
			m_bEnvelopeLoop = false;
			m_bResetEnvelope = true;
		}
		else {
			m_bEnvelopeLoop = true;
			return CChannelHandler2A03::HandleEffect(cmd); // true
		}
		break;
	default: return CChannelHandler2A03::HandleEffect(cmd);
	}

	return true;
}

void CTriangleChan::ClearRegisters()
{
	m_pAPU->Write(0x4008, 0);
	m_pAPU->Write(0x400A, 0);
	m_pAPU->Write(0x400B, 0);
}

std::string CTriangleChan::GetCustomEffectString() const		// // //
{
	std::string str;

	if (m_iLinearCounter > -1)
		str += MakeCommandString({effect_t::NOTE_CUT, static_cast<uint8_t>(m_iLinearCounter | 0x80)});
	if (!m_bEnvelopeLoop)
		str += MakeCommandString({effect_t::VOLUME, static_cast<uint8_t>(m_iLengthCounter)});
	if (!m_bEnvelopeLoop)
		str += MakeCommandString({effect_t::VOLUME, static_cast<uint8_t>(0xE0 + !m_bEnvelopeLoop)});

	return str;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////
// Noise
///////////////////////////////////////////////////////////////////////////////////////////////////////////

void CNoiseChan::HandleNote(int MidiNote)
{
	CChannelHandler2A03::HandleNote(MidiNote);		// // //

	int NewNote = (MidiNote & 0x0F) | 0x100;
	int NesFreq = TriggerNote(NewNote);

	// // // NesFreq = (NesFreq & 0x0F) | 0x10;

	// // // NewNote &= 0x0F;

	if (m_iPortaSpeed > 0 && m_iEffect == effect_t::PORTAMENTO) {
		if (m_iPeriod == 0)
			m_iPeriod = NesFreq;
		m_iPortaTo = NesFreq;
	}
	else
		m_iPeriod = NesFreq;

	m_bGate = true;

	m_iNote = NewNote;
}

void CNoiseChan::SetupSlide()		// // //
{
	const auto GetSlideSpeed = [] (unsigned char param) {
		return ((param & 0xF0) >> 3) + 1;
	};

	switch (m_iEffect) {
	case effect_t::PORTAMENTO:
		m_iPortaSpeed = m_iEffectParam;
		break;
	case effect_t::SLIDE_UP:
		m_iNote += m_iEffectParam & 0xF;
		m_iPortaSpeed = GetSlideSpeed(m_iEffectParam);
		break;
	case effect_t::SLIDE_DOWN:
		m_iNote -= m_iEffectParam & 0xF;
		m_iPortaSpeed = GetSlideSpeed(m_iEffectParam);
		break;
	}

	m_iPortaTo = m_iNote;
}

int CNoiseChan::LimitPeriod(int Period) const		// // //
{
	return Period; // no limit
}

int CNoiseChan::LimitRawPeriod(int Period) const		// // //
{
	return Period; // no limit
}

/*
int CNoiseChan::CalculatePeriod() const
{
	return LimitPeriod(m_iPeriod - GetVibrato() + GetFinePitch() + GetPitch());
}
*/

CNoiseChan::CNoiseChan(stChannelID ch) : CChannelHandler2A03(ch)		// // //
{
}

void CNoiseChan::RefreshChannel()
{
	int Period = CalculatePeriod();
	int Volume = CalculateVolume();
	char NoiseMode = (m_iDutyPeriod & 0x01) << 7;

#ifdef NOISE_PITCH_SCALE
	Period = (Period >> 4) & 0x0F;
#else
	Period = Period & 0x0F;
#endif

	Period ^= 0x0F;

	if (m_bGate)		// // //
		m_pAPU->Write(0x400C, (m_bEnvelopeLoop << 5) | (!m_bHardwareEnvelope << 4) | Volume);		// // //
	else {
		m_pAPU->Write(0x400C, 0x30);
		return;
	}
	m_pAPU->Write(0x400E, NoiseMode | Period);
	if (m_bEnvelopeLoop || m_bResetEnvelope)		// // //
		m_pAPU->Write(0x400F, m_iLengthCounter << 3);

	m_bResetEnvelope = false;		// // //
}

void CNoiseChan::ClearRegisters()
{
	m_pAPU->Write(0x400C, 0x30);
	m_pAPU->Write(0x400E, 0);
	m_pAPU->Write(0x400F, 0);
}

std::string CNoiseChan::GetCustomEffectString() const		// // //
{
	std::string str;

	if (!m_bEnvelopeLoop)
		str += MakeCommandString({effect_t::VOLUME, static_cast<uint8_t>(m_iLengthCounter)});
	if (!m_bEnvelopeLoop || m_bHardwareEnvelope)
		str += MakeCommandString({effect_t::VOLUME, static_cast<uint8_t>(0xE0 + !m_bEnvelopeLoop * 2 + m_bHardwareEnvelope)});

	return str;
}

int CNoiseChan::TriggerNote(int Note)
{
	// Clip range to 0-15
	/*
	if (Note > 0x0F)
		Note = 0x0F;
	if (Note < 0)
		Note = 0;
		*/

	RegisterKeyState(Note);

//	Note &= 0x0F;

#ifdef NOISE_PITCH_SCALE
	return (Note ^ 0x0F) << 4;
#else
	return Note | 0x100;		// // //
#endif
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////
// DPCM
///////////////////////////////////////////////////////////////////////////////////////////////////////////

CDPCMChan::CDPCMChan(stChannelID ch) :		// // //
	CChannelHandler(ch, 0xF, 0x3F)		// // // does not use these anyway
{
}

void CDPCMChan::HandleNoteData(stChanNote &NoteData)		// // //
{
	m_iCustomPitch = -1;
	m_iRetrigger = 0;

	if (NoteData.Note != note_t::none) {
		m_iNoteCut = 0;
		m_iNoteRelease = 0;			// // //
	}

	CChannelHandler::HandleNoteData(NoteData);
}

bool CDPCMChan::HandleEffect(stEffectCommand cmd)
{
	switch (cmd.fx) {
	case effect_t::DAC:
		m_cDAC = cmd.param & 0x7F;
		break;
	case effect_t::SAMPLE_OFFSET:
		m_iOffset = cmd.param & 0x3F;		// // //
		break;
	case effect_t::DPCM_PITCH:
		m_iCustomPitch = cmd.param & 0x0F;		// // //
		break;
	case effect_t::RETRIGGER:
//		if (NoteData->cmd.param[i] > 0) {
			m_iRetrigger = cmd.param + 1;
			if (m_iRetriggerCntr == 0)
				m_iRetriggerCntr = m_iRetrigger;
//		}
//		m_iEnableRetrigger = 1;
		break;
	case effect_t::NOTE_CUT:
	case effect_t::NOTE_RELEASE:
		return CChannelHandler::HandleEffect(cmd);
	default: return false; // unless WAVE_CHAN analog for CChannelHandler exists
	}

	return true;
}

void CDPCMChan::HandleEmptyNote()
{
}

void CDPCMChan::HandleCut()
{
//	KillChannel();
	CutNote();
}

void CDPCMChan::HandleRelease()
{
	m_bRelease = true;
}

void CDPCMChan::HandleNote(int MidiNote)
{
	CChannelHandler::HandleNote(MidiNote);		// // //
	m_iNote = MidiNote;		// // //
	TriggerNote(m_iNote);
	m_bGate = true;
}

bool CDPCMChan::CreateInstHandler(inst_type_t Type)
{
	switch (Type) {
	case INST_2A03:
		switch (m_iInstTypeCurrent) {
		case INST_2A03: break;
		default:
			m_pInstHandler = std::make_unique<CInstHandlerDPCM>(this);
			return true;
		}
	}
	return false;
}

void CDPCMChan::RefreshChannel()
{
	if (m_cDAC != 255) {
		m_pAPU->Write(0x4011, m_cDAC);
		m_cDAC = 255;
	}

	if (m_iRetrigger != 0) {
		--m_iRetriggerCntr;
		if (m_iRetriggerCntr == 0) {
			m_iRetriggerCntr = m_iRetrigger;
			m_bEnabled = true;
			m_bRetrigger = true;
		}
	}

	if (m_bRelease) {
		// Release command
		m_pAPU->Write(0x4015, 0x0F);
		m_bEnabled = false;
		m_bRelease = false;
	}

/*
	if (m_bRelease) {
		// Release loop flag
		m_bRelease = false;
		m_pAPU->Write(0x4010, 0x00 | (m_iPeriod & 0x0F));
		return;
	}
*/

	if (!m_bEnabled)
		return;

	if (!m_bGate) {
		// Cut sample
		m_pAPU->Write(0x4015, 0x0F);

#ifndef FT0CC_EXT_BUILD
		if (!FTEnv.GetSettings()->General.bNoDPCMReset || FTEnv.GetSoundGenerator()->IsPlaying())		// // //
#endif
			m_pAPU->Write(0x4011, 0);	// regain full volume for TN

		m_bEnabled = false;		// don't write to this channel anymore
	}
	else if (m_bRetrigger) {
		// Start playing the sample
		m_pAPU->Write(0x4010, (m_iPeriod & 0x0F) | m_iLoop);
		m_pAPU->Write(0x4012, m_iOffset);							// load address, start at $C000
		m_pAPU->Write(0x4013, m_iSampleLength);						// length
		m_pAPU->Write(0x4015, 0x0F);
		m_pAPU->Write(0x4015, 0x1F);								// fire sample

		// Loop offset
		if (m_iLoopOffset > 0) {
			m_pAPU->Write(0x4012, m_iLoopOffset);
			m_pAPU->Write(0x4013, m_iLoopLength);
		}

		m_bRetrigger = false;
	}
}

int CDPCMChan::GetChannelVolume() const
{
	return VOL_COLUMN_MAX;
}

void CDPCMChan::WriteDCOffset(unsigned char Delta)		// // //
{
	// Initial delta counter value
	if (Delta != 255 && m_cDAC == 255)
		m_cDAC = Delta;
}

void CDPCMChan::SetLoopOffset(unsigned char Loop)		// // //
{
	m_iLoopOffset = Loop;
}

void CDPCMChan::PlaySample(std::shared_ptr<const ft0cc::doc::dpcm_sample> pSamp, int Pitch)		// // //
{
	int SampleSize = pSamp->size();
	if (auto *p2A03 = dynamic_cast<C2A03 *>(m_pAPU->GetSoundChip(sound_chip_t::APU)))
		p2A03->WriteSample(std::move(pSamp));		// // //
	m_iPeriod = m_iCustomPitch != -1 ? m_iCustomPitch : Pitch;
	m_iSampleLength = (SampleSize >> 4) - (m_iOffset << 2);
	m_iLoopLength = SampleSize - m_iLoopOffset;
	m_bEnabled = true;
	m_bRetrigger = true;
	m_iLoop = (Pitch & 0x80) >> 1;
	m_iRetriggerCntr = m_iRetrigger;
}

void CDPCMChan::ClearRegisters()
{
	m_pAPU->Write(0x4015, 0x0F);

	m_pAPU->Write(0x4010, 0);
	m_pAPU->Write(0x4011, 0);
	m_pAPU->Write(0x4012, 0);
	m_pAPU->Write(0x4013, 0);

	m_iOffset = 0;
	m_cDAC = 255;
}

std::string CDPCMChan::GetCustomEffectString() const		// // //
{
	std::string str;

	if (m_iOffset)
		str += MakeCommandString({effect_t::SAMPLE_OFFSET, m_iOffset});

	return str;
}
