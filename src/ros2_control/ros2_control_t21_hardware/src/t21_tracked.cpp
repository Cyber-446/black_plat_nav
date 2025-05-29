/**
 *  t21_tracked.cpp
 *  ------------------------------------------------------------------
 */

#include "ros2_control_t21_hardware/t21_tracked.hpp"
#include "hardware_interface/types/hardware_interface_type_values.hpp"
#include "rclcpp/rclcpp.hpp"
#include <cmath>

namespace t21_hardware
{

// ────────────────────────────── on_init ───────────────────────────────────────
hardware_interface::CallbackReturn
T21TrackedHardware::on_init(const hardware_interface::HardwareInfo & info)
{
  if (hardware_interface::SystemInterface::on_init(info) !=
      hardware_interface::CallbackReturn::SUCCESS)
  {
    return hardware_interface::CallbackReturn::ERROR;
  }

  if (info_.joints.size() != 3)
  {
    RCLCPP_FATAL(rclcpp::get_logger("T21TrackedHardware"),
                 "URDF должен содержать 3 сустава (left, right, geom). Найдено %zu",
                 info_.joints.size());
    return hardware_interface::CallbackReturn::ERROR;
  }

  const char *exp_cmd[3]   = {
    hardware_interface::HW_IF_VELOCITY,
    hardware_interface::HW_IF_VELOCITY,
    hardware_interface::HW_IF_POSITION
  };
  const char *exp_state0[3] = {
    hardware_interface::HW_IF_VELOCITY,
    hardware_interface::HW_IF_VELOCITY,
    hardware_interface::HW_IF_POSITION
  };

  for (size_t i = 0; i < 3; ++i)
  {
    if (info_.joints[i].command_interfaces[0].name != exp_cmd[i] ||
        info_.joints[i].state_interfaces[0].name   != exp_state0[i])
    {
      RCLCPP_FATAL(
        rclcpp::get_logger("T21TrackedHardware"),
        "Joint %s: ожидался cmd '%s', state '%s'",
        info_.joints[i].name.c_str(),
        exp_cmd[i], exp_state0[i]
      );
      return hardware_interface::CallbackReturn::ERROR;
    }
  }

  return hardware_interface::CallbackReturn::SUCCESS;
}

// ───────────────────── export_state_interfaces ───────────────────────────────
std::vector<hardware_interface::StateInterface>
T21TrackedHardware::export_state_interfaces()
{
  std::vector<hardware_interface::StateInterface> si;

  // wheel_left_joint
  si.emplace_back("wheel_left_joint",
                  hardware_interface::HW_IF_VELOCITY,
                  &state_[0]);
  si.emplace_back("wheel_left_joint",
                  hardware_interface::HW_IF_POSITION,
                  &lin_pos_);

  // wheel_right_joint
  si.emplace_back("wheel_right_joint",
                  hardware_interface::HW_IF_VELOCITY,
                  &state_[1]);
  si.emplace_back("wheel_right_joint",
                  hardware_interface::HW_IF_POSITION,
                  &ang_pos_);

  // geom_joint (flipper)
  si.emplace_back("geom_joint",
                  hardware_interface::HW_IF_POSITION,
                  &state_[2]);

  return si;
}

// ─────────────────── export_command_interfaces ───────────────────────────────
std::vector<hardware_interface::CommandInterface>
T21TrackedHardware::export_command_interfaces()
{
  std::vector<hardware_interface::CommandInterface> ci;

  ci.emplace_back("wheel_left_joint",
                  hardware_interface::HW_IF_VELOCITY,
                  &cmd_[0]);
  ci.emplace_back("wheel_right_joint",
                  hardware_interface::HW_IF_VELOCITY,
                  &cmd_[1]);
  ci.emplace_back("geom_joint",
                  hardware_interface::HW_IF_POSITION,
                  &cmd_[2]);

  return ci;
}

// ────────────────────────── activate / deactivate ────────────────────────────
hardware_interface::CallbackReturn
T21TrackedHardware::on_activate(const rclcpp_lifecycle::State &)
{
  RCLCPP_INFO(rclcpp::get_logger("T21TrackedHardware"),
              "Hardware interface activated");
  first_read_ = true;
  lin_pos_ = ang_pos_ = 0.0;
  last_time_ = rclcpp::Clock().now();
  return hardware_interface::CallbackReturn::SUCCESS;
}

hardware_interface::CallbackReturn
T21TrackedHardware::on_deactivate(const rclcpp_lifecycle::State &)
{
  RCLCPP_INFO(rclcpp::get_logger("T21TrackedHardware"),
              "Hardware interface deactivated");
  return hardware_interface::CallbackReturn::SUCCESS;
}

// ───────────────────────────────── read ───────────────────────────────────────
hardware_interface::return_type
T21TrackedHardware::read(const rclcpp::Time & /*time*/,
                         const rclcpp::Duration & period)
{
  tracked_platform_udp::Packet128 pkt;
  const bool got = socket_.receiveState(pkt);

  // первый проход: эхо команд
  if (first_read_)
  {
    first_read_ = false;
    if (!got)
    {
      state_[0] = cmd_[0];
      state_[1] = cmd_[1];
      state_[2] = cmd_[2];
    }
    last_time_ = rclcpp::Clock().now();
  }

  RCLCPP_INFO(
      rclcpp::get_logger("T21TrackedHardware"),
      "geom raw=%.2f°, rad=%.3f",
      pkt.geomPos, pkt.geomPos * M_PI/180.0);

  // обновляем состояния
  if (got)
  {
    state_[0] = pkt.linVel;
    state_[1] = pkt.angVel;
    state_[2] = pkt.geomPos * M_PI / 180.0;
    last_time_ = rclcpp::Clock().now();
  }

  // интегрируем виртуальные позиции
  double dt = period.seconds();
  if (dt > 0.1) dt = 0.1;
  lin_pos_ += state_[0] * dt;
  ang_pos_ += state_[1] * dt;

  // предупреждение о потере пакетов
  static rclcpp::Clock steady_clock(RCL_STEADY_TIME);
  static rclcpp::Time  last_ok = rclcpp::Clock().now();
  rclcpp::Time now = rclcpp::Clock().now();
  if (got) {
    last_ok = now;
  } else if ((now - last_ok).seconds() > 1.0) {
    RCLCPP_WARN_THROTTLE(
      rclcpp::get_logger("T21TrackedHardware"),
      steady_clock, 2000,
      "No UDP packets for %.1f s",
      (now - last_ok).seconds()
    );
  }

  return hardware_interface::return_type::OK;
}

// ───────────────────────────────── write ──────────────────────────────────────
hardware_interface::return_type
T21TrackedHardware::write(const rclcpp::Time &, const rclcpp::Duration &)
{
  // cmd_[0] = ω_l, cmd_[1] = ω_r, cmd_[2] = geom_pos (рад)
  double omega_l = cmd_[0];
  double omega_r = cmd_[1];
  double geom_pos_rad = cmd_[2];
  float  geom_deg     = static_cast<float>(geom_pos_rad * 180.0 / M_PI);

  // параметры платформы
  const double R = 0.07;  // радиус «колеса», м
  const double L = 0.38;  // база между «колёсами», м

  // дифференциальная кинематика
  double lin_vel = R * 0.5 * (omega_r + omega_l);
  double ang_vel = R * (omega_r - omega_l) / L;
  RCLCPP_INFO(
    rclcpp::get_logger("T21TrackedHardware"),
    "TX  lin=%.3f m/s  ang=%.3f rad/s  geom=%.1f°  (ω_l=%.2f, ω_r=%.2f)",
    lin_vel, ang_vel, geom_deg, omega_l, omega_r);

  socket_.sendCommand(
    static_cast<float>(lin_vel),
    static_cast<float>(ang_vel),
    geom_deg,
    /*geom_pos_mode=*/true
  );

  return hardware_interface::return_type::OK;
}

}  // namespace t21_hardware

// ───────────────────────── plugin export ──────────────────────────────────────
#include "pluginlib/class_list_macros.hpp"
PLUGINLIB_EXPORT_CLASS(
  t21_hardware::T21TrackedHardware,
  hardware_interface::SystemInterface
)
