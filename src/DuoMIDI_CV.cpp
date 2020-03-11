#include "plugin.hpp"
#include <algorithm>

struct DuoMIDI_CV : Module {
	enum ParamIds {
		NUM_PARAMS
	};
	enum InputIds {
		NOTESTOP1_INPUT,
		NOTESTOP2_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		PITCH1_OUTPUT,
		PITCH2_OUTPUT,
		GATE1_OUTPUT,
		GATE2_OUTPUT,
		VELOCITY1_OUTPUT,
		VELOCITY2_OUTPUT,
		BENT_PITCH1_OUTPUT,
		BENT_PITCH2_OUTPUT,
		RETRIGGER1_OUTPUT,
		RETRIGGER2_OUTPUT,
		MOD1_OUTPUT,
		MOD2_OUTPUT,
		PITCH_BEND1_OUTPUT,
		PITCH_BEND2_OUTPUT,
		AFTERTOUCH1_OUTPUT,
		AFTERTOUCH2_OUTPUT,
		START_OUTPUT,
		STOP_OUTPUT,
		CONTINUE_OUTPUT,
		CLOCK_OUTPUT,
		CLOCK_DIV_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		NUM_LIGHTS
	};

	midi::InputQueue midiInput;

	int channels1, channels2;
	enum PolyMode {
		ROTATE_MODE,
		REUSE_MODE,
		RESET_MODE,
		MPE_MODE,
		NUM_POLY_MODES
	};
	PolyMode polyMode;

	enum MPEMode {
		DIRECT_MPE,
		ROTATE_MPE,
		REUSE_MPE,
		RESET_MPE,
		NUM_MPE_MODES
	};
	MPEMode mpeMode;

	uint32_t clock = 0;
	int clockDivision;

	float bendRangeUp = 60.f;
	float bendRangeDown = 60.f;

	bool pedal;
	// Indexed by channel, indexes 16-31 correspond to secondary output channels
	uint8_t notes[32];
	bool gates[32];
	uint8_t velocities[32];
	uint8_t aftertouches[32];
	int assignedChannels[32];
	std::vector<uint8_t> heldNotes;

	int rotateIndex;

	// 32 channels for MPE. When MPE is disabled, only the first channel is used.
	uint16_t bends[32];
	uint8_t mods[32];
	dsp::ExponentialFilter pitchFilters[32];
	dsp::ExponentialFilter modFilters[32];

	dsp::PulseGenerator clockPulse;
	dsp::PulseGenerator clockDividerPulse;
	dsp::PulseGenerator retriggerPulses[32];
	dsp::PulseGenerator startPulse;
	dsp::PulseGenerator stopPulse;
	dsp::PulseGenerator continuePulse;


