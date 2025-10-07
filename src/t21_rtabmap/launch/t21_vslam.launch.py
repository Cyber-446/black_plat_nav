import os
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, IncludeLaunchDescription, OpaqueFunction
from launch.substitutions import LaunchConfiguration, PathJoinSubstitution
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.conditions import IfCondition, UnlessCondition
from launch_ros.substitutions import FindPackageShare
from launch_ros.actions import Node
from ament_index_python.packages import get_package_share_directory

def launch_setup(context, *args, **kwargs):
    
    localization = LaunchConfiguration('localization')

    rtabmap_package='t21_rtabmap'
    
    use_sim_time = LaunchConfiguration("use_sim_time")
    
    # With the simulator, the imu is not published fast enough 
    # and have a huge delay, disabling imu usage from VO
    use_imu = use_sim_time.perform(context) in ["false", "False"]

    vslam_params = os.path.join(
        get_package_share_directory(rtabmap_package),
        'config',
        'rtabmap.yaml'
    )

    icp_params = {
    'Reg/Strategy': '1',
    'Reg/Force3DoF': 'true',
    'Mem/NotLinkedNodesKept': 'false',
    'Icp/VoxelSize': '0.1',
    'Icp/MaxCorrespondenceDistance': '1',
    'Icp/PointToPlaneGroundNormalsUp': '0.9',
    'Icp/RangeMin': '0.7',
    'Icp/MaxTranslation': '1',
    # Add synchronization parameters:
    'approx_sync': False,
    'approx_sync_max_interval': 0.2,
    'queue_size': 10,
    'use_sim_time': LaunchConfiguration('use_sim_time')
    }
    vslam_remappings=[('imu', 'imu'),
                      ('odom', 'odom'),
                      ('scan_cloud', '/velodyne_points'),]
  
    rgbd_remappings = [
        ('rgb/image', '/camera/image_raw'),
        ('rgb/camera_info', '/camera/camera_info'),
        ('depth/image', '/camera/depth/image_raw'),
    ]
    
    lidar_remappings=[('scan_cloud', '/velodyne_points'),]
    
    return [        
        # compute imu orientation
        # Node(
        #     package='imu_filter_madgwick', executable='imu_filter_madgwick_node', output='screen',
        #     parameters=[{
        #       'use_mag':False,
        #       'world_frame':'map',
        #       'publish_tf':False}],
        #     remappings=[
        #         ('imu/data_raw', 'imu/data'),
        #         ('imu/data', 'imu')]
        #     ),
        
        # VSLAM nodes:
        Node(
            package='rtabmap_sync', executable='rgbd_sync', 
            parameters=[{
                'approx_sync': False,
                'topic_queue_size': 10,
                'qos': 1,  # Reliable QoS
                'qos_image': 1,
                'qos_info': 1,
                'depth_scale': 1.0,
                'use_sim_time': LaunchConfiguration('use_sim_time')
            }],
            remappings=rgbd_remappings
        ),

        Node(
            package='rtabmap_odom', executable='rgbd_odometry', output='screen',
            parameters=[vslam_params, {
                'odom_frame_id': 'odom',
                'subscribe_rgbd': True,
                'rgbd_cameras': 1,
                'frame_id': 'base_link',
                'publish_tf_odom': True,
                'wait_for_transform': 0.1,
                'use_sim_time': LaunchConfiguration('use_sim_time')
            }],
            remappings=vslam_remappings,
            arguments=["--ros-args", "--log-level", 'info']),

        Node(
            package='rtabmap_odom', executable='icp_odometry', output='screen',
            parameters=[vslam_params, icp_params],
            remappings=[('/scan_cloud',   '/velodyne_points')],
            arguments=["--ros-args", "--log-level", 'info']
        ),

        # SLAM Mode:
        Node(
            condition=UnlessCondition(localization),
            package='rtabmap_slam', executable='rtabmap', output='screen',
            parameters=[vslam_params,use_sim_time],
            remappings=vslam_remappings,
            arguments=['-d']), # This will delete the previous database (~/.ros/rtabmap.db)
            
        # Localization mode:
        Node(
            condition=IfCondition(localization),
            package='rtabmap_slam', executable='rtabmap', output='screen',
            parameters=[vslam_params, 
              {'Mem/IncrementalMemory': 'False',
               'Mem/InitWMWithAllNodes': 'True'},
               use_sim_time],
            remappings=vslam_remappings,
        ),

        Node(
            package='rtabmap_viz', executable='rtabmap_viz', output='screen',
            condition=IfCondition(LaunchConfiguration("rtabmap_viz")),
            parameters=[vslam_params,use_sim_time],
            remappings=vslam_remappings,
            arguments=["--ros-args", "--log-level", 'info']
        ),

        # Debug
        Node(
            package='rqt_topic', executable='rqt_topic', name='topic_monitor',
            condition=IfCondition(LaunchConfiguration("debug"))
        ),
        Node(
            package='rqt_graph', executable='rqt_graph', name='graph_monitor',
            condition=IfCondition(LaunchConfiguration("debug"))
        ),
    #     # Compute ground/obstacle clouds for nav2 voxel layers
    #     Node(
    #         package='rtabmap_util', executable='point_cloud_xyz', output='screen',
    #         parameters=[{'decimation': 2,
    #                      'max_depth': 3.0,
    #                      'voxel_size': 0.02}],
    #            remappings=[('depth/image', '/camera/depth/image_rect_raw'),
    #             ('depth/camera_info', '/camera/depth/camera_info'),
    #             ('cloud', '/camera/depth/points')]
    #     ),
        
    #     Node(
    #         package='rtabmap_util', executable='obstacles_detection', output='screen',
    #         parameters=[{
    #             'frame_id': 'base_link',
    #             'map_frame_id': 'map',
    #             'min_cluster_size': 20,
    #             'max_obstacle_height': 2.0,
    #             'wait_for_transform': 0.2
    #         }],
    #         remappings=[
    #             ('cloud', '/camera/points'),
    #             ('obstacles', '/camera/obstacles'),
    #             ('ground', '/camera/ground')
    #         ]
    #     ),
     ]        
def generate_launch_description():
    
    return LaunchDescription([
        DeclareLaunchArgument(
            name='use_sim_time', 
            default_value='false',
            description='Enable use_sime_time to true'
        ),

        DeclareLaunchArgument(
            name='rviz', 
            default_value='false',
            description='Run rviz'
        ),
        
        DeclareLaunchArgument(
            name='rtabmap_viz', 
            default_value='true',
            description='Run rtabmap_viz'
        ),

        DeclareLaunchArgument(
            'localization', default_value='false', choices=['true', 'false'],
            description='Launch rtabmap in localization mode (a map should have been already created).'),
        
        DeclareLaunchArgument(
            name="debug",
            default_value="false",
            description="Show rqt_graph and topic monitor"
        ),
        OpaqueFunction(function=launch_setup)
    ])