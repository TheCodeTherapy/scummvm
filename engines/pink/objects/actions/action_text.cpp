/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "common/debug.h"
#include "common/substream.h"

#include "graphics/transparent_surface.h"

#include "pink/archive.h"
#include "pink/director.h"
#include "pink/pink.h"
#include "pink/objects/actors/actor.h"
#include "pink/objects/actions/action_text.h"
#include "pink/objects/pages/page.h"

namespace Pink {

ActionText::ActionText() {
	_txtWnd = nullptr;
	_macText = nullptr;

	_xLeft = _xRight = 0;
	_yTop = _yBottom = 0;

	_centered = 0;
	_scrollBar = 0;

	_textRGB = 0;
	_backgroundRGB = 0;

	_textColorIndex = 0;
	_backgroundColorIndex = 0;
}

ActionText::~ActionText() {
	end();
}

void ActionText::deserialize(Archive &archive) {
	Action::deserialize(archive);
	_fileName = archive.readString();

	_xLeft = archive.readDWORD();
	_yTop = archive.readDWORD();
	_xRight = archive.readDWORD();
	_yBottom = archive.readDWORD();

	_centered = archive.readDWORD();
	_scrollBar = archive.readDWORD();
	_textRGB = archive.readDWORD();
	_backgroundRGB = archive.readDWORD();
}

void ActionText::toConsole() const {
	debugC(6, kPinkDebugLoadingObjects, "\tActionText: _name = %s, _fileName = %s, "
				  "_xLeft = %u, _yTop = %u, _xRight = %u, _yBottom = %u _centered = %u, _scrollBar = %u, _textColor = %u _backgroundColor = %u",
		  _name.c_str(), _fileName.c_str(), _xLeft, _yTop, _xRight, _yBottom, _centered, _scrollBar, _textRGB, _backgroundRGB);
}

void ActionText::start() {
	findColorsInPalette();
	Director *director = _actor->getPage()->getGame()->getDirector();
	Graphics::TextAlign align = _centered ? Graphics::kTextAlignCenter : Graphics::kTextAlignLeft;
	Common::SeekableReadStream *stream = _actor->getPage()->getResourceStream(_fileName);

	char *str = new char[stream->size()];
	stream->read(str, stream->size());
	delete stream;

	switch(_actor->getPage()->getGame()->getLanguage()) {
	case Common::DA_DNK:
	case Common::ES_ESP:
	case Common::FR_FRA:
	case Common::PT_BRA:
		_text = Common::String(str).decode(Common::kWindows1252);
		break;

	case Common::FI_FIN:
	case Common::SE_SWE:
		_text = Common::String(str).decode(Common::kWindows1257);
		break;

	case Common::HE_ISR:
		_text = Common::String(str).decode(Common::kWindows1255);
		if (!_centered) {
			align = Graphics::kTextAlignRight;
		}
		break;

	case Common::PL_POL:
		_text = Common::String(str).decode(Common::kWindows1250);
		break;

	case Common::RU_RUS:
		_text = Common::String(str).decode(Common::kWindows1251);
		break;

	case Common::EN_ANY:
	default:
		_text = Common::String(str);
		break;
	}

	delete[] str;

	while ( _text.size() > 0 && (_text[ _text.size() - 1 ] == '\n' || _text[ _text.size() - 1 ] == '\r') )
		_text.deleteLastChar();

	if (_scrollBar) {
		_txtWnd = director->getWndManager().addTextWindow(director->getTextFont(), _textColorIndex, _backgroundColorIndex,
														  _xRight - _xLeft, align, nullptr, false);
		_txtWnd->setTextColorRGB(_textRGB);
		_txtWnd->enableScrollbar(true);
		// it will hide the scrollbar when the text height is smaller than the window height
		_txtWnd->setMode(Graphics::kWindowModeDynamicScrollbar);
		_txtWnd->move(_xLeft, _yTop);
		_txtWnd->resize(_xRight - _xLeft, _yBottom - _yTop);
		_txtWnd->setEditable(false);
		_txtWnd->setSelectable(false);

		_txtWnd->appendText(_text);
		director->addTextWindow(_txtWnd);

	} else {
		director->addTextAction(this);

		// alignment not working, thus we implement alignment for center manually
		Graphics::TextAlign alignment = _centered ? Graphics::kTextAlignCenter : Graphics::kTextAlignLeft;
		if (!_centered && _actor->getPage()->getGame()->getLanguage() == Common::HE_ISR) {
			alignment = Graphics::kTextAlignRight;
		}
		_macText = new Graphics::MacText(_text, &director->getWndManager(), director->getTextFont(), _textColorIndex, _backgroundColorIndex, _xRight - _xLeft, alignment);
	}
}

Common::Rect ActionText::getBound() {
	return Common::Rect(_xLeft, _yTop, _xRight, _yBottom);
}

void ActionText::end() {
	Director *director = _actor->getPage()->getGame()->getDirector();
	if (_scrollBar && _txtWnd) {
		director->getWndManager().removeWindow(_txtWnd);
		director->removeTextWindow(_txtWnd);
		_txtWnd = nullptr;
	} else {
		director->removeTextAction(this);
		delete _macText;
	}
}

void ActionText::draw(Graphics::ManagedSurface *surface) {
	int xOffset = 0, yOffset = 0;
	// we need to first fill this area with backgroundColor, in order to wash away the previous text
	surface->fillRect(Common::Rect(_xLeft, _yTop, _xRight, _yBottom), _backgroundColorIndex);

	if (_centered) {
		xOffset = (_xRight - _xLeft) / 2 - _macText->getTextMaxWidth() / 2;
		yOffset = (_yBottom - _yTop) / 2 - _macText->getTextHeight() / 2;
	}
	_macText->drawToPoint(surface, Common::Rect(0, 0, _xRight - _xLeft, _yBottom - _yTop), Common::Point(_xLeft + xOffset, _yTop + yOffset));
}

#define BLUE(rgb) ((rgb) & 0xFF)
#define GREEN(rgb) (((rgb) >> 8) & 0xFF)
#define RED(rgb) (((rgb) >> 16) & 0xFF)

static uint findBestColor(byte *palette, uint32 rgb) {
	uint bestColor = 0;
	double min = 0xFFFFFFFF;
	for (uint i = 0; i < 256; ++i) {
		int rmean = (*(palette + 3 * i + 0) + RED(rgb)) / 2;
		int r = *(palette + 3 * i + 0) - RED(rgb);
		int g = *(palette + 3 * i + 1) - GREEN(rgb);
		int b = *(palette + 3 * i + 2) - BLUE(rgb);

		double dist = sqrt((((512 + rmean) * r * r) >> 8) + 4 * g * g + (((767 - rmean) * b * b) >> 8));
		if (min > dist) {
			bestColor = i;
			min = dist;
		}
	}

	debug(2, "for color %06x the best color is %02x%02x%02x", rgb, palette[bestColor * 3], palette[bestColor * 3 + 1], palette[bestColor * 3 + 2]);

	return bestColor;
}

void ActionText::findColorsInPalette() {
	byte palette[256 * 3];
	g_system->getPaletteManager()->grabPalette(palette, 0, 256);

	debug(2, "textcolorindex: %06x", _textRGB);
	_textColorIndex = findBestColor(palette, _textRGB);
	debug(2, "backgroundColorIndex: %06x", _backgroundRGB);
	_backgroundColorIndex = findBestColor(palette, _backgroundRGB);
}

} // End of namespace Pink