	DuoMIDI_CV() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		heldNotes.reserve(128);
		for (int c = 0; c < 32; c++) {
			pitchFilters[c].setTau(1 / 30.f);
			modFilters[c].setTau(1 / 30.f);
		}
		onReset();
	}

	void onReset() override {
		channels1 = 1;
		channels2 = 0;
		polyMode = ROTATE_MODE;
		mpeMode = DIRECT_MPE;
		clockDivision = 24;
		bendRangeUp = 60.f;
		bendRangeDown = 60.f;
		panic();
		midiInput.reset();
	}

	/** Resets performance state */
	void panic() {
		pedal = false;
		for (int c = 0; c < 32; c++) {
			notes[c] = 60;
			gates[c] = false;
			velocities[c] = 0;
			aftertouches[c] = 0;
			bends[c] = 8192;
			mods[c] = 0;
			pitchFilters[c].reset();
			modFilters[c].reset();
			assignedChannels[c] = c;
		}
		pedal = false;
		//MPE rotation skips "master channel" 1
		if (polyMode == MPE_MODE && mpeMode == ROTATE_MPE)
			rotateIndex = 0;
		else
			rotateIndex = -1;

		heldNotes.clear();
	}

	void process(const ProcessArgs& args) override {
		midi::Message msg;
		while (midiInput.shift(&msg)) {
			processMessage(msg);
		}

		inputs[NOTESTOP1_INPUT].setChannels(channels1);
		inputs[NOTESTOP2_INPUT].setChannels(channels2);
		outputs[PITCH1_OUTPUT].setChannels(channels1);
		outputs[PITCH2_OUTPUT].setChannels(channels2);
		outputs[GATE1_OUTPUT].setChannels(channels1);
		outputs[GATE2_OUTPUT].setChannels(channels2);
		outputs[VELOCITY1_OUTPUT].setChannels(channels1);
		outputs[VELOCITY2_OUTPUT].setChannels(channels2);
		outputs[AFTERTOUCH1_OUTPUT].setChannels(channels1);
		outputs[AFTERTOUCH2_OUTPUT].setChannels(channels2);
		outputs[RETRIGGER1_OUTPUT].setChannels(channels1);
		outputs[RETRIGGER2_OUTPUT].setChannels(channels2);
		outputs[BENT_PITCH1_OUTPUT].setChannels(channels1);
		outputs[BENT_PITCH2_OUTPUT].setChannels(channels2);

		for (int c = 0; c < channels1; c++) {
			if (inputs[NOTESTOP1_INPUT].getVoltage(c) >= 1.f) {
				velocities[c] = 0;
				aftertouches[c] = 0;
				bends[c] = 8192;
				mods[c] = 0;
				pitchFilters[c].reset();
				modFilters[c].reset();
			}
			outputs[PITCH1_OUTPUT].setVoltage((notes[c] - 60.f) / 12.f, c);
			outputs[GATE1_OUTPUT].setVoltage(gates[c] ? 10.f : 0.f, c);
			outputs[VELOCITY1_OUTPUT].setVoltage(rescale(velocities[c], 0, 127, 0.f, 10.f), c);
			outputs[AFTERTOUCH1_OUTPUT].setVoltage(rescale(aftertouches[c], 0, 127, 0.f, 10.f), c);
			outputs[RETRIGGER1_OUTPUT].setVoltage(retriggerPulses[c].process(args.sampleTime) ? 10.f : 0.f, c);
		}
		for (int c = 0; c < channels2; c++) {
			if (inputs[NOTESTOP2_INPUT].getVoltage(c) >= 1.f) {
				velocities[16 + c] = 0;
				aftertouches[16 + c] = 0;
				bends[16 + c] = 8192;
				mods[16 + c] = 0;
				pitchFilters[16 + c].reset();
				modFilters[16 + c].reset();
			}
			outputs[PITCH2_OUTPUT].setVoltage((notes[16 + c] - 60.f) / 12.f, c);
			outputs[GATE2_OUTPUT].setVoltage(gates[16 + c] ? 10.f : 0.f, c);
			outputs[VELOCITY2_OUTPUT].setVoltage(rescale(velocities[16 + c], 0, 127, 0.f, 10.f), c);
			outputs[AFTERTOUCH2_OUTPUT].setVoltage(rescale(aftertouches[16 + c], 0, 127, 0.f, 10.f), c);
			outputs[RETRIGGER2_OUTPUT].setVoltage(retriggerPulses[16 + c].process(args.sampleTime) ? 10.f : 0.f, c);
		}

		if (polyMode == MPE_MODE) {
			outputs[PITCH_BEND1_OUTPUT].setChannels(channels1);
			outputs[PITCH_BEND2_OUTPUT].setChannels(channels2);
			outputs[MOD1_OUTPUT].setChannels(channels1);
			outputs[MOD2_OUTPUT].setChannels(channels2);
			for (int c = 0; c < channels1; c++) {
				if (bends[c] > 8192)
					outputs[PITCH_BEND1_OUTPUT].setVoltage(pitchFilters[c].process(args.sampleTime, rescale(bends[c], 0, 1 << 14, -5.f, 5.f)*(bendRangeUp/60.f)), c);
				else
					outputs[PITCH_BEND1_OUTPUT].setVoltage(pitchFilters[c].process(args.sampleTime, rescale(bends[c], 0, 1 << 14, -5.f, 5.f)*(bendRangeDown/60.f)), c);
				outputs[BENT_PITCH1_OUTPUT].setVoltage((outputs[PITCH1_OUTPUT].getVoltage(c) + outputs[PITCH_BEND1_OUTPUT].getVoltage(c)), c);
				outputs[MOD1_OUTPUT].setVoltage(modFilters[c].process(args.sampleTime, rescale(mods[c], 0, 127, 0.f, 10.f)), c);
			}
			for (int c = 0; c < channels2; c++) {
				if (bends[16 + c] > 8192)
					outputs[PITCH_BEND2_OUTPUT].setVoltage(pitchFilters[16 + c].process(args.sampleTime, rescale(bends[16 + c], 0, 1 << 14, -5.f, 5.f)*(bendRangeUp/60.f)), c);
				else
					outputs[PITCH_BEND2_OUTPUT].setVoltage(pitchFilters[16 + c].process(args.sampleTime, rescale(bends[16 + c], 0, 1 << 14, -5.f, 5.f)*(bendRangeDown/60.f)), c);
				outputs[BENT_PITCH2_OUTPUT].setVoltage((outputs[PITCH2_OUTPUT].getVoltage(c) + outputs[PITCH_BEND2_OUTPUT].getVoltage(c)), c);
				outputs[MOD2_OUTPUT].setVoltage(modFilters[16 + c].process(args.sampleTime, rescale(mods[16 + c], 0, 127, 0.f, 10.f)), c);
			}
		}
		else {
			outputs[PITCH_BEND1_OUTPUT].setChannels(1);
			outputs[PITCH_BEND2_OUTPUT].setChannels(1);
			outputs[MOD1_OUTPUT].setChannels(1);
			outputs[MOD2_OUTPUT].setChannels(1);
			if (bends[0] > 8192)
				outputs[PITCH_BEND1_OUTPUT].setVoltage(pitchFilters[0].process(args.sampleTime, rescale(bends[0], 0, 1 << 14, -5.f, 5.f)*(bendRangeUp/60.f)));
			else
				outputs[PITCH_BEND1_OUTPUT].setVoltage(pitchFilters[0].process(args.sampleTime, rescale(bends[0], 0, 1 << 14, -5.f, 5.f)*(bendRangeDown/60.f)));
			outputs[PITCH_BEND2_OUTPUT].setVoltage(outputs[PITCH_BEND1_OUTPUT].getVoltage());
			outputs[MOD1_OUTPUT].setVoltage(modFilters[0].process(args.sampleTime, rescale(mods[0], 0, 127, 0.f, 10.f)));
			outputs[MOD2_OUTPUT].setVoltage(outputs[MOD1_OUTPUT].getVoltage());
			for (int c = 0; c < channels1; c++)
				outputs[BENT_PITCH1_OUTPUT].setVoltage((outputs[PITCH1_OUTPUT].getVoltage(c) + outputs[PITCH_BEND1_OUTPUT].getVoltage()), c);
			for (int c = 0; c < channels2; c++)
				outputs[BENT_PITCH2_OUTPUT].setVoltage((outputs[PITCH2_OUTPUT].getVoltage(c) + outputs[PITCH_BEND2_OUTPUT].getVoltage()), c);
		}

		outputs[CLOCK_OUTPUT].setVoltage(clockPulse.process(args.sampleTime) ? 10.f : 0.f);
		outputs[CLOCK_DIV_OUTPUT].setVoltage(clockDividerPulse.process(args.sampleTime) ? 10.f : 0.f);
		outputs[START_OUTPUT].setVoltage(startPulse.process(args.sampleTime) ? 10.f : 0.f);
		outputs[STOP_OUTPUT].setVoltage(stopPulse.process(args.sampleTime) ? 10.f : 0.f);
		outputs[CONTINUE_OUTPUT].setVoltage(continuePulse.process(args.sampleTime) ? 10.f : 0.f);
	}

	void processMessage(midi::Message msg) {
		// DEBUG("MIDI: %01x %01x %02x %02x", msg.getStatus(), msg.getChannel(), msg.getNote(), msg.getValue());

		switch (msg.getStatus()) {
			// note off
			case 0x8: {
				releaseNote(msg.getNote());
			} break;
			// note on
			case 0x9: {
				if (msg.getValue() > 0) {
					int c = msg.getChannel();
					pressNote(msg.getNote(), &c);
					velocities[c] = msg.getValue();
				}
				else {
					// For some reason, some keyboards send a "note on" event with a velocity of 0 to signal that the key has been released.
					releaseNote(msg.getNote());
				}
			} break;
			// key pressure
			case 0xa: {
				// Set the aftertouches with the same note
				// TODO Should we handle the MPE case differently?
				for (int c = 0; c < 32; c++) {
					if (notes[c] == msg.getNote())
						aftertouches[c] = msg.getValue();
				}
			} break;
			// cc
			case 0xb: {
				processCC(msg);
			} break;
			// channel pressure
			case 0xd: {
				if (polyMode == MPE_MODE) {
					if (mpeMode == DIRECT_MPE) {
						// Set the channel aftertouch
						aftertouches[msg.getChannel()] = msg.getNote();
					}
					else
						aftertouches[assignedChannels[msg.getChannel()]] = msg.getNote();
				}
				else {
					// Set all aftertouches
					for (int c = 0; c < 32; c++) {
						aftertouches[c] = msg.getNote();
					}
				}
			} break;
			// pitch wheel
			case 0xe: {
				int c = 0;
				if (polyMode == MPE_MODE) {
					if (mpeMode == DIRECT_MPE)
						c = msg.getChannel();
					else
						c = assignedChannels[msg.getChannel()];
				}
				bends[c] = ((uint16_t) msg.getValue() << 7) | msg.getNote();
			} break;
			case 0xf: {
				processSystem(msg);
			} break;
			default: break;
		}
	}

	void processCC(midi::Message msg) {
		switch (msg.getNote()) {
			// mod
			case 0x01: {
				int c = 0;
				if (polyMode == MPE_MODE) {
					if (mpeMode == DIRECT_MPE)
						c = msg.getChannel();
					else
						c = assignedChannels[msg.getChannel()];
				}
				mods[c] = msg.getValue();
			} break;
			// sustain
			case 0x40: {
				if (msg.getValue() >= 64)
					pressPedal();
				else
					releasePedal();
			} break;
			default: break;
		}
	}

	void processSystem(midi::Message msg) {
		switch (msg.getChannel()) {
			// Timing
			case 0x8: {
				clockPulse.trigger(1e-3);
				if (clock % clockDivision == 0) {
					clockDividerPulse.trigger(1e-3);
				}
				clock++;
			} break;
			// Start
			case 0xa: {
				startPulse.trigger(1e-3);
				clock = 0;
			} break;
			// Continue
			case 0xb: {
				continuePulse.trigger(1e-3);
			} break;
			// Stop
			case 0xc: {
				stopPulse.trigger(1e-3);
				clock = 0;
			} break;
			default: break;
		}
	}

	int assignChannel(uint8_t note, int* channel) {
		if (channels1 == 1 && channels2 == 0)
			return 0;

		switch (polyMode) {
			case REUSE_MODE: {
				// Find channel with the same note
				for (int c = 0; c < (channels1 + channels2); c++) {
					if (c < channels1) {
						if (notes[c] == note)
							return c;
					}
					else {
						if (notes[16 + (c - channels1)] == note)
							return 16 + (c - channels1);
					}
				}
			} // fallthrough

			case ROTATE_MODE: {
				// Find next available channel
				for (int i = 0; i < (channels1 + channels2); i++) {
					rotateIndex++;
					if (rotateIndex >= channels1) {
						if (channels2) {
							if (rotateIndex < 16)
								rotateIndex = 16;
							else if (rotateIndex >= 16 + channels2)
								rotateIndex = 0;
						}
						else
							rotateIndex = 0;
					}
					if (!gates[rotateIndex])
						return rotateIndex;
				}
				// No notes are available. Advance rotateIndex once more.
				rotateIndex++;
				if (rotateIndex >= (16 + channels2))
					rotateIndex = 0;
				return rotateIndex;
			} break;

			case RESET_MODE: {
				for (int c = 0; c < (channels1 + channels2); c++) {
					if (c < channels1) {
						if (!gates[c])
							return c;
					}
					else {
						if (!gates[16 + (c - channels1)])
							return 16 + (c - channels1);
					}
				}
				return (16 + channels2) - 1;
			} break;

			case MPE_MODE: {
				switch (mpeMode) {
					case DIRECT_MPE: {
						// This case is handled by querying the MIDI message channel.
						return 0;
					} break;

					case REUSE_MPE: {
						// Find channel with the same note
						for (int c = 1; c < (channels1 + channels2); c++) {
							if (c < channels1) {
								if (notes[c] == note) {
									assignedChannels[*channel] = c;
									return c;
								}
							}
							else {
								if (notes[16 + (c - channels1)] == note) {
									assignedChannels[*channel] = 16 + (c - channels1);
									return 16 + (c - channels1);
								}
							}
						}
					} // fallthrough

					case ROTATE_MPE: {
						for (int i = 1; i < (channels1 + channels2); i++) {
							rotateIndex++;
							if (rotateIndex >= channels1) {
								if (channels2) {
									if (rotateIndex < 16)
										rotateIndex = 16;
								}
								else
									rotateIndex = 1;
							}
							if (!gates[rotateIndex]) {
								assignedChannels[*channel] = rotateIndex;
								return rotateIndex;
							}
						}
						// No notes are available. Advance rotateIndex once more.
						rotateIndex++;
						if (rotateIndex >= (16 + channels2))
							rotateIndex = 1;
						assignedChannels[*channel] = rotateIndex;
						return rotateIndex;
					} break;

					case RESET_MPE: {
						for (int c = 0; c < (channels1 + channels2); c++) {
							if (c < channels1) {
								if (!gates[c]) {
									assignedChannels[*channel] = c;
									return c;
								}
							}
							else {
								if (!gates[16 + (c - channels1)]) {
									assignedChannels[*channel] = 16 + (c - channels1);
									return 16 + (c - channels1);
								}
							}
						}
						assignedChannels[*channel] = (16 + channels2) - 1;
						return (16 + channels2) - 1;
					}

					default: return 1;
				}
			} break;

			default: return 0;
		}
	}

	void pressNote(uint8_t note, int* channel) {
		// Remove existing similar note
		auto it = std::find(heldNotes.begin(), heldNotes.end(), note);
		if (it != heldNotes.end())
			heldNotes.erase(it);
		// Push note
		heldNotes.push_back(note);
		// Determine actual channel
		if (polyMode == MPE_MODE && mpeMode == DIRECT_MPE) {
			// Channel is already decided for us
		}
		else {
			*channel = assignChannel(note, channel);
		}
		// Set note
		notes[*channel] = note;
		gates[*channel] = true;
		retriggerPulses[*channel].trigger(1e-3);
	}

	void releaseNote(uint8_t note) {
		// Remove the note
		auto it = std::find(heldNotes.begin(), heldNotes.end(), note);
		if (it != heldNotes.end())
			heldNotes.erase(it);
		// Hold note if pedal is pressed
		if (pedal)
			return;
		// Turn off gate of all channels with note
		for (int c = 0; c < (16 + channels2); c++) {
			if (notes[c] == note) {
				gates[c] = false;
			}
		}
		// Set last note if monophonic
		if (channels1 == 1 && channels2 == 0) {
			if (note == notes[0] && !heldNotes.empty()) {
				uint8_t lastNote = heldNotes.back();
				notes[0] = lastNote;
				gates[0] = true;
				return;
			}
		}
	}

	void pressPedal() {
		if (pedal)
			return;
		pedal = true;
	}

	void releasePedal() {
		if (!pedal)
			return;
		pedal = false;
		// Set last note if monophonic
		if (channels1 == 1 && channels2 == 0) {
			if (!heldNotes.empty()) {
				uint8_t lastNote = heldNotes.back();
				notes[0] = lastNote;
			}
		}
		// Clear notes that are not held if polyphonic
		else {
			for (int c = 0; c < channels1; c++) {
				if (!gates[c])
					continue;
				gates[c] = false;
				for (uint8_t note : heldNotes) {
					if (notes[c] == note) {
						gates[c] = true;
						break;
					}
				}
			}
			for (int c = 0; c < channels2; c++) {
				if (!gates[16 + c])
					continue;
				gates[16 + c] = false;
				for (uint8_t note : heldNotes) {
					if (notes[16 + c] == note) {
						gates[16 + c] = true;
						break;
					}
				}
			}
		}
	}

	void setChannels1(int channels) {
		if (channels == this->channels1)
			return;
		this->channels1 = channels;
		panic();
	}

	void setChannels2(int channels) {
		if (channels == this->channels2)
			return;
		this->channels2 = channels;
		panic();
	}

	void setPolyMode(PolyMode polyMode) {
		if (polyMode == this->polyMode)
			return;
		this->polyMode = polyMode;
		panic();
	}

	void setMPEMode(MPEMode mpeMode) {
		if (mpeMode == this->mpeMode)
			return;
		this->mpeMode = mpeMode;
		panic();
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();
		json_object_set_new(rootJ, "channels 1", json_integer(channels1));
		json_object_set_new(rootJ, "channels 2", json_integer(channels2));
		json_object_set_new(rootJ, "polyMode", json_integer(polyMode));
		json_object_set_new(rootJ, "mpeMode", json_integer(mpeMode));
		json_object_set_new(rootJ, "clockDivision", json_integer(clockDivision));
		// Saving/restoring pitch and mod doesn't make much sense for MPE.
		if (polyMode != MPE_MODE) {
			json_object_set_new(rootJ, "lastBend", json_integer(bends[0]));
			json_object_set_new(rootJ, "lastMod", json_integer(mods[0]));
		}
		json_object_set_new(rootJ, "midi", midiInput.toJson());
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		json_t* channels1J = json_object_get(rootJ, "channels 1");
		if (channels1J)
			setChannels1(json_integer_value(channels1J));

		json_t* channels2J = json_object_get(rootJ, "channels 2");
		if (channels2J)
			setChannels2(json_integer_value(channels2J));

		json_t* polyModeJ = json_object_get(rootJ, "polyMode");
		if (polyModeJ)
			polyMode = (PolyMode) json_integer_value(polyModeJ);

		json_t* mpeModeJ = json_object_get(rootJ, "mpeMode");
		if (mpeModeJ)
			mpeMode = (MPEMode) json_integer_value(mpeModeJ);

		json_t* clockDivisionJ = json_object_get(rootJ, "clockDivision");
		if (clockDivisionJ)
			clockDivision = json_integer_value(clockDivisionJ);

		json_t* lastBendJ = json_object_get(rootJ, "lastBend");
		if (lastBendJ)
			bends[0] = json_integer_value(lastBendJ);

		json_t* lastModJ = json_object_get(rootJ, "lastMod");
		if (lastModJ)
			mods[0] = json_integer_value(lastModJ);

		json_t* midiJ = json_object_get(rootJ, "midi");
		if (midiJ)
			midiInput.fromJson(midiJ);
	}
};


