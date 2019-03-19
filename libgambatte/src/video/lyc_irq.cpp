//
//   Copyright (C) 2007 by sinamas <sinamas at users.sourceforge.net>
//
//   This program is free software; you can redistribute it and/or modify
//   it under the terms of the GNU General Public License version 2 as
//   published by the Free Software Foundation.
//
//   This program is distributed in the hope that it will be useful,
//   but WITHOUT ANY WARRANTY; without even the implied warranty of
//   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//   GNU General Public License version 2 for more details.
//
//   You should have received a copy of the GNU General Public License
//   version 2 along with this program; if not, write to the
//   Free Software Foundation, Inc.,
//   51 Franklin St, Fifth Floor, Boston, MA  02110-1301, USA.
//

#include "lyc_irq.h"
#include "counterdef.h"
#include "lcddef.h"
#include "ly_counter.h"
#include "savestate.h"
#include <algorithm>

using namespace gambatte;

namespace {

unsigned long schedule(unsigned statReg,
		unsigned lycReg, LyCounter const &lyCounter, unsigned long cc) {
	bool const ds = lyCounter.isDoubleSpeed();
	return (statReg & lcdstat_lycirqen) && lycReg < lcd_lines_per_frame
	? lyCounter.nextFrameCycle(lycReg
		? 1l * lycReg * lcd_cycles_per_line - 2 + 2 * ds
		: (lcd_lines_per_frame - 1l) * lcd_cycles_per_line + 6 + 2 * ds, cc)
	: static_cast<unsigned long>(disabled_time);
}

bool lycIrqBlockedByM2OrM1StatIrq(unsigned ly, unsigned statreg) {
	return ly < lcd_vres && ly > 0
	? statreg & lcdstat_m2irqen
	: statreg & lcdstat_m1irqen;
}

}

LycIrq::LycIrq()
: time_(disabled_time)
, lycRegSrc_(0)
, statRegSrc_(0)
, lycReg_(0)
, statReg_(0)
, cgb_(false)
{
}

void LycIrq::regChange(unsigned const statReg,
		unsigned const lycReg, LyCounter const &lyCounter, unsigned long const cc) {
	unsigned long const timeSrc = schedule(statReg, lycReg, lyCounter, cc);
	statRegSrc_ = statReg;
	lycRegSrc_ = lycReg;
	time_ = std::min(time_, timeSrc);

	if (cgb_) {
		if (time_ - cc > 8 || (timeSrc != time_ && time_ - cc > 4u - lyCounter.isDoubleSpeed() * 4))
			lycReg_ = lycReg;

		if (time_ - cc > 4u - lyCounter.isDoubleSpeed() * 4)
			statReg_ = statReg;
	} else {
		if (time_ - cc > 4 || timeSrc != time_)
			lycReg_ = lycReg;

		if (time_ - cc > 4 || lycReg_ != 0)
			statReg_ = statReg;

		statReg_ = (statReg_ & lcdstat_lycirqen) | (statReg & ~(1u * lcdstat_lycirqen));
	}
}

void LycIrq::doEvent(unsigned char *const ifreg, LyCounter const &lyCounter) {
	if ((statReg_ | statRegSrc_) & lcdstat_lycirqen) {
		unsigned const cmpLy = lyCounter.ly() == lcd_lines_per_frame - 1
			&& lyCounter.time() - time_ < lyCounter.lineTime()
			? 0
			: lyCounter.ly() + (lyCounter.time() - time_ < 4);
		if (lycReg_ == cmpLy && !lycIrqBlockedByM2OrM1StatIrq(lycReg_, statReg_))
			*ifreg |= 2;
	}

	lycReg_ = lycRegSrc_;
	statReg_ = statRegSrc_;
	time_ = schedule(statReg_, lycReg_, lyCounter, time_);
}

void LycIrq::loadState(SaveState const &state) {
	lycRegSrc_ = state.mem.ioamhram.get()[0x145];
	statRegSrc_ = state.mem.ioamhram.get()[0x141];
	lycReg_ = state.ppu.lyc;
	statReg_ = statRegSrc_;
}

void LycIrq::saveState(SaveState &state) const {
	state.ppu.lyc = lycReg_;
}

void LycIrq::reschedule(LyCounter const &lyCounter, unsigned long cc) {
	time_ = std::min(schedule(statReg_   , lycReg_   , lyCounter, cc),
	                 schedule(statRegSrc_, lycRegSrc_, lyCounter, cc));
}

void LycIrq::lcdReset() {
	statReg_ = statRegSrc_;
	lycReg_ = lycRegSrc_;
}
