from launch import LaunchDescription
from launch_ros.actions import Node

def generate_launch_description():
    return LaunchDescription([
        Node(
            package='joy', executable='joy_node', name='joy_node',
            parameters=[{'deadzone': 0.05, 'autorepeat_rate': 20.0}]
        ),
        Node(
            package='t21_teleop', executable='joy_full_teleop',
            parameters=[{
                'axis_lin':      1,
                'axis_ang':      0,
                'axis_flipper':  4,
                'scale_lin':     0.5,   # м/с
                'scale_ang':     1.0,   # рад/с
                'scale_flipper': 0.7,   # рад
                'deadzone':      0.05
            }]
        ),
    ])
