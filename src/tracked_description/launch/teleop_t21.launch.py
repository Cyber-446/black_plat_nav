# bringup_t21_with_rviz.launch.py
# ─────────────────────────────────────────────────────────────────────────────
# Поднимает:
#   • robot_state_publisher  +  ros2_control_node  (+ controllers.yaml)
#   • joint_state_broadcaster + 3 ForwardCommandController-а
#   • RViz2  (rviz:=false — выключить)
#   • joy_node  →  teleop_twist_joy  →  cmd_vel_to_float
# ─────────────────────────────────────────────────────────────────────────────
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.conditions import IfCondition
from launch_ros.actions import Node
from launch.substitutions import (
    Command, FindExecutable, LaunchConfiguration, PathJoinSubstitution
)
from launch_ros.substitutions import FindPackageShare

PKG = FindPackageShare("tracked_description")


def generate_launch_description() -> LaunchDescription:
    # ── аргументы ────────────────────────────────────────────────────────────
    prefix_arg = DeclareLaunchArgument("prefix", default_value="")
    rviz_arg   = DeclareLaunchArgument("rviz",   default_value="true",
                                       description="true/false — запускать RViz")

    # ── robot_description из xacro ───────────────────────────────────────────
    robot_description = {
        "robot_description": Command([
            FindExecutable(name="xacro"), " ",
            PathJoinSubstitution([PKG, "urdf", "t21.urdf.xacro"]), " ",
            "prefix:=", LaunchConfiguration("prefix")
        ])
    }

    # ── общий YAML-файл с параметрами контроллеров ───────────────────────────
    yaml_file = PathJoinSubstitution([PKG, "config", "controllers.yaml"])
    cm_ns = "/controller_manager"

    # ── базовые узлы робота ──────────────────────────────────────────────────
    bringup = [
        Node(  # TF + /robot_description
            package="robot_state_publisher",
            executable="robot_state_publisher",
            parameters=[robot_description],
            output="screen"),

        Node(  # ros2_control_node (получает robot_description + YAML)
            package="controller_manager",
            executable="ros2_control_node",
            parameters=[robot_description, yaml_file],
            output="screen"),

        # joint_state_broadcaster
        Node(package="controller_manager", executable="spawner",
             arguments=[
                 "joint_state_broadcaster",
                 "--controller-manager", cm_ns,
                 "--param-file", yaml_file],
             output="screen"),

        # lin_vel_controller
        Node(package="controller_manager", executable="spawner",
             arguments=[
                 "lin_vel_controller",
                 "--controller-manager", cm_ns,
                 "--controller-type",
                 "forward_command_controller/ForwardCommandController",
                 "--param-file", yaml_file],
             output="screen"),

        # ang_vel_controller
        Node(package="controller_manager", executable="spawner",
             arguments=[
                 "ang_vel_controller",
                 "--controller-manager", cm_ns,
                 "--controller-type",
                 "forward_command_controller/ForwardCommandController",
                 "--param-file", yaml_file],
             output="screen"),

        # geom_position_controller
        Node(package="controller_manager", executable="spawner",
             arguments=[
                 "geom_position_controller",
                 "--controller-manager", cm_ns,
                 "--controller-type",
                 "forward_command_controller/ForwardCommandController",
                 "--param-file", yaml_file],
             output="screen"),
    ]

    # ── RViz2 ────────────────────────────────────────────────────────────────
    rviz_cfg = PathJoinSubstitution([PKG, "rviz", "t21.rviz"])
    rviz = Node(
        package="rviz2",
        executable="rviz2",
        name="rviz2",
        arguments=["-d", rviz_cfg],
        condition=IfCondition(LaunchConfiguration("rviz")),
        output="screen")

    # ── блок джой-телеопа ────────────────────────────────────────────────────
    teleop_yaml = PathJoinSubstitution([PKG, "config", "ps4_teleop.yaml"])

    joy_node = Node(
        package="joy",
        executable="joy_node",
        name="joy_node",
        parameters=[{"dev": "/dev/input/js0"}],
        output="screen")

    teleop_node = Node(
        package="teleop_twist_joy",
        executable="teleop_node",
        name="teleop_twist_joy",
        arguments=["--ros-args", "--params-file", teleop_yaml],
        output="screen")

    cmd_conv = Node(
        package="tracked_description",
        executable="cmd_vel_to_float",
        name="cmd_vel_to_float",
        output="screen")

    # ── итоговый LaunchDescription ───────────────────────────────────────────
    return LaunchDescription(
        [prefix_arg, rviz_arg] +
        bringup +
        [rviz, joy_node, teleop_node, cmd_conv]
    )
