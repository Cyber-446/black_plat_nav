/**
 *  t21_tracked.cpp
 *  ------------------------------------------------------------------
 *  Конверсии откорректированы: UDP пакет ⇄ ROS всегда в SI-единицах.
 */

#include "ros2_control_t21_hardware/t21_tracked.hpp"
#include "hardware_interface/types/hardware_interface_type_values.hpp"
#include "rclcpp/rclcpp.hpp"
#include <cmath>

namespace t21_hardware
{

constexpr double SIGN_L = -1.0;   // левый мотор
constexpr double SIGN_R = -1.0;   // правый мотор
constexpr double R = 0.07;        // м, радиус «колеса»
constexpr double L = 0.38;        // м, база

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
                  &left_pos_);

  // wheel_right_joint
  si.emplace_back("wheel_right_joint",
                  hardware_interface::HW_IF_VELOCITY,
                  &state_[1]);
  si.emplace_back("wheel_right_joint",
                  hardware_interface::HW_IF_POSITION,
                  &right_pos_);

  // geom_joint
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
  left_pos_ = 0.0;
  right_pos_ = 0.0;
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

  // первый проход – эхо последних команд
  if (first_read_)
  {
    first_read_ = false;
    if (!got) {
      state_[0] = cmd_[0];
      state_[1] = cmd_[1];
      state_[2] = cmd_[2];
    }
    last_time_ = rclcpp::Clock().now();
  }

  /* --------------------------------------------------------------------------
   *  1.  Конвертируем «сырые» данные пакета:
   *      pkt.linVel  → ωL_rpm (левый борт, об/мин)
   *      pkt.angVel  → ωR_rpm (правый борт, об/мин)
   *      pkt.geomPos → угол геометрии, град
   * -------------------------------------------------------------------------- */
  const double rpm_to_rad = 2.0 * M_PI / 60.0;      // коэффициент преобразования

  const double omega_l = SIGN_L * pkt.linVel * rpm_to_rad;  // рад/с
  const double omega_r = SIGN_R * pkt.angVel * rpm_to_rad;  // рад/с
  const double geom_rad = pkt.geomPos * M_PI / 180.0;       // рад

  /* 2.  Высчитываем шасси-одометрию (м/с, рад/с) для логов/диагностики. */
  const double lin_si = 0.5 * R * (omega_r + omega_l);      // м/с
  const double ang_si = (R / L) * (omega_r - omega_l);      // рад/с

  /* 3.  Обновляем состояния, которые выставляются в ROS-контроллеры. */
  if (got) {
    state_[0] = omega_l;   // скорость левого колеса, рад/с
    state_[1] = omega_r;   // скорость правого колеса, рад/с
    state_[2] = geom_rad;  // позиция флиппера,   рад
    last_time_ = rclcpp::Clock().now();
  }

  /* 4.  Интегрируем позиции колёс для StateInterface-ов позиции. */
  const double dt = std::min(period.seconds(), 0.1);
  left_pos_  += state_[0] * dt;
  right_pos_ += state_[1] * dt;

  /* 5.  Логируем в человекочитаемом виде. */
/*   RCLCPP_INFO(
    rclcpp::get_logger("T21TrackedHardware"),
    "RX  ωL=%.1f об/мин  ωR=%.1f об/мин  →  lin=%.3f м/с  ang=%.3f рад/с  geom=%.1f°",
    pkt.linVel, pkt.angVel, lin_si, ang_si, pkt.geomPos); */

  /* 6.  Предупреждение о потере пакетов. */
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
  const double omega_l_hw = SIGN_L * cmd_[0];
  const double omega_r_hw = SIGN_R * cmd_[1];

  /* обратная дифф-кинематика (SI) ------------------------------------------ */
  const double lin_si = 0.5 * R * (omega_r_hw + omega_l_hw);   // м/с
  const double ang_si = (R / L) * (omega_r_hw - omega_l_hw);   // рад/с

  /* перевод для пакета: м/с → см/с, рад/с → град/с ------------------------ */
  const float lin_cm  = static_cast<float>(lin_si * 100.0);            // см/с
  const float ang_deg = static_cast<float>(ang_si * 180.0 / M_PI);     // град/с
  const float geom_deg = static_cast<float>(cmd_[2] * 180.0 / M_PI);   // град

/*   RCLCPP_INFO(
    rclcpp::get_logger("T21TrackedHardware"),
    "TX  lin=%.2f см/с  ang=%.2f °/с  geom=%.1f°  (ω_l=%.2f, ω_r=%.2f)",
    lin_cm, ang_deg, geom_deg, omega_l_hw, omega_r_hw); */

  socket_.sendCommand(lin_cm, ang_deg, geom_deg, /*geom_pos_mode=*/true);

  return hardware_interface::return_type::OK;
}

}  // namespace t21_hardware

// ───────────────────────── plugin export ──────────────────────────────────────
#include "pluginlib/class_list_macros.hpp"
PLUGINLIB_EXPORT_CLASS(
  t21_hardware::T21TrackedHardware,
  hardware_interface::SystemInterface
)
