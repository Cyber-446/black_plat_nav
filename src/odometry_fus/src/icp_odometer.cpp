#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/point_cloud2.hpp>
#include <nav_msgs/msg/odometry.hpp>
#include <geometry_msgs/msg/pose_stamped.hpp>
#include <tf2_ros/transform_broadcaster.h>
#include <tf2/LinearMath/Quaternion.h>

#include <Eigen/Dense>
#include <Eigen/Geometry>

#include <vector>
#include <cmath>
#include <limits>
#include <memory>
#include <unordered_map>
#include <string>

struct Point2D
{
    float x, y;
    Point2D() : x(0), y(0) {}
    Point2D(float x, float y) : x(x), y(y) {}
    
    Point2D operator-(const Point2D& other) const {
        return Point2D(x - other.x, y - other.y);
    }
    
    Point2D operator+(const Point2D& other) const {
        return Point2D(x + other.x, y + other.y);
    }
    
    float dot(const Point2D& other) const {
        return x * other.x + y * other.y;
    }
    
    float norm() const {
        return std::sqrt(x*x + y*y);
    }
    
    // Преобразование в Eigen вектор
    Eigen::Vector2f toEigen() const {
        return Eigen::Vector2f(x, y);
    }
};

class ICPOdometer : public rclcpp::Node
{
public:
    ICPOdometer() : Node("icp_odometer")
    {
        // Параметры
        this->declare_parameter<std::string>("cloud_topic", "/velodyne_points");
        this->declare_parameter<std::string>("odom_topic", "/icp_odom");
        this->declare_parameter<int>("max_iterations", 20);
        this->declare_parameter<double>("max_correspondence_distance", 0.5);
        this->declare_parameter<double>("transformation_epsilon", 1e-6);
        this->declare_parameter<int>("min_points", 50);
        this->declare_parameter<double>("voxel_size", 0.1);
        this->declare_parameter<double>("z_min", -0.5);
        this->declare_parameter<double>("z_max", 0.5);

        // Получение параметров
        cloud_topic_ = this->get_parameter("cloud_topic").as_string();
        odom_topic_ = this->get_parameter("odom_topic").as_string();
        max_iterations_ = this->get_parameter("max_iterations").as_int();
        max_correspondence_distance_ = this->get_parameter("max_correspondence_distance").as_double();
        transformation_epsilon_ = this->get_parameter("transformation_epsilon").as_double();
        min_points_ = this->get_parameter("min_points").as_int();
        voxel_size_ = this->get_parameter("voxel_size").as_double();
        z_min_ = this->get_parameter("z_min").as_double();
        z_max_ = this->get_parameter("z_max").as_double();

        // Подписка и публикация
        cloud_sub_ = this->create_subscription<sensor_msgs::msg::PointCloud2>(
            cloud_topic_, rclcpp::SensorDataQoS(),
            std::bind(&ICPOdometer::cloud_callback, this, std::placeholders::_1));

        odom_pub_ = this->create_publisher<nav_msgs::msg::Odometry>(odom_topic_, 10);

        // Инициализация текущей позиции
        current_pose_.setIdentity();

        RCLCPP_INFO(this->get_logger(), "ICP Odometer initialized");
        RCLCPP_INFO(this->get_logger(), "Subscribing to: %s", cloud_topic_.c_str());
        RCLCPP_INFO(this->get_logger(), "Using z range: [%.2f, %.2f]", z_min_, z_max_);
    }

private:
    std::vector<Point2D> parse_pointcloud_2d(const sensor_msgs::msg::PointCloud2::SharedPtr msg)
    {
        std::vector<Point2D> points;
        
        if (!msg) {
            RCLCPP_WARN(this->get_logger(), "Received null pointcloud message");
            return points;
        }

        // Находим поля x, y, z
        int x_offset = -1, y_offset = -1, z_offset = -1;
        
        for (const auto& field : msg->fields) {
            if (field.name == "x" && field.datatype == sensor_msgs::msg::PointField::FLOAT32) {
                x_offset = field.offset;
            } else if (field.name == "y" && field.datatype == sensor_msgs::msg::PointField::FLOAT32) {
                y_offset = field.offset;
            } else if (field.name == "z" && field.datatype == sensor_msgs::msg::PointField::FLOAT32) {
                z_offset = field.offset;
            }
        }

        if (x_offset == -1 || y_offset == -1 || z_offset == -1) {
            RCLCPP_WARN(this->get_logger(), "PointCloud2 doesn't have required x,y,z fields");
            return points;
        }

        // Парсим точки и фильтруем по высоте
        const uint8_t* data = msg->data.data();
        size_t point_step = msg->point_step;
        size_t num_points = msg->width * msg->height;

        points.reserve(num_points);

        for (size_t i = 0; i < num_points; ++i) {
            const uint8_t* point_data = data + i * point_step;
            
            float x = *reinterpret_cast<const float*>(point_data + x_offset);
            float y = *reinterpret_cast<const float*>(point_data + y_offset);
            float z = *reinterpret_cast<const float*>(point_data + z_offset);
            
            // Фильтруем по высоте и проверяем на валидные значения
            if (std::isfinite(x) && std::isfinite(y) && std::isfinite(z) &&
                z >= z_min_ && z <= z_max_) {
                points.emplace_back(x, y);
            }
        }

        RCLCPP_DEBUG(this->get_logger(), "Parsed %zu 2D points from PointCloud2", points.size());
        return points;
    }
    
