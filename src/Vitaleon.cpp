// SPDX-License-Identifier: GPL-3.0-or-later
// Vitaleon by Andersson Audio. Experimental Vital-engine port, not official Vital.
// MIDI -> Vital voices / wavetable oscillators -> Vital filter / ADSR -> stereo.
// The base class retains the 0.1.x state schema for conversion. FullSynth.h adds the full engine,
// asset imports, modulation, effects and MPC graphics around the same DSP core.
#include "mpc_plugin.h"
#include "engine/sound_engine.h"
#include "engine/wave_frame.h"
#include "engine/wavetable.h"
#include "engine/synth_oscillator.h"
#include "engine/voice_handler.h"
#include <array>
#include <memory>

class VitaleonLegacy : public mpc::Plugin {
public:
    enum { Volume, Voices, Unison, Oscillators, WavePos, Cutoff, Resonance, Detune,
           Attack, Decay, Sustain, Release, Filter, Spectral, Warp, Bench, Count };
    VitaleonLegacy() : mpc::Plugin(makeInfo()) {
        addParam(mpc::floatParam("volume", "Volume", -60, -6, -24, "dB", 1));
        // At most eight voices (0.3.24, user request): more than that is beyond the MPC's
        // processor with a full sound. A project saved with more is restored at eight.
        addParam(mpc::intParam("voices", "Voices", 1, 8, 4));
        addParam(mpc::intParam("unison", "Unison", 1, 8, 1));
        addParam(mpc::intParam("oscillators", "Oscillators", 1, 3, 1));
        addParam(mpc::floatParam("wave_pos", "Wave Pos", 0, 3, 2, "", 2));
        addParam(mpc::logParam("cutoff", "Cutoff", 40, 18000, 4000, "Hz", 0));
        addParam(mpc::floatParam("resonance", "Resonance", 0, 0.85f, 0.15f, "", 2));
        addParam(mpc::floatParam("detune", "Detune", 0, 30, 8, "%", 1));
        addParam(mpc::logParam("attack", "Attack", 1, 2000, 5, "ms", 0));
        addParam(mpc::logParam("decay", "Decay", 5, 3000, 200, "ms", 0));
        addParam(mpc::floatParam("sustain", "Sustain", 0, 1, 0.8f, "", 2));
        addParam(mpc::logParam("release", "Release", 5, 3000, 180, "ms", 0));
        addParam(mpc::boolParam("filter", "Filter", true));
        addParam(mpc::choiceParam("spectral", "Spectral", {"Off", "Formant", "Harmonic", "Smear"}, 0));
        addParam(mpc::floatParam("warp", "Warp", 0, 1, 0.5f, "", 2));
        auto bench = mpc::boolParam("bench", "Bench", false);
        bench.automatable = false;
        addParam(bench);
        engine_ = std::make_unique<vital::SoundEngine>();
        engine_->setStatusUpdatesEnabled(false);
        auto controls = engine_->getControls();
        auto find = [&](const std::string& key) {
            auto found = controls.find(key);
            if (found == controls.end()) throw std::runtime_error("Missing engine control: " + key);
            return found->second;
        };
        volume_ = find("volume"); polyphony_ = find("polyphony");
        filter_on_ = find("filter_1_on"); filter_cutoff_ = find("filter_1_cutoff");
        filter_resonance_ = find("filter_1_resonance");
        attack_ = find("env_1_attack"); decay_ = find("env_1_decay");
        sustain_ = find("env_1_sustain"); release_ = find("env_1_release");
        find("oversampling")->set(0);
        // Andersson Audio, 2026-11-13: reuse voices at the polyphony limit.
        // Vital's default kKill mode can render a second full chord while the
        // old voices fade for 50 ms, causing an avoidable CPU spike on MPC.
        // kSteal retains Vital's envelope/filter/phase transition handling.
        find("voice_override")->set(vital::VoiceHandler::kSteal);
        find("filter_1_model")->set(vital::constants::kDigital);
        find("filter_1_style")->set(0);
        find("filter_2_on")->set(0); find("sample_on")->set(0);
        for (const char* fx : {"chorus_on", "compressor_on", "delay_on", "distortion_on", "eq_on", "flanger_on", "phaser_on", "reverb_on"}) find(fx)->set(0);
        const vital::PredefinedWaveFrames::Shape shapes[] = {
            vital::PredefinedWaveFrames::kSin, vital::PredefinedWaveFrames::kTriangle,
            vital::PredefinedWaveFrames::kSaw, vital::PredefinedWaveFrames::kSquare};
        for (int i = 0; i < 3; ++i) {
            auto prefix = std::string("osc_") + std::to_string(i + 1) + "_";
            auto& o = osc_[i];
            o.on = find(prefix + "on"); o.unison = find(prefix + "unison_voices");
            o.detune = find(prefix + "unison_detune"); o.position = find(prefix + "wave_frame");
            o.spectral = find(prefix + "spectral_morph_type"); o.warp = find(prefix + "spectral_morph_amount");
            find(prefix + "destination")->set(vital::constants::kFilter1);
            find(prefix + "level")->set(0.5f); find(prefix + "random_phase")->set(0.75f);
            find(prefix + "tune")->set(i == 0 ? 0 : (i == 1 ? -0.07f : 0.07f));
            auto* table = engine_->getWavetable(i);
            table->setNumFrames(4);
            for (int frame = 0; frame < 4; ++frame)
                table->loadWaveFrame(vital::PredefinedWaveFrames::getWaveFrame(shapes[frame]), frame);
        }
    }
    void prepare(double sampleRate, int) override {
        engine_->setSampleRate(int(sampleRate)); engine_->checkOversampling();
        controlsSynced_ = false;
        syncControls();
        engine_->noteOn(60, .1f, 0, 0); engine_->process(128); engine_->allSoundsOff();
    }
    void reset() override {
        engine_->allSoundsOff();
        engine_->process(128); // clear accumulated output from the previous voice set
        // Preserve the saved parameter; require Off -> On before restarting a saved benchmark.
        benchArmed_ = !getBool(Bench); benchmarkCount_ = 0;
        controlsSynced_ = false;
    }
    void process(mpc::ProcessContext& ctx) override {
        syncControls();
        if (!getBool(Bench)) benchArmed_ = true;
        const bool bench = getBool(Bench) && benchArmed_;
        const int desired = bench ? getInt(Voices) : 0;
        if (desired != benchmarkCount_) {
            engine_->allSoundsOff(); benchmarkCount_ = desired;
            for (int n = 0; n < desired; ++n) engine_->noteOn(48 + n, .65f, 0, 0);
        }
        int event = 0;
        for (int offset = 0; offset < ctx.numFrames;) {
            while (event < ctx.numMidiEvents && ctx.midi[event].sampleOffset <= offset) {
                if (!bench) handleMidi(ctx.midi[event], 0);
                ++event;
            }
            int frames = std::min(128, ctx.numFrames - offset);
            // Split at MIDI boundaries: avoid upstream voice envelopes changing samples
            // before a mid-block trigger. Events at one boundary retain their input order.
            if (!bench && event < ctx.numMidiEvents)
                frames = std::min(frames, ctx.midi[event].sampleOffset - offset);
            engine_->process(frames);
            const auto* out = engine_->output()->buffer;
            for (int i = 0; i < frames; ++i) {
                ctx.outputs[0][offset + i] = out[i][0];
                ctx.outputs[1][offset + i] = out[i][1];
            }
            offset += frames;
        }
    }
    int activeVoices() const { return engine_->getNumActiveVoices(); }
protected:
    struct OscControls { vital::Value *on, *unison, *detune, *position, *spectral, *warp; };
    std::unique_ptr<vital::SoundEngine> engine_;
    std::array<OscControls, 3> osc_;
    vital::Value *volume_, *polyphony_, *filter_on_, *filter_cutoff_, *filter_resonance_;
    vital::Value *attack_, *decay_, *sustain_, *release_;
    int benchmarkCount_ = 0;
    bool benchArmed_ = true;
    std::array<float, Count> syncedParameters_{};
    bool controlsSynced_ = false;
    void syncControls() {
        // Andersson Audio, 2026-11-12: host values are already atomic. Avoid
        // repeating log/pow conversions and control-buffer writes when static.
        bool changed = !controlsSynced_;
        for (int i = 0; i < Count; ++i) {
            const float value = getNormalised(i);
            changed |= value != syncedParameters_[i];
            syncedParameters_[i] = value;
        }
        if (!changed) return;
        controlsSynced_ = true;
        const float db = get(Volume) + 80;
        volume_->set(db * db); polyphony_->set(float(getInt(Voices)));
        engine_->setVoiceLimit(getInt(Voices));
        filter_on_->set(getBool(Filter) ? 1 : 0);
        filter_cutoff_->set(69 + 12 * std::log2(get(Cutoff) / 440));
        filter_resonance_->set(get(Resonance));
        attack_->set(std::sqrt(std::sqrt(get(Attack) * .001f)));
        decay_->set(std::sqrt(std::sqrt(get(Decay) * .001f)));
        sustain_->set(getBool(Bench) ? 1 : get(Sustain));
        release_->set(std::sqrt(std::sqrt(get(Release) * .001f)));
        constexpr int modes[] = {vital::SynthOscillator::kNoSpectralMorph,
            vital::SynthOscillator::kFormScale, vital::SynthOscillator::kHarmonicScale, vital::SynthOscillator::kSmear};
        for (int i = 0; i < 3; ++i) {
            auto& o = osc_[i]; o.on->set(i < getInt(Oscillators) ? 1 : 0);
            o.unison->set(float(getInt(Unison))); o.detune->set(std::sqrt(get(Detune)));
            o.position->set(get(WavePos)); o.spectral->set(float(modes[getInt(Spectral)])); o.warp->set(get(Warp));
        }
    }
    void handleMidi(const mpc::MidiEvent& e, int offset) {
        const int ch = e.channel();
        if (e.isNoteOn()) engine_->noteOn(e.note(), e.velocity() / 127.0f, offset, ch);
        else if (e.isNoteOff()) engine_->noteOff(e.note(), e.velocity() / 127.0f, offset, ch);
        else if (e.isPitchBend()) engine_->setPitchWheel(e.pitchBend() / 8192.0f, ch);
        else if (e.isController()) {
            if (e.controller() == 120) engine_->allSoundsOff();
            else if (e.controller() == 123) engine_->allNotesOffRange(offset, ch, ch);
            else if (e.controller() == 64) {
                if (e.controllerValue() >= 64) engine_->sustainOn(ch);
                else engine_->sustainOff(offset, ch);
            }
            else if (e.controller() == 1) engine_->setModWheel(e.controllerValue() / 127.0f, ch);
        }
        else if (e.isAftertouch()) engine_->setChannelAftertouch(ch, e.data[1] / 127.0f, offset);
    }
    static mpc::PluginInfo makeInfo() {
        mpc::PluginInfo info;
        info.name = "Vitaleon"; info.vendor = "Andersson Audio";
        info.uniqueId = MPC_FOURCC('A','l','7','q'); info.version = 319; info.latencySamples = 128; /* effect pipeline, see FullSynth.h */
        info.isInstrument = true; info.wantsMidi = true;
        info.numInputs = 0; info.numOutputs = 2;
        return info;
    }
};
#include "FullSynth.h"
#ifndef VITALEON_TEST
MPC_PLUGIN_ENTRY(Vitaleon)
#endif
