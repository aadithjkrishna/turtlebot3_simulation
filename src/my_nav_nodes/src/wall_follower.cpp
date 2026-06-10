#include <chrono>
#include <memory>
#include <cmath>
#include "rclcpp/rclcpp.hpp"
#include "geometry_msgs/msg/twist_stamped.hpp" // <-- Updated header
#include "sensor_msgs/msg/laser_scan.hpp"

class WallFollower : public rclcpp::Node
{
public:
  WallFollower() : Node("wall_follower_node")
  {
    // 1. Update publisher to TwistStamped
    publisher_ = this->create_publisher<geometry_msgs::msg::TwistStamped>(
      "/cmd_vel", rclcpp::SystemDefaultsQoS());
      
    subscription_ = this->create_subscription<sensor_msgs::msg::LaserScan>(
      "/scan", rclcpp::SystemDefaultsQoS(), std::bind(&WallFollower::scan_callback, this, std::placeholders::_1));
  }

private:
  void scan_callback(const sensor_msgs::msg::LaserScan::SharedPtr msg)
  {
    // 2. Create the Stamped message envelope
    auto twist_msg = geometry_msgs::msg::TwistStamped();
    
    // 3. Populate the exact timestamp and frame for the Gazebo bridge
    twist_msg.header.stamp = this->get_clock()->now();
    twist_msg.header.frame_id = "base_link"; 

    float front_distance = msg->ranges[0];
    float right_distance = msg->ranges[270];

    if (std::isinf(front_distance)) front_distance = msg->range_max;
    if (std::isinf(right_distance)) right_distance = msg->range_max;

    RCLCPP_INFO(this->get_logger(), "Front: %.2f m | Right: %.2f m", front_distance, right_distance);

    // 4. Assign velocity values to the internal 'twist' object
    if (front_distance < 0.5) {
      twist_msg.twist.linear.x = 0.0;
      twist_msg.twist.angular.z = 0.6; // Hard left away from obstacle
    } else {
      twist_msg.twist.linear.x = 0.15; // Smooth cruise
      if (right_distance > 0.45) twist_msg.twist.angular.z = -0.15; // Shift right closer to wall
      else if (right_distance < 0.35) twist_msg.twist.angular.z = 0.15; // Shift left away from wall
      else twist_msg.twist.angular.z = 0.0;
    }
    
    // Publish the stamped envelope
    publisher_->publish(twist_msg);
  }
  
  rclcpp::Publisher<geometry_msgs::msg::TwistStamped>::SharedPtr publisher_;
  rclcpp::Subscription<sensor_msgs::msg::LaserScan>::SharedPtr subscription_;
};

int main(int argc, char * argv[])
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<WallFollower>());
  rclcpp::shutdown();
  return 0;
}