struct ClockDivisionValueItem : MenuItem {
	DuoMIDI_CV* module;
	int clockDivision;
	void onAction(const event::Action& e) override {
		module->clockDivision = clockDivision;
	}
};


struct ClockDivisionItem : MenuItem {
	DuoMIDI_CV* module;
	Menu* createChildMenu() override {
		Menu* menu = new Menu;
		std::vector<int> divisions = {24 * 4, 24 * 2, 24, 24 / 2, 24 / 4, 24 / 8, 2, 1};
		std::vector<std::string> divisionNames = {"Whole", "Half", "Quarter", "8th", "16th", "32nd", "12 PPQN", "24 PPQN"};
		for (size_t i = 0; i < divisions.size(); i++) {
			ClockDivisionValueItem* item = new ClockDivisionValueItem;
			item->text = divisionNames[i];
			item->rightText = CHECKMARK(module->clockDivision == divisions[i]);
			item->module = module;
			item->clockDivision = divisions[i];
			menu->addChild(item);
		}
		return menu;
	}
};


struct BendRangeUpValueItem : MenuItem {
	DuoMIDI_CV* module;
	int bendRangeUp;
	void onAction(const event::Action& e) override {
		module->bendRangeUp = bendRangeUp;
	}
};


struct BendRangeUpItem : MenuItem {
	DuoMIDI_CV* module;
	Menu* createChildMenu() override {
		Menu* menu = new Menu;
		std::vector<float> ranges = {2.f, 5.f, 7.f, 12.f, 24.f, 36.f, 48.f, 60.f, 72.f, 84.f, 96.f};
		for (size_t i = 0; i < ranges.size(); i++) {
			BendRangeUpValueItem* item = new BendRangeUpValueItem;
			item->text = string::f("+ %d", static_cast<int>(ranges[i]));
			item->rightText = CHECKMARK(module->bendRangeUp == ranges[i]);
			item->module = module;
			item->bendRangeUp = ranges[i];
			menu->addChild(item);
		}
		return menu;
	}
};


