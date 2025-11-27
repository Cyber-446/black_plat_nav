# bringup_t21_with_diff_drive.launch.py
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, IncludeLaunchDescription
from launch.conditions import IfCondition
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch_ros.actions import Node
from launch.substitutions import (
    Command, FindExecutable, LaunchConfiguration, PathJoinSubstitution
)
from launch_ros.substitutions import FindPackageShare


PKG = FindPackageShare("tracked_description")

def generate_launch_description() -> LaunchDescription:
    prefix_arg = DeclareLaunchArgument("prefix", default_value="")
    rviz_arg   = DeclareLaunchArgument(
        "rviz", default_value="true",
        description="Запускать RViz (true/false)"
    )
    use_lidar_arg  = DeclareLaunchArgument(
        name = "use_lidar",
        default_value = "true",
        description = "Use VLP-16" 
    )
    sim_lidar_arg = DeclareLaunchArgument(
        name = "sim_lidar",
        default_value = "false",
        description = "Use VLP-16 in URDF for Gazebo"
    )
    use_camera_arg = DeclareLaunchArgument(
        name="use_camera",
        default_value="false",
        description="Run RGBD camera nodes and enable URDF description"
    )
    use_rtabmap_arg = DeclareLaunchArgument(
        name="use_rtabmap",
        default_value="false",
        description="Use rtabmap SLAM approach"
    )
    use_sim_time = DeclareLaunchArgument(
            'use_sim_time',
            default_value='false',
            description='Use simulation (Gazebo) clock'
    )

    start_lidar = IncludeLaunchDescription(
    PythonLaunchDescriptionSource([
        PathJoinSubstitution([FindPackageShare("t21_lidar"), "launch", "t21_lidar.launch.py"])
    ]),
    condition=IfCondition(LaunchConfiguration("use_lidar"))
    )

    start_lidar = IncludeLaunchDescription(
    PythonLaunchDescriptionSource([
        PathJoinSubstitution([FindPackageShare("t21_lidar"), "launch", "t21_lidar.launch.py"])
    ]),
    condition=IfCondition(LaunchConfiguration("use_lidar"))
    )
    
    start_camera = IncludeLaunchDescription(
    PythonLaunchDescriptionSource([
        PathJoinSubstitution([FindPackageShare("t21_rtabmap"), "launch", "camera.launch.py"])
    ]),
    condition=IfCondition(LaunchConfiguration("use_camera"))
    )
    start_rtabmap = IncludeLaunchDescription(
    PythonLaunchDescriptionSource([
        PathJoinSubstitution([FindPackageShare("t21_rtabmap"), "launch", "t21_vslam.launch.py"])
    ]),
    condition=IfCondition(LaunchConfiguration("use_rtabmap"))
    )

    # Добавление Xsens launch
    # Добавление Xsens launch с передачей параметров
    xsens_launch = IncludeLaunchDescription(
        PythonLaunchDescriptionSource([
            PathJoinSubstitution([
                FindPackageShare('bluespace_ai_xsens_mti_driver'), 'launch', 'xsens_mti_node.launch.py'])
        ]),
        launch_arguments={
            # Основные настройки публикации данных
            'pub_angular_velocity': 'true',      # Включить угловую скорость
            'pub_acceleration': 'true',          # Включить линейное ускорение
            'pub_mag': 'false',                  # Отключить магнитометр (если не нужен)
            'pub_free_acceleration': 'false',    # Отключить свободное ускорение
            'pub_gnss': 'false',                 # Отключить GNSS данные
            'pub_twist': 'false',                # Отключить twist
            'pub_positionLLA': 'false',          # Отключить координаты LLA
            'pub_transform': 'false',            # Отключить трансформы
            'pub_dq': 'false',                   # Отключить delta quaternion
            'pub_dv': 'false',                   # Отключить delta velocity
        
            # Другие настройки (опционально)
            'frame_id': 'imu_link',              # Задать frame_id
            'scan_for_devices': 'true',          # Автопоиск устройства
            'device_id': '',                      # Пустой ID - использовать любое устройство

            # Настройки tf2 для связи СК IMU с СК родительского звена
            'parent_id': 'big_box_link', 
            'x_translation': '0.0',
            'y_translation': '0.08',
            'z_translation': '0.01',
        }.items()
    )

    
    robot_description = {
        "robot_description": Command([
            FindExecutable(name="xacro"), " ",
            PathJoinSubstitution([PKG, "urdf", "tracked_robot.urdf.xacro"]), " ",
            "prefix:=", LaunchConfiguration("prefix"), " ",
            #"use_lidar:=", LaunchConfiguration("sim_lidar")
        ])
    }

    yaml_file = PathJoinSubstitution([PKG, "config", "controllers.yaml"])
    cm_ns = "/controller_manager"

    nodes = [
        # публикуем описания робота
        Node(
            package="robot_state_publisher",
            executable="robot_state_publisher",
            parameters=[robot_description],
            output="screen"
        ),

        # запускаем ros2_control
        Node(
            package="controller_manager",
            executable="ros2_control_node",
            parameters=[robot_description],
            output="screen"
        ),

        # бродкастер состояний
        Node(
            package="controller_manager",
            executable="spawner",
            arguments=[
                "joint_state_broadcaster",
                "--controller-manager", cm_ns,
                "--controller-type", "joint_state_broadcaster/JointStateBroadcaster",
                "--param-file", yaml_file
            ],
            output="screen"
        ),

        # контроллер дифференциального привода
        Node(package="controller_manager", executable="spawner",
            arguments=[
                "diff_drive_controller",
                "--controller-manager", cm_ns,
                "--controller-type", "diff_drive_controller/DiffDriveController",
                "--param-file", yaml_file
            ],
            output="screen"),

        # контроллер флиппера
        Node(package="controller_manager", executable="spawner",
            arguments=[
                "geom_position_controller",
                "--controller-manager", cm_ns,
                "--controller-type", "forward_command_controller/ForwardCommandController",
                "--param-file", yaml_file
            ],
            output="screen"),
        
        # RViz
        Node(
            package="rviz2",
            executable="rviz2",
            name="rviz2",
            arguments=["-d", PathJoinSubstitution([PKG, "config", "t21.rviz"])],
            condition=IfCondition(LaunchConfiguration("rviz")),
            output="screen"
        ),

        xsens_launch,
    ]

    return LaunchDescription([prefix_arg, 
                              rviz_arg, 
                              use_lidar_arg,
                              sim_lidar_arg,
                              use_camera_arg,
                              use_rtabmap_arg,
                              start_lidar,
                              start_camera,
                              start_rtabmap,
                              ] 
                              + nodes)
