# Use ROS 1 Melodic as the base image
FROM ros:melodic-ros-base

# Replace /bin/sh with /bin/bash for compatibility
RUN rm /bin/sh && ln -s /bin/bash /bin/sh

# Update and install basic packages
RUN apt-get update -y && apt-get install -y \
    build-essential \
    apt-utils \
    nano \
    python-pip \
    curl \
    git \
    wget \
    gpg \
    gdb \
    libboost-all-dev \
    ca-certificates \
    python-wstool \
    python-catkin-tools \
    libyaml-cpp-dev \
    libpcap-dev \
    libgoogle-glog-dev \
    libglew-dev \
    libopencv-dev \
    libdw-dev \
    rsync \
    software-properties-common \
    && rm -rf /var/lib/apt/lists/*

# # Install GTSAM 4.0.3
# RUN add-apt-repository ppa:borglab/gtsam-develop -y
# RUN apt-get update -y && apt-get install -y \
#     libgtsam-dev \
#     && rm -rf /var/lib/apt/lists/*

# Install ROS Melodic packages
RUN apt-get update -y && apt-get install -y \
    ros-melodic-nav-msgs \
    ros-melodic-velodyne-msgs \
    ros-melodic-pcl-ros \
    ros-melodic-eigen-conversions \
    ros-melodic-tf-conversions \
    ros-melodic-angles \
    ros-melodic-diagnostic-updater \
    ros-melodic-cv-bridge \
    ros-melodic-rviz \
    ros-melodic-desktop-full \
    ros-melodic-tf2-geometry-msgs \
    ros-melodic-imu-tools \
    && rm -rf /var/lib/apt/lists/*

# Create data directory for mounting local files
RUN mkdir /root/data
RUN mkdir /root/catkin_ws/

# Create and initialize catkin workspace
WORKDIR /root/catkin_ws/src
RUN /bin/bash -c "source /opt/ros/melodic/setup.bash && catkin_init_workspace"

# Clone required repositories
# RUN git clone https://github.com/ori-drs/allan_variance_ros.git
RUN git clone https://github.com/chennuo0125-HIT/lidar_imu_calib.git

# Clone code_utils with retry and error handling
# RUN git clone https://github.com/hku-mars/code_utils.git || \
#     (echo "Failed to clone code_utils, trying alternative..." && \
#      git clone https://gitee.com/hku-mars/code_utils.git || \
#      echo "Warning: code_utils not available, some features might be limited")
# Issue with flann and lz4
RUN mv /usr/include/flann/ext/lz4.h /usr/include/flann/ext/lz4.h.bak
RUN mv /usr/include/flann/ext/lz4hc.h /usr/include/flann/ext/lz4.h.bak
RUN ln -s /usr/include/lz4.h /usr/include/flann/ext/lz4.h
RUN ln -s /usr/include/lz4hc.h /usr/include/flann/ext/lz4hc.h
# Build the catkin workspace
WORKDIR /root/catkin_ws
RUN /bin/bash -c "source /opt/ros/melodic/setup.bash && catkin_make"

# Set up environment variables
RUN echo "source /opt/ros/melodic/setup.bash" >> ~/.bashrc
RUN echo "source /root/catkin_ws/devel/setup.bash" >> ~/.bashrc
# RUN echo "alias sors='source /root/catkin_ws/devel/setup.bash'" >> ~/.bashrc

# Set working directory
WORKDIR /root

# Default command
CMD ["/bin/bash"]