struct BendRangeDownValueItem : MenuItem {
	DuoMIDI_CV* module;
	int bendRangeDown;
	void onAction(const event::Action& e) override {
		module->bendRangeDown = bendRangeDown;
	}
};


struct BendRangeDownItem : MenuItem {
	DuoMIDI_CV* module;
	Menu* createChildMenu() override {
		Menu* menu = new Menu;
		std::vector<float> ranges = {2.f, 5.f, 7.f, 12.f, 24.f, 36.f, 48.f, 60.f, 72.f, 84.f, 96.f};
		for (size_t i = 0; i < ranges.size(); i++) {
			BendRangeDownValueItem* item = new BendRangeDownValueItem;
			item->text = string::f("- %d", static_cast<int>(ranges[i]));
			item->rightText = CHECKMARK(module->bendRangeDown == ranges[i]);
			item->module = module;
			item->bendRangeDown = ranges[i];
			menu->addChild(item);
		}
		return menu;
	}
};


struct Channel1ValueItem : MenuItem {
	DuoMIDI_CV* module;
	int channels;
	void onAction(const event::Action& e) override {
		module->setChannels1(channels);
	}
};

struct Channel2ValueItem : MenuItem {
	DuoMIDI_CV* module;
	int channels;
	void onAction(const event::Action& e) override {
		module->setChannels2(channels);
	}
};


