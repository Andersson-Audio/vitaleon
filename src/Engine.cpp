/* Copyright 2013-2019 Matt Tytel
 *
 * vital is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * vital is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with vital.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "engine/fir_halfband_decimator.cpp"
#include "engine/digital_svf.cpp"
#include "engine/dc_filter.cpp"
#include "engine/linkwitz_riley_filter.cpp"
#include "engine/synth_filter.cpp"
#include "engine/formant_filter.cpp"
#include "engine/decimator.cpp"
#include "engine/upsampler.cpp"
#include "engine/diode_filter.cpp"
#include "engine/comb_filter.cpp"
#include "engine/vocal_tract.cpp"
#include "engine/formant_manager.cpp"
#include "engine/dirty_filter.cpp"
#include "engine/iir_halfband_decimator.cpp"
#include "engine/sallen_key_filter.cpp"
#include "engine/phaser_filter.cpp"
#include "engine/ladder_filter.cpp"
#include "engine/synth_oscillator.cpp"
#include "engine/sample_source.cpp"
#include "engine/wave_frame.cpp"
#include "engine/wavetable.cpp"
#include "engine/utils.cpp"
#include "engine/feedback.cpp"
#include "engine/voice_handler.cpp"
#include "engine/processor.cpp"
#include "engine/synth_module.cpp"
#include "engine/operators.cpp"
#include "engine/processor_router.cpp"
#include "engine/value.cpp"
#include "engine/trigger_random.cpp"
#include "engine/synth_lfo.cpp"
#include "engine/random_lfo.cpp"
#include "engine/line_map.cpp"
#include "engine/envelope.cpp"
#include "engine/smooth_value.cpp"
#include "engine/peak_meter.cpp"
#include "engine/value_switch.cpp"
#include "engine/legato_filter.cpp"
#include "engine/portamento_slope.cpp"
#include "engine/filter_module.cpp"
#include "engine/filters_module.cpp"
#include "engine/envelope_module.cpp"
#include "engine/lfo_module.cpp"
#include "engine/random_lfo_module.cpp"
#include "engine/reorderable_effect_chain.cpp"
#include "engine/chorus_module.cpp"
#include "engine/modulation_connection_processor.cpp"
#include "engine/equalizer_module.cpp"
#include "engine/distortion_module.cpp"
#include "engine/flanger_module.cpp"
#include "engine/reverb_module.cpp"
#include "engine/delay_module.cpp"
#include "engine/comb_module.cpp"
#include "engine/compressor_module.cpp"
#include "engine/formant_module.cpp"
#include "engine/sample_module.cpp"
#include "engine/oscillator_module.cpp"
#include "engine/phaser_module.cpp"
#include "engine/producers_module.cpp"
#include "engine/phaser.cpp"
#include "engine/distortion.cpp"
#include "engine/compressor.cpp"
#include "engine/delay.cpp"
#include "engine/reverb.cpp"
#include "engine/sound_engine.cpp"
#include "engine/synth_voice_handler.cpp"

#include "engine/line_generator.cpp"
#include "engine/synth_parameters.cpp"
#include "engine/synth_types.cpp"
