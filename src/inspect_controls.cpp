#include "inspect_controls.h"
#include <istream>
#include <ostream>
#include <stdexcept>
#include <string>
#include "simulation/control_input.h"

void inspect_controls(std::istream &input, std::ostream &output) {
  using apache::simulation::control_configuration;
  using apache::simulation::control_input;
  using apache::simulation::decode_joystick_buttons;
  using apache::simulation::joystick_axis;
  using apache::simulation::joystick_curve;
  control_input state;
  std::string command;
  while(input >> command) {
    if(command == "frame") {
      unsigned int view{0};
      unsigned int count{0};
      bool gunner{false};
      bool alt{false};
      input >> view >> gunner >> alt;
      state.held.fill(false);
      state.counts.fill(0);
      input >> count;
      for(unsigned int i{0}; i != count; ++i) {
        unsigned int key{0};
        input >> key;
        state.held.at(key) = true;
      }
      input >> count;
      for(unsigned int i{0}; i != count; ++i) {
        unsigned int key{0};
        uint32_t value{0};
        input >> key >> value;
        state.counts.at(key) = value;
      }
      state.held[0x38] = alt;
      state.update(view, gunner);
      for(auto v : state.keyboard) output << v << ' ';
      for(auto v : state.flight.controls) output << v << ' ';
      for(auto v : state.external) output << v << ' ';
      output << '\n';
    } else if(command == "buttons") {
      unsigned int mode;
      unsigned int bits;
      unsigned int position;
      bool hat;
      input >> mode >> bits >> hat >> position;
      auto const out{decode_joystick_buttons(mode, bits, hat, position)};
      output << out.alternate << ' ' << out.count;
      for(size_t i{0}; i != out.count; ++i) output << ' ' << out.keys[i];
      output << '\n';
    } else if(command == "device") {
      input >> state.configuration.cyclic >> state.joystick_present >> state.hat_present >> state.hat >> state.axis_count >> state.joystick[3].value >> state.buttons;
      state.configuration.resolve();
      auto const out{state.button_commands()};
      output << out.alternate << ' ' << out.count;
      for(size_t i{0}; i != out.count; ++i) output << ' ' << out.keys[i];
      output << '\n';
    } else if(command == "setup") {
      control_configuration config;
      input >> config.cyclic >> config.collective >> config.tail_rotor;
      config.resolve();
      output << config.cyclic << ' ' << config.collective << ' ' << config.tail_rotor << ' ' << config.input_type << ' ' << config.absolute_throttle << ' ' << config.rudder << ' ' << config.analogue_hat << ' ' << config.button_mode << '\n';
    } else if(command == "axis") {
      unsigned int deadband{0};
      unsigned int mode{0};
      joystick_axis axis;
      input >> deadband >> mode >> axis.minimum >> axis.centre >> axis.maximum >> axis.value;
      output << joystick_curve{deadband}.sample(mode, axis) << '\n';
    } else if(command == "curve") {
      unsigned int deadband{0};
      input >> deadband;
      for(auto v : joystick_curve{deadband}.values) output << v << ' ';
      output << '\n';
    } else {
      throw std::runtime_error{"Unknown control-input command"};
    }
    if(!input) throw std::runtime_error{"Incomplete control-input command"};
  }
}