struct Channel1Item : MenuItem {
	DuoMIDI_CV* module;
	Menu* createChildMenu() override {
		Menu* menu = new Menu;
		for (int channels = 1; channels <= 16; channels++) {
			Channel1ValueItem* item = new Channel1ValueItem;
			item->text = string::f("%d", channels);
			item->rightText = CHECKMARK(module->channels1 == channels);
			item->module = module;
			item->channels = channels;
			menu->addChild(item);
		}
		return menu;
	}
};


struct Channel2Item : MenuItem {
	DuoMIDI_CV* module;
	Menu* createChildMenu() override {
		Menu* menu = new Menu;
		for (int channels = 0; channels <= 16; channels++) {
			Channel2ValueItem* item = new Channel2ValueItem;
			item->text = string::f("%d", channels);
			item->rightText = CHECKMARK(module->channels2 == channels);
			item->module = module;
			item->channels = channels;
			menu->addChild(item);
		}
		return menu;
	}
};


struct PolyModeValueItem : MenuItem {
	DuoMIDI_CV* module;
	DuoMIDI_CV::PolyMode polyMode;
	void onAction(const event::Action& e) override {
		module->setPolyMode(polyMode);
	}
};

struct MPEModeValueItem : MenuItem {
	DuoMIDI_CV* module;
	DuoMIDI_CV::MPEMode mpeMode;
	void onAction(const event::Action& e) override {
		module->setPolyMode(DuoMIDI_CV::MPE_MODE);
		module->setMPEMode(mpeMode);
	}
};


