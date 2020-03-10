#include <rack.hpp>

#include <algorithm> // std::find
#include <vector> // std::vector
#include <sstream> // stringstream
#include <utility> // std::pair
#include "midiDllz.hpp"

#define mFONT_FILE asset::plugin(pluginInstance, "res/terminal-grotesque.ttf")

using namespace rack;

// Declare the Plugin, defined in plugin.cpp
extern Plugin* pluginInstance;

// Declare each Model, defined in each module source file
extern Model* modelSuperMIDI64;
extern Model* modelDuoMIDI_CV;

///////////////////////
// custom components
///////////////////////

struct TPurpleLight : GrayModuleLightWidget {
	TPurpleLight() {
		this->addBaseColor(nvgRGB(139, 112, 162));
	}
};

///Switch with Led
struct DLXSwitchLed : SvgSwitch {
	DLXSwitchLed() {
		// box.size = Vec(10, 18);
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/DLXSwitchLed_0.svg")));
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/DLXSwitchLed_1.svg")));
		shadow->opacity = 0.f;
	}
	void randomize() override{
	}
};

/// Transp Light over led switches
struct TranspOffRedLight : ModuleLightWidget {
	TranspOffRedLight() {
		box.size = Vec(10.f,10.f);
		addBaseColor(nvgRGBA(0xff, 0x00, 0x00, 0x88));//borderColor = nvgRGBA(0, 0, 0, 0x60);
	}
};
/// UpDown + - Buttons
struct minusButtonB : SvgSwitch {
	minusButtonB() {
		momentary = true;
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/DLXSqrMinus_0.svg")));
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/DLXSqrMinus_1.svg")));
		shadow->opacity = 0.f;
	}
	void randomize() override{
	}
};
struct plusButtonB : SvgSwitch {
	plusButtonB() {
		momentary = true;
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/DLXSqrPlus_0.svg")));
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/DLXSqrPlus_1.svg")));
		shadow->opacity = 0.f;
	}
	void randomize() override{
	}
};

///Jacks
struct DLXPortPoly : SvgPort {
	DLXPortPoly() {
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/DLXPort16.svg")));
		shadow->opacity = 0.f;
	}
};

struct DLXPortPolyOut : SvgPort {
	DLXPortPolyOut() {
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/DLXPort16b.svg")));
		shadow->opacity = 0.f;
	}
};

struct DLXPortG : SvgPort {
	DLXPortG() {
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/DLXPortG.svg")));
		shadow->opacity = 0.f;
	}
};
