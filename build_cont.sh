#!/bin/bash

xhost +local:docker

docker run -it -e DISPLAY=$DISPLAY \
    --privileged \
    -v /tmp/.X11-unix:/tmp/.X11-unix \
    -e ROS_DOMAIN_ID=15 \
    --net host \
    --shm-size=6G \
    --volume $(pwd):/black_plat \
    -v /dev:/dev \
    -v $HOME/.gazebo/models:/root/.gazebo/models \
    --user root \
    --name black_plat black_plat \
    -c ". /opt/ros/humble/setup.bash; cd /black_plat; colcon build; . install/setup.bash; exec /bin/bash"