struct MPEModeItem : MenuItem {
	DuoMIDI_CV* module;
	Menu* createChildMenu() override {
		Menu* menu = new Menu;
		std::vector<std::string> mpeModeNames = {
			"Direct",
			"Rotate",
			"Reuse",
			"Reset",
		};
		for (int i = 0; i < DuoMIDI_CV::NUM_MPE_MODES; i++) {
			DuoMIDI_CV::MPEMode mpeMode = (DuoMIDI_CV::MPEMode) i;
			MPEModeValueItem* item = new MPEModeValueItem;
			item->text = mpeModeNames[i];
			if (module->polyMode == DuoMIDI_CV::MPE_MODE)
				item->rightText = CHECKMARK(module->mpeMode == mpeMode);
			item->module = module;
			item->mpeMode = mpeMode;
			menu->addChild(item);
		}
		return menu;
	}
};


struct PolyModeItem : MenuItem {
	DuoMIDI_CV* module;
	Menu* createChildMenu() override {
		Menu* menu = new Menu;
		std::vector<std::string> polyModeNames = {
			"Rotate",
			"Reuse",
			"Reset",
		};
		for (int i = 0; i < (DuoMIDI_CV::NUM_POLY_MODES - 1); i++) { //Do not iterate over MPE Mode
			DuoMIDI_CV::PolyMode polyMode = (DuoMIDI_CV::PolyMode) i;
			PolyModeValueItem* item = new PolyModeValueItem;
			item->text = polyModeNames[i];
			item->rightText = CHECKMARK(module->polyMode == polyMode);
			item->module = module;
			item->polyMode = polyMode;
			menu->addChild(item);
		}
		MPEModeItem* mpeModeItem = new MPEModeItem;
		mpeModeItem->text = "MPE mode";
		mpeModeItem->rightText = RIGHT_ARROW;
		mpeModeItem->module = module;
		menu->addChild(mpeModeItem);
		return menu;
	}
};


