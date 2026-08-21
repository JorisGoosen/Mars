#include "gereedschap.h"
#include "weergaveSchermPerspectief.h"
#include <GLFW/glfw3.h>

//Gevoeligheden (raden/zoomstappen per eenheid invoer); tekenomdraaien = richting omdraaien.
static const float sleepFactor = 0.005f; //rad per beeldpixel tijdens het slepen
static const float swipeFactor = 0.01f;  //rad per scroll-eenheid (horizontale swipe)
static const float zoomFactor  = 0.05f;  //kijkafstand per scroll-eenheid (verticaal/pinch)

//Stuurt een sleep-delta naar het juiste doel: planeet = cameradraai (trackball),
//zon = de zichtbare zon om de origin (callback van Simulatie, met de camera-asjes).
void verplaatsGereedschap::_stuurDelta(double dx, double dy)
{
	if(_zonRoteer && _modus == mZon)
	{
		//De callback van Simulatie roteert de render-zon met de camera-asjes;
		//de sim-zon merkt hier niets van.
		_zonRoteer(dx, dy);
	}
	else
	{
		//Trackball: het aangeklikte oppervlak volgt de muis — rechts slepen
		//schuift de planeet mee naar rechts (zelfde richting als de pijltjestoetsen).
		_scherm->roteer((float)(dx * sleepFactor),
		                (float)(dy * sleepFactor));
	}
}

void verplaatsGereedschap::muisPos(double x, double y)
{
	_aanwijzer.muisPos(x, y);
	if(_sleept)
	{
		double dx = x - _laatsteX;
		double dy = y - _laatsteY;
		if(_modus == mOnbekend)
		{
			//Buffer de delta totdat de pick-pass de modus heeft beslist (Simulatie
			//roept zetModus, die de wachtrij doorspoelt); er hoeft niets verloren te gaan.
			_wachtX += dx;
			_wachtY += dy;
		}
		else
		{
			_stuurDelta(dx, dy);
		}
	}
	_laatsteX = x;
	_laatsteY = y;
}

void verplaatsGereedschap::muisKnop(int knop, int actie, int mods)
{
	(void)mods;
	if(knop == _rotatieKnop)
	{
		_sleept = (actie == GLFW_PRESS);
		if(_sleept)
		{
			_modus  = mOnbekend;
			_wachtX = 0.0;
			_wachtY = 0.0;
		}
	}
}

void verplaatsGereedschap::muisWiel(double dx, double dy)
{
	//Richting-splitsing: horizontale swipe roteert om de Y-as, verticale scroll
	//(en pinch, dat de browser als scroll meldt) zoomt. De browser levert delta's
	//tegengesteld aan GLFW (deltaY > 0 = omlaag scrollen), dus op web wordt het
	//teken omgedraaid zodat "omhoog scrollen / pinch-uit" inzoemt.
#ifdef __EMSCRIPTEN__
	_scherm->zoom((float)(-dy * zoomFactor));
	_scherm->roteer((float)(dx * swipeFactor), 0.0f);
#else
	_scherm->zoom((float)(dy * zoomFactor));
	_scherm->roteer((float)(-dx * swipeFactor), 0.0f);
#endif
}

void verplaatsGereedschap::zetModus(modus m)
{
	_modus = m;
	if(_modus != mOnbekend && (_wachtX != 0.0 || _wachtY != 0.0))
	{
		double bX = _wachtX, bY = _wachtY;
		_wachtX  = 0.0;
		_wachtY  = 0.0;
		_stuurDelta(bX, bY);
	}
}