    std::vector<Point2D> voxel_filter_2d(const std::vector<Point2D>& points, double voxel_size)
    {
        if (voxel_size <= 0.0 || points.empty()) {
            return points;
        }
        
        std::vector<Point2D> filtered_points;
        std::unordered_map<std::string, Point2D> voxel_map;
        
        for (const auto& pt : points) {
            // Квантуем координаты
            int voxel_x = std::round(pt.x / voxel_size);
            int voxel_y = std::round(pt.y / voxel_size);
            
            std::string voxel_key = std::to_string(voxel_x) + "_" + std::to_string(voxel_y);
            
            // Сохраняем только первую точку в вокселе
            if (voxel_map.find(voxel_key) == voxel_map.end()) {
                voxel_map[voxel_key] = pt;
                filtered_points.push_back(pt);
            }
        }
        
        RCLCPP_DEBUG(this->get_logger(), "Voxel filtering: %zu -> %zu points", 
                    points.size(), filtered_points.size());
        return filtered_points;
    }
    
    std::vector<std::pair<size_t, size_t>> find_correspondences(
        const std::vector<Point2D>& source, 
        const std::vector<Point2D>& target)
    {
        std::vector<std::pair<size_t, size_t>> correspondences;
        
        if (source.empty() || target.empty()) {
            return correspondences;
        }
        
        for (size_t i = 0; i < source.size(); ++i) {
            float min_dist = std::numeric_limits<float>::max();
            size_t min_idx = 0;
            
            for (size_t j = 0; j < target.size(); ++j) {
                float dist = (source[i] - target[j]).norm();
                if (dist < min_dist) {
                    min_dist = dist;
                    min_idx = j;
                }
            }
            
            if (min_dist < max_correspondence_distance_) {
                correspondences.emplace_back(i, min_idx);
            }
        }
        
        return correspondences;
    }
    
    Eigen::Matrix3d compute_transformation_2d(
        const std::vector<Point2D>& source,
        const std::vector<Point2D>& target,
        const std::vector<std::pair<size_t, size_t>>& correspondences)
    {
        if (correspondences.size() < 3) {
            RCLCPP_DEBUG(this->get_logger(), "Not enough correspondences: %zu", correspondences.size());
            return Eigen::Matrix3d::Identity();
        }
        
        // Вычисляем центроиды
        Point2D src_centroid(0, 0);
        Point2D tgt_centroid(0, 0);
        
        for (const auto& corr : correspondences) {
            src_centroid = src_centroid + source[corr.first];
            tgt_centroid = tgt_centroid + target[corr.second];
        }
        
        src_centroid.x /= correspondences.size();
        src_centroid.y /= correspondences.size();
        tgt_centroid.x /= correspondences.size();
        tgt_centroid.y /= correspondences.size();
        
        // Вычисляем матрицу ковариации
        Eigen::Matrix2d H = Eigen::Matrix2d::Zero();
        
        for (const auto& corr : correspondences) {
            Point2D src_pt = source[corr.first] - src_centroid;
            Point2D tgt_pt = target[corr.second] - tgt_centroid;
            
            H(0, 0) += src_pt.x * tgt_pt.x;
            H(0, 1) += src_pt.x * tgt_pt.y;
            H(1, 0) += src_pt.y * tgt_pt.x;
            H(1, 1) += src_pt.y * tgt_pt.y;
        }
        
        // SVD разложение
        Eigen::JacobiSVD<Eigen::Matrix2d> svd(H, Eigen::ComputeFullU | Eigen::ComputeFullV);
        Eigen::Matrix2d U = svd.matrixU();
        Eigen::Matrix2d V = svd.matrixV();
        
        // Вычисляем матрицу вращения
        Eigen::Matrix2d R = V * U.transpose();
        
        // Проверяем определитель на отражение
        if (R.determinant() < 0) {
            V.col(1) *= -1;
            R = V * U.transpose();
        }
        
        // Вычисляем трансляцию
        Eigen::Vector2d t = tgt_centroid.toEigen().cast<double>() - R * src_centroid.toEigen().cast<double>();
        
        // Создаем матрицу преобразования 3x3 (2D)
        Eigen::Matrix3d transformation = Eigen::Matrix3d::Identity();
        transformation.block<2, 2>(0, 0) = R;
        transformation(0, 2) = t(0);
        transformation(1, 2) = t(1);
        
        return transformation;
    }
    