struct DuoMIDI_CVPanicItem : MenuItem {
	DuoMIDI_CV* module;
	void onAction(const event::Action& e) override {
		module->panic();
	}
};


struct DuoMIDI_CVWidget : ModuleWidget {
	DuoMIDI_CVWidget(DuoMIDI_CV* module) {
		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/DuoMIDI-CV.svg")));

		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addOutput(createOutput<DLXPortPoly>(Vec(16.369, 176.168), module, DuoMIDI_CV::PITCH1_OUTPUT));
		addOutput(createOutput<DLXPortPoly>(Vec(45.569, 176.168), module, DuoMIDI_CV::PITCH2_OUTPUT));
		addOutput(createOutput<DLXPortPoly>(Vec(79.969, 176.168), module, DuoMIDI_CV::GATE1_OUTPUT));
		addOutput(createOutput<DLXPortPoly>(Vec(109.170, 176.168), module, DuoMIDI_CV::GATE2_OUTPUT));
		addOutput(createOutput<DLXPortPoly>(Vec(143.571, 176.168), module, DuoMIDI_CV::VELOCITY1_OUTPUT));
		addOutput(createOutput<DLXPortPoly>(Vec(172.772, 176.168), module, DuoMIDI_CV::VELOCITY2_OUTPUT));
		addOutput(createOutput<DLXPortPoly>(Vec(16.369, 220.057), module, DuoMIDI_CV::BENT_PITCH1_OUTPUT));
		addOutput(createOutput<DLXPortPoly>(Vec(45.569, 220.057), module, DuoMIDI_CV::BENT_PITCH2_OUTPUT));
		addOutput(createOutput<DLXPortPoly>(Vec(79.969, 220.057), module, DuoMIDI_CV::RETRIGGER1_OUTPUT));
		addOutput(createOutput<DLXPortPoly>(Vec(109.170, 220.057), module, DuoMIDI_CV::RETRIGGER2_OUTPUT));
		addOutput(createOutput<DLXPortPoly>(Vec(143.571, 220.057), module, DuoMIDI_CV::MOD1_OUTPUT));
		addOutput(createOutput<DLXPortPoly>(Vec(172.772, 220.057), module, DuoMIDI_CV::MOD2_OUTPUT));
		addOutput(createOutput<DLXPortPoly>(Vec(16.369, 263.946), module, DuoMIDI_CV::PITCH_BEND1_OUTPUT));
		addOutput(createOutput<DLXPortPoly>(Vec(45.569, 263.946), module, DuoMIDI_CV::PITCH_BEND2_OUTPUT));
		addInput(createInput<DLXPortPolyOut>(Vec(79.969, 263.946), module, DuoMIDI_CV::NOTESTOP1_INPUT));
		addInput(createInput<DLXPortPolyOut>(Vec(109.170, 263.946), module, DuoMIDI_CV::NOTESTOP2_INPUT));
		addOutput(createOutput<DLXPortPoly>(Vec(143.571, 263.946), module, DuoMIDI_CV::AFTERTOUCH1_OUTPUT));
		addOutput(createOutput<DLXPortPoly>(Vec(172.772, 263.946), module, DuoMIDI_CV::AFTERTOUCH2_OUTPUT));
		addOutput(createOutput<DLXPortPoly>(Vec(34.069, 307.833), module, DuoMIDI_CV::START_OUTPUT));
		addOutput(createOutput<DLXPortPoly>(Vec(63.270, 307.833), module, DuoMIDI_CV::STOP_OUTPUT));
		addOutput(createOutput<DLXPortPoly>(Vec(92.469, 307.833), module, DuoMIDI_CV::CONTINUE_OUTPUT));
		addOutput(createOutput<DLXPortPoly>(Vec(126.371, 307.833), module, DuoMIDI_CV::CLOCK_OUTPUT));
		addOutput(createOutput<DLXPortPoly>(Vec(155.572, 307.833), module, DuoMIDI_CV::CLOCK_DIV_OUTPUT));


		MidiWidget* midiWidget = createWidget<MidiWidget>(Vec(54.892, 43.941));
		midiWidget->box.size = Vec(100.217, 82.922);
		midiWidget->setMidiPort(module ? &module->midiInput : NULL);
		addChild(midiWidget);
	}

