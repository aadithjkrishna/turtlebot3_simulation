#include <chrono>
#include <memory>
#include <cmath>
#include "rclcpp/rclcpp.hpp"
#include "geometry_msgs/msg/twist.hpp"
#include "sensor_msgs/msg/laser_scan.hpp"

class WallFollower : public rclcpp::Node
{
public:
  WallFollower() : Node("wall_follower_node")
  {
    publisher_ = this->create_publisher<geometry_msgs::msg::Twist>("/cmd_vel", 10);
    subscription_ = this->create_subscription<sensor_msgs::msg::LaserScan>(
      "/scan", 10, std::bind(&WallFollower::scan_callback, this, std::placeholders::_1));
  }

private:
  void scan_callback(const sensor_msgs::msg::LaserScan::SharedPtr msg)
  {
    auto twist = geometry_msgs::msg::Twist();
    float front_distance = msg->ranges[0];
    float right_distance = msg->ranges[270];

    if (std::isinf(front_distance)) front_distance = msg->range_max;
    if (std::isinf(right_distance)) right_distance = msg->range_max;

    RCLCPP_INFO(this->get_logger(), "Front: %.2f m | Right: %.2f m", front_distance, right_distance);

    if (front_distance < 0.5) {
      twist.linear.x = 0.0;
      twist.angular.z = 0.6; // Hard left away from obstacle
    } else {
      twist.linear.x = 0.15; // Smooth cruise
      if (right_distance > 0.45) twist.angular.z = -0.15; // Shift right closer to wall
      else if (right_distance < 0.35) twist.angular.z = 0.15; // Adjust left away from wall
      else twist.angular.z = 0.0;
    }
    publisher_->publish(twist);
  }
  rclcpp::Publisher<geometry_msgs::msg::Twist>::SharedPtr publisher_;
  rclcpp::Subscription<sensor_msgs::msg::LaserScan>::SharedPtr subscription_;
};

int main(int argc, char * argv[])
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<WallFollower>());
  rclcpp::shutdown();
  return 0;
}