    Eigen::Matrix3d perform_icp_2d(
        const std::vector<Point2D>& source,
        const std::vector<Point2D>& target,
        double& fitness_score)
    {
        if (source.size() < min_points_ || target.size() < min_points_) {
            RCLCPP_DEBUG(this->get_logger(), "Not enough points for ICP: source=%zu, target=%zu", 
                        source.size(), target.size());
            fitness_score = std::numeric_limits<double>::max();
            return Eigen::Matrix3d::Identity();
        }
        
        Eigen::Matrix3d final_transformation = Eigen::Matrix3d::Identity();
        std::vector<Point2D> current_source = source;
        double prev_error = std::numeric_limits<double>::max();
        
        for (int iter = 0; iter < max_iterations_; ++iter) {
            // Находим соответствия
            auto correspondences = find_correspondences(current_source, target);
            
            if (correspondences.size() < 3) {
                RCLCPP_DEBUG(this->get_logger(), "Not enough correspondences at iteration %d: %zu", 
                            iter, correspondences.size());
                break;
            }
            
            // Вычисляем преобразование
            Eigen::Matrix3d transformation = compute_transformation_2d(current_source, target, correspondences);
            
            // Вычисляем ошибку
            double error = 0.0;
            for (const auto& corr : correspondences) {
                error += (current_source[corr.first] - target[corr.second]).norm();
            }
            error /= correspondences.size();
            
            // Применяем преобразование к точкам
            for (auto& pt : current_source) {
                Eigen::Vector3d pt_vec(pt.x, pt.y, 1.0);
                Eigen::Vector3d transformed_pt = transformation * pt_vec;
                pt.x = transformed_pt(0);
                pt.y = transformed_pt(1);
            }
            
            // Накопление преобразования (правильный порядок!)
            final_transformation = transformation * final_transformation;
            
            // Проверка сходимости
            if (std::abs(prev_error - error) < transformation_epsilon_) {
                RCLCPP_DEBUG(this->get_logger(), "ICP converged at iteration %d", iter);
                break;
            }
            
            prev_error = error;
        }
        
        // Вычисляем итоговую ошибку
        fitness_score = 0.0;
        auto final_correspondences = find_correspondences(current_source, target);
        if (!final_correspondences.empty()) {
            for (const auto& corr : final_correspondences) {
                fitness_score += (current_source[corr.first] - target[corr.second]).norm();
            }
            fitness_score /= final_correspondences.size();
        }
        
        return final_transformation;
    }

    void cloud_callback(const sensor_msgs::msg::PointCloud2::SharedPtr msg)
    {
        try
        {
            RCLCPP_DEBUG(this->get_logger(), "Received point cloud with %dx%d points", 
                        msg->width, msg->height);
            
            // Парсим облако точек в 2D (проекция на плоскость XY)
            auto current_points = parse_pointcloud_2d(msg);
            
            if (current_points.size() < min_points_) {
                RCLCPP_DEBUG(this->get_logger(), "Not enough points: %zu", current_points.size());
                return;
            }
            
            // Применяем воксельную фильтрацию
            auto filtered_current = voxel_filter_2d(current_points, voxel_size_);
            
            RCLCPP_DEBUG(this->get_logger(), "Points after filtering: %zu", filtered_current.size());
            
            if (previous_points_.empty()) {
                previous_points_ = filtered_current;
                RCLCPP_DEBUG(this->get_logger(), "First cloud received, storing as reference");
                return;
            }
            
            // Выполняем ICP в 2D
            double fitness_score;
            Eigen::Matrix3d transformation = perform_icp_2d(filtered_current, previous_points_, fitness_score);
            
            // Извлекаем смещение и поворот
            double dx = transformation(0, 2);
            double dy = transformation(1, 2);
            double dtheta = std::atan2(transformation(1, 0), transformation(0, 0));
            
            RCLCPP_INFO(this->get_logger(), "ICP result: dx=%.3f, dy=%.3f, dtheta=%.3f, fitness=%.6f",
                       dx, dy, dtheta, fitness_score);
            
            // Правильное обновление позиции (без инвертирования!)
            update_pose(dx, dy, dtheta);
            
            // Публикуем одометрию
            publish_odometry(msg->header.stamp, fitness_score);
            
            // Обновляем предыдущие точки
            previous_points_ = filtered_current;
        }
        catch (const std::exception& e)
        {
            RCLCPP_ERROR(this->get_logger(), "Exception in cloud_callback: %s", e.what());
        }
        catch (...)
        {
            RCLCPP_ERROR(this->get_logger(), "Unknown exception in cloud_callback");
        }
    }