	void appendContextMenu(Menu* menu) override {
		DuoMIDI_CV* module = dynamic_cast<DuoMIDI_CV*>(this->module);

		menu->addChild(new MenuEntry);

		ClockDivisionItem* clockDivisionItem = new ClockDivisionItem;
		clockDivisionItem->text = "CLK/N divider";
		clockDivisionItem->rightText = RIGHT_ARROW;
		clockDivisionItem->module = module;
		menu->addChild(clockDivisionItem);

		BendRangeUpItem* bendRangeUpItem = new BendRangeUpItem;
		bendRangeUpItem->text = "Pitch bend up range";
		bendRangeUpItem->rightText = RIGHT_ARROW;
		bendRangeUpItem->module = module;
		menu->addChild(bendRangeUpItem);

		BendRangeDownItem* bendRangeDownItem = new BendRangeDownItem;
		bendRangeDownItem->text = "Pitch bend down range";
		bendRangeDownItem->rightText = RIGHT_ARROW;
		bendRangeDownItem->module = module;
		menu->addChild(bendRangeDownItem);

		Channel1Item* channel1Item = new Channel1Item;
		channel1Item->text = "Output 1 Polyphony channels";
		channel1Item->rightText = string::f("%d", module->channels1) + " " + RIGHT_ARROW;
		channel1Item->module = module;
		menu->addChild(channel1Item);

		Channel2Item* channel2Item = new Channel2Item;
		channel2Item->text = "Output 2 Polyphony channels";
		channel2Item->rightText = string::f("%d", module->channels2) + " " + RIGHT_ARROW;
		channel2Item->module = module;
		menu->addChild(channel2Item);

		PolyModeItem* polyModeItem = new PolyModeItem;
		polyModeItem->text = "Polyphony mode";
		polyModeItem->rightText = RIGHT_ARROW;
		polyModeItem->module = module;
		menu->addChild(polyModeItem);

		DuoMIDI_CVPanicItem* panicItem = new DuoMIDI_CVPanicItem;
		panicItem->text = "Panic";
		panicItem->module = module;
		menu->addChild(panicItem);
	}
};


Model* modelDuoMIDI_CV = createModel<DuoMIDI_CV, DuoMIDI_CVWidget>("DuoMIDI_CV");
