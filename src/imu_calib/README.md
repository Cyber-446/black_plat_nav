## Инструкция по подготовке данных и запуску калибровки

Пакет для конвертации bag-файлов:
```bash
pip3 install rosbags
```

Все манипуляции лучше проводить в папке `data/`, чтобы потом подтянуть в контейнер.

В файле `out.yaml` указываются имена топиков, которые нужно вычленить из rosbag:

```
output_bags:
  - uri: test_lio_3 # Имя bag-файла
    topics: [/imu/data, /velodyne_points]
```

```bash
ros2 bag convert -i <YOUR-ROS2-BAG-FOLDER> -o out.yaml
```

Далее конвертируем их в формат для ROS1:
```bash
rosbags-convert <YOUR-SPLITTED-ROS2-BAG-FOLDER> --dst <OUTPUT-BAG-FILE>
```

Далее нужно собрать контейнер:
```bash
sudo docker build -t li_calib .
```

Запускать контейнер следует из директории проекта:
```bash
docker run -it --rm \
  --env="DISPLAY" \
  --volume="$HOME/.Xauthority:/root/.Xauthority:rw" \
  --volume="/tmp/.X11-unix:/tmp/.X11-unix:rw" \
  --volume="$(pwd)/data:/root/data" \
  li_calib bash
```
В самом контейнере нужно отредактировать launch-файл `/root/catkin_ws/src/lidar_imu_calib/lidar_imu_calib/launch
`, указав в нём нужные топики:
```
<launch>
    <node pkg="lidar_imu_calib" type="calib_exR_lidar2imu_node" name="calib_exR_lidar2imu_node" output="screen">
        <param name="/lidar_topic" value="/velodyne_points"/>
        <param name="/imu_topic" value="/imu/data"/>
        <param name="/bag_file" value="/root/data/<BAG_DIR>/<BAG_NAME>.bag"/>
    </node>
</launch>
```
Запуск:
```
roslaunch lidar_imu_calib calib_exR_lidar2imu.launch
```
Процесс займёт какое-то время. По завершению на экране выведутся данные:
```
result euler angle(RPY) : 0 0 0
result extrinsic rotation matrix : 
1 0 0
0 1 0
0 0 1
```

[Оригинальная статья](https://autowarefoundation.github.io/autoware-documentation/main/how-to-guides/integrating-autoware/creating-vehicle-and-sensor-model/calibrating-sensors/lidar-imu-calibration/) написана для пакета OA_LICalib, но получить полезные данные не удалось.

[Репозиторий](https://github.com/chennuo0125-HIT/lidar_imu_calib?tab=readme-ov-file)