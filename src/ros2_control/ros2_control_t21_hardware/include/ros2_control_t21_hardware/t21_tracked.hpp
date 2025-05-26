/**
 *  t21_tracked.hpp
 *  ------------------------------------------------------------------
 *  ROS 2 Control SystemInterface для гусеничной платформы Т-21.
 *  ------------------------------------------------------------------
 */
#pragma once

#include <array>
#include <vector>

#include "hardware_interface/system_interface.hpp"
#include "hardware_interface/types/hardware_interface_return_values.hpp"
#include "rclcpp/macros.hpp"
#include "rclcpp/clock.hpp"
#include "rclcpp/time.hpp"
#include "rclcpp_lifecycle/node_interfaces/lifecycle_node_interface.hpp"

#include "udp_tracked.hpp"
#include "visibility_control.h"

namespace t21_hardware
{

class T21TrackedHardware final : public hardware_interface::SystemInterface
{
public:
  RCLCPP_SHARED_PTR_DEFINITIONS(T21TrackedHardware)

  ROS2_CONTROL_T21_HARDWARE_PUBLIC
  hardware_interface::CallbackReturn on_init(
      const hardware_interface::HardwareInfo & info) override;

  ROS2_CONTROL_T21_HARDWARE_PUBLIC
  std::vector<hardware_interface::StateInterface> export_state_interfaces() override;

  ROS2_CONTROL_T21_HARDWARE_PUBLIC
  std::vector<hardware_interface::CommandInterface> export_command_interfaces() override;

  ROS2_CONTROL_T21_HARDWARE_PUBLIC
  hardware_interface::CallbackReturn on_activate(
      const rclcpp_lifecycle::State & /*previous_state*/) override;

  ROS2_CONTROL_T21_HARDWARE_PUBLIC
  hardware_interface::CallbackReturn on_deactivate(
      const rclcpp_lifecycle::State & /*previous_state*/) override;

  ROS2_CONTROL_T21_HARDWARE_PUBLIC
  hardware_interface::return_type read(
      const rclcpp::Time & /*time*/,
      const rclcpp::Duration & /*period*/) override;

  ROS2_CONTROL_T21_HARDWARE_PUBLIC
  hardware_interface::return_type write(
      const rclcpp::Time & /*time*/,
      const rclcpp::Duration & /*period*/) override;

private:
  tracked_platform_udp::EthTrackedSocket socket_{};

  // индексы: 0-linVel, 1-angVel, 2-geomPos
  std::array<double,3> cmd_{0.0, 0.0, 0.0};
  std::array<double,3> state_{0.0, 0.0, 0.0};

  // локально интегрируемые позиции виртуальных суставов
  double lin_pos_{0.0};
  double ang_pos_{0.0};

  rclcpp::Time last_time_;
  bool first_read_{true};
};

}  // namespace t21_hardware
