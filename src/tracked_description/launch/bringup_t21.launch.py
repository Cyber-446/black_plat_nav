# bringup_t21_with_rviz.launch.py
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
    # ── аргументы ─────────────────────────────────────────────────────────────
    prefix_arg = DeclareLaunchArgument("prefix", default_value="")
    rviz_arg   = DeclareLaunchArgument("rviz",   default_value="true",
                                       description="Запускать RViz (true/false)")

    # ── URDF → robot_description ─────────────────────────────────────────────
    robot_description = {
        "robot_description": Command([
            FindExecutable(name="xacro"), " ",
            PathJoinSubstitution([PKG, "urdf", "t21.urdf.xacro"]), " ",
            "prefix:=", LaunchConfiguration("prefix")
        ])
    }

    # ── YAML с контроллерами ─────────────────────────────────────────────────
    yaml_file = PathJoinSubstitution([PKG, "config", "controllers.yaml"])
    cm_ns = "/controller_manager"

    # ── узлы ─────────────────────────────────────────────────────────────────
    nodes = [
        Node(  # robot_state_publisher
            package="robot_state_publisher",
            executable="robot_state_publisher",
            parameters=[robot_description],
            output="screen"),

        Node(  # ros2_control_node
            package="controller_manager",
            executable="ros2_control_node",
            parameters=[robot_description],
            output="screen"),

        # joint_state_broadcaster
        Node(package="controller_manager", executable="spawner",
             arguments=["joint_state_broadcaster",
                        "--controller-manager", cm_ns,
                        "--controller-type", "joint_state_broadcaster/JointStateBroadcaster",
                        "--param-file", yaml_file],
             output="screen"),

        # lin_vel_controller
        Node(package="controller_manager", executable="spawner",
             arguments=["lin_vel_controller",
                        "--controller-manager", cm_ns,
                        "--controller-type", "forward_command_controller/ForwardCommandController",
                        "--param-file", yaml_file],
             output="screen"),

        # ang_vel_controller
        Node(package="controller_manager", executable="spawner",
             arguments=["ang_vel_controller",
                        "--controller-manager", cm_ns,
                        "--controller-type", "forward_command_controller/ForwardCommandController",
                        "--param-file", yaml_file],
             output="screen"),

        # geom_position_controller
        Node(package="controller_manager", executable="spawner",
             arguments=["geom_position_controller",
                        "--controller-manager", cm_ns,
                        "--controller-type", "forward_command_controller/ForwardCommandController",
                        "--param-file", yaml_file],
             output="screen"),
    ]

    # ── RViz 2 ────────────────────────────────────────────────────────────────
    rviz_cfg = PathJoinSubstitution([PKG, "tracked_description", "t21.rviz"])
    nodes.append(
        Node(package="rviz2",
             executable="rviz2",
             name="rviz2",
             arguments=["-d", rviz_cfg],
             condition=IfCondition(LaunchConfiguration("rviz")),
             output="screen")
    )

    return LaunchDescription([prefix_arg, rviz_arg] + nodes)
