from launch import LaunchDescription
from launch_ros.actions import Node
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration, PathJoinSubstitution
from ament_index_python.packages import get_package_share_directory
import os

package_name='t21_rtabmap'

def generate_launch_description():
    # Путь к YAML-файлу параметров
    default_params_file = os.path.join(
        get_package_share_directory(package_name),
        'config',
        'camera.yaml'
    )
    
    # Corrected remappings list
    camera_remaps = [
        ('/camera/color/camera_info', '/camera/camera_info'),
        ('/camera/color/image_raw', '/camera/image_raw'),
        ('/camera/depth/image_rect_raw', '/camera/depth/image_raw'),
        ('/camera/depth/color/points', '/camera/points'),
    ]
    
    return LaunchDescription([
        # Аргументы для переопределения
        DeclareLaunchArgument('camera_name', default_value='camera'),
        DeclareLaunchArgument('camera_namespace', default_value=''),
        DeclareLaunchArgument('config_file', default_value=default_params_file),
        
        # Corrected camera node
        Node(
            package='realsense2_camera',
            executable='realsense2_camera_node',
            name=LaunchConfiguration('camera_name'),
            namespace=LaunchConfiguration('camera_namespace'),
            parameters=[LaunchConfiguration('config_file')],
            output='screen',
            remappings=camera_remaps  
        )
    ])