    void update_pose(double dx, double dy, double dtheta)
    {
        // Создаем матрицу преобразования для текущего шага
        Eigen::Matrix4d step_transform = Eigen::Matrix4d::Identity();
        step_transform(0, 3) = dx;
        step_transform(1, 3) = dy;
        
        // Матрица вращения вокруг оси Z
        step_transform(0, 0) = std::cos(dtheta);
        step_transform(0, 1) = -std::sin(dtheta);
        step_transform(1, 0) = std::sin(dtheta);
        step_transform(1, 1) = std::cos(dtheta);
        
        // Правильное обновление: current_pose = current_pose * step_transform
        current_pose_ = current_pose_ * step_transform;
    }

    void publish_odometry(const builtin_interfaces::msg::Time& stamp, double fitness_score)
    {
        try
        {
            nav_msgs::msg::Odometry odom_msg;
            odom_msg.header.stamp = stamp;
            odom_msg.header.frame_id = "odom";
            odom_msg.child_frame_id = "base_link";

            // Позиция
            odom_msg.pose.pose.position.x = current_pose_(0, 3);
            odom_msg.pose.pose.position.y = current_pose_(1, 3);
            odom_msg.pose.pose.position.z = current_pose_(2, 3);

            // Ориентация (из матрицы вращения в кватернион)
            Eigen::Matrix3d rotation_matrix = current_pose_.block<3, 3>(0, 0);
            Eigen::Quaterniond quat(rotation_matrix);
            
            odom_msg.pose.pose.orientation.x = quat.x();
            odom_msg.pose.pose.orientation.y = quat.y();
            odom_msg.pose.pose.orientation.z = quat.z();
            odom_msg.pose.pose.orientation.w = quat.w();

            // Линейная скорость (приблизительно)
            odom_msg.twist.twist.linear.x = current_pose_(0, 3) - prev_x_;
            odom_msg.twist.twist.linear.y = current_pose_(1, 3) - prev_y_;
            
            // Угловая скорость (из изменения ориентации)
            Eigen::Vector3d euler = rotation_matrix.eulerAngles(0, 1, 2);
            odom_msg.twist.twist.angular.z = euler[2] - prev_yaw_;
            
            prev_x_ = current_pose_(0, 3);
            prev_y_ = current_pose_(1, 3);
            prev_yaw_ = euler[2];

            odom_pub_->publish(odom_msg);
            
            RCLCPP_INFO(this->get_logger(), "Position: x=%.3f, y=%.3f, yaw=%.3f",
                       current_pose_(0, 3), current_pose_(1, 3), euler[2]);
        }
        catch (const std::exception& e)
        {
            RCLCPP_ERROR(this->get_logger(), "Exception in publish_odometry: %s", e.what());
        }
    }

    // Переменные
    rclcpp::Subscription<sensor_msgs::msg::PointCloud2>::SharedPtr cloud_sub_;
    rclcpp::Publisher<nav_msgs::msg::Odometry>::SharedPtr odom_pub_;

    std::vector<Point2D> previous_points_;
    Eigen::Matrix4d current_pose_;
    double prev_x_ = 0.0;
    double prev_y_ = 0.0;
    double prev_yaw_ = 0.0;

    // Параметры
    std::string cloud_topic_;
    std::string odom_topic_;
    int max_iterations_;
    double max_correspondence_distance_;
    double transformation_epsilon_;
    int min_points_;
    double voxel_size_;
    double z_min_;
    double z_max_;
};

int main(int argc, char** argv)
{
    rclcpp::init(argc, argv);
    try {
        auto node = std::make_shared<ICPOdometer>();
        rclcpp::spin(node);
    } catch (const std::exception& e) {
        std::cerr << "Exception in main: " << e.what() << std::endl;
    }
    rclcpp::shutdown();
    return 0;
}