/**
 *  t21_tracked.cpp
 *  ------------------------------------------------------------------
 *  UDP ⇄ ROS-2: все величины в SI, учтён редуктор 30.75 : 1.
 */

#include "ros2_control_t21_hardware/t21_tracked.hpp"
#include "hardware_interface/types/hardware_interface_type_values.hpp"
#include "rclcpp/rclcpp.hpp"

#include <cmath>
#include <algorithm>          // std::min

namespace t21_hardware
{

/* ───── константы шасси ───── */
constexpr double G       = 30.75;    // редукция мотор-звезда
constexpr double R       = 0.065;    // м, физический радиус звезды
constexpr double L       = 0.33;     // м, база между траками

/* ─────────────────────────── on_init ─────────────────────────── */
hardware_interface::CallbackReturn
T21TrackedHardware::on_init(const hardware_interface::HardwareInfo &info)
{
  if (SystemInterface::on_init(info) != CallbackReturn::SUCCESS)
    return CallbackReturn::ERROR;

  if (info_.joints.size() != 3) {
    RCLCPP_FATAL(rclcpp::get_logger("T21TrackedHardware"),
                 "URDF должен содержать 3 сустава, найдено %zu", info_.joints.size());
    return CallbackReturn::ERROR;
  }

  const char *exp_cmd[3]   = {hardware_interface::HW_IF_VELOCITY,
                              hardware_interface::HW_IF_VELOCITY,
                              hardware_interface::HW_IF_POSITION};
  const char *exp_state[3] = {hardware_interface::HW_IF_VELOCITY,
                              hardware_interface::HW_IF_VELOCITY,
                              hardware_interface::HW_IF_POSITION};

  for (size_t i = 0; i < 3; ++i) {
    if (info_.joints[i].command_interfaces[0].name != exp_cmd[i] ||
        info_.joints[i].state_interfaces[0].name   != exp_state[i]) {
      RCLCPP_FATAL(rclcpp::get_logger("T21TrackedHardware"),
                   "Joint %s: ожидались cmd '%s', state '%s'",
                   info_.joints[i].name.c_str(), exp_cmd[i], exp_state[i]);
      return CallbackReturn::ERROR;
    }
  }
  return CallbackReturn::SUCCESS;
}

/* ───────────────────── state / command IF ───────────────────── */
std::vector<hardware_interface::StateInterface>
T21TrackedHardware::export_state_interfaces()
{
  std::vector<hardware_interface::StateInterface> si;
  si.reserve(5);

  si.emplace_back("wheel_left_joint",
                  hardware_interface::HW_IF_VELOCITY, &state_[0]);
  si.emplace_back("wheel_left_joint",
                  hardware_interface::HW_IF_POSITION,  &left_pos_);

  si.emplace_back("wheel_right_joint",
                  hardware_interface::HW_IF_VELOCITY, &state_[1]);
  si.emplace_back("wheel_right_joint",
                  hardware_interface::HW_IF_POSITION,  &right_pos_);

  si.emplace_back("geom_joint",
                  hardware_interface::HW_IF_POSITION,  &state_[2]);

  return si;
}

std::vector<hardware_interface::CommandInterface>
T21TrackedHardware::export_command_interfaces()
{
  std::vector<hardware_interface::CommandInterface> ci;
  ci.reserve(3);

  ci.emplace_back("wheel_left_joint",
                  hardware_interface::HW_IF_VELOCITY, &cmd_[0]);
  ci.emplace_back("wheel_right_joint",
                  hardware_interface::HW_IF_VELOCITY, &cmd_[1]);
  ci.emplace_back("geom_joint",
                  hardware_interface::HW_IF_POSITION,  &cmd_[2]);

  return ci;
}

/* ───────────────────────── activate / deactivate ─────────────── */
hardware_interface::CallbackReturn
T21TrackedHardware::on_activate(const rclcpp_lifecycle::State &)
{
  first_read_ = true;
  left_pos_ = right_pos_ = 0.0;
  last_time_ = rclcpp::Clock().now();
  RCLCPP_INFO(rclcpp::get_logger("T21TrackedHardware"), "Interface activated");
  return CallbackReturn::SUCCESS;
}

hardware_interface::CallbackReturn
T21TrackedHardware::on_deactivate(const rclcpp_lifecycle::State &)
{
  RCLCPP_INFO(rclcpp::get_logger("T21TrackedHardware"), "Interface deactivated");
  return CallbackReturn::SUCCESS;
}

/* ───────────────────────────── read ──────────────────────────── */
hardware_interface::return_type
T21TrackedHardware::read(const rclcpp::Time&, const rclcpp::Duration &period)
{
  tracked_platform_udp::Packet128 pkt;
  const bool got = socket_.receiveState(pkt);

  if (first_read_) {                       // первое чтение
    first_read_ = false;
    if (!got) state_ = cmd_;
    last_time_ = rclcpp::Clock().now();
  }

  const double rpm2rad = 2.0 * M_PI / 60.0;

  /* мотор-RPM → рад/с звезды (делим на редукцию) */
  const double omega_l = pkt.linVel * rpm2rad / G;
  const double omega_r = pkt.angVel * rpm2rad / G;
  const double geom_rad = pkt.geomPos * M_PI / 180.0;

  if (got) {
    state_[0] = omega_l;
    state_[1] = omega_r;
    state_[2] = geom_rad;
    last_time_ = rclcpp::Clock().now();
  }

  /* интеграция позиций */
  const double dt = std::min(period.seconds(), 0.1);
  left_pos_  += state_[0] * dt;
  right_pos_ += state_[1] * dt;

  return hardware_interface::return_type::OK;
}

/* ───────────────────────────── write ─────────────────────────── */
hardware_interface::return_type
T21TrackedHardware::write(const rclcpp::Time&, const rclcpp::Duration&)
{
  /* cmd_[] задаёт ω звезды (рад/с) */
  const double omega_l = cmd_[0];
  const double omega_r = cmd_[1];

  /* звёзды → скорость корпуса */
  const double lin_si = 0.5 * R * (omega_r + omega_l);      // м/с
  const double ang_si = (R / L) * (omega_r - omega_l);      // рад/с

  /* в пакет: см/с, град/с, град */
  const float lin_cm   = static_cast<float>(lin_si * 100.0f);
  const float ang_deg  = static_cast<float>(ang_si * 180.0 / M_PI);
  const float geom_deg = static_cast<float>(cmd_[2] * 180.0 / M_PI);

  socket_.sendCommand(lin_cm, ang_deg, geom_deg, true);
  return hardware_interface::return_type::OK;
}

} // namespace t21_hardware

/* ───────────── plugin export ───────────── */
#include "pluginlib/class_list_macros.hpp"
PLUGINLIB_EXPORT_CLASS(t21_hardware::T21TrackedHardware,
                       hardware_interface::SystemInterface)
