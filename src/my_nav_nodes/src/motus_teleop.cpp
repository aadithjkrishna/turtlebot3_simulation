#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <vector>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <iostream>

#include "rclcpp/rclcpp.hpp"
#include "geometry_msgs/msg/twist_stamped.hpp" // Corrected to TwistStamped

using namespace std::chrono_literals;

class MotusTeleopNode : public rclcpp::Node
{
public:
  MotusTeleopNode()
  : Node("motus_teleop")
  {
    this->declare_parameter("protocol", "udp");
    this->declare_parameter("port", 8080);
    this->declare_parameter("max_linear_speed", 1.0);
    this->declare_parameter("max_angular_speed", 2.84);

    protocol_ = this->get_parameter("protocol").as_string();
    port_ = this->get_parameter("port").as_int();
    max_linear_speed_ = this->get_parameter("max_linear_speed").as_double();
    max_angular_speed_ = this->get_parameter("max_angular_speed").as_double();

    // Publishing TwistStamped to match Gazebo Harmonic's requirements
    publisher_ = this->create_publisher<geometry_msgs::msg::TwistStamped>(
      "/cmd_vel", rclcpp::SystemDefaultsQoS());

    RCLCPP_INFO(this->get_logger(), "Starting Motus Teleop Node");
    RCLCPP_INFO(this->get_logger(), "Protocol: %s, Port: %d", protocol_.c_str(), port_);

    if (protocol_ == "tcp") {
      listen_thread_ = std::thread(&MotusTeleopNode::listen_tcp, this);
    } else {
      listen_thread_ = std::thread(&MotusTeleopNode::listen_udp, this);
    }
  }

  ~MotusTeleopNode()
  {
    running_ = false;
    if (server_fd_ > 0) {
      close(server_fd_);
    }
    if (listen_thread_.joinable()) {
      listen_thread_.join();
    }
  }

private:
  void listen_udp()
  {
    server_fd_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (server_fd_ == 0) {
      RCLCPP_ERROR(this->get_logger(), "UDP socket creation failed");
      return;
    }

    int opt = 1;
    setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port_);

    if (bind(server_fd_, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
      RCLCPP_ERROR(this->get_logger(), "UDP bind failed");
      return;
    }

    char buffer[1024];
    while (running_ && rclcpp::ok()) {
      struct sockaddr_in client_addr;
      socklen_t len = sizeof(client_addr);
      int n = recvfrom(server_fd_, buffer, sizeof(buffer) - 1, 0, (struct sockaddr *)&client_addr, &len);
      if (n > 0) {
        buffer[n] = '\0';
        RCLCPP_INFO(this->get_logger(), "UDP received: %s", buffer);
        process_data(std::string(buffer));
      }
    }
  }

  void listen_tcp()
  {
    server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd_ == 0) {
      RCLCPP_ERROR(this->get_logger(), "TCP socket creation failed");
      return;
    }

    int opt = 1;
    setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port_);

    if (bind(server_fd_, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
      RCLCPP_ERROR(this->get_logger(), "TCP bind failed");
      return;
    }

    if (listen(server_fd_, 3) < 0) {
      RCLCPP_ERROR(this->get_logger(), "TCP listen failed");
      return;
    }

    while (running_ && rclcpp::ok()) {
      struct sockaddr_in client_addr;
      socklen_t len = sizeof(client_addr);
      int new_socket = accept(server_fd_, (struct sockaddr *)&client_addr, &len);
      if (new_socket < 0) continue;

      RCLCPP_INFO(this->get_logger(), "TCP client connected");
      char buffer[1024];
      while (running_ && rclcpp::ok()) {
        int valread = read(new_socket, buffer, sizeof(buffer) - 1);
        if (valread <= 0) break;
        buffer[valread] = '\0';
        RCLCPP_INFO(this->get_logger(), "TCP received: %s", buffer);
        process_data(std::string(buffer));
      }
      close(new_socket);
      RCLCPP_INFO(this->get_logger(), "TCP client disconnected");
    }
  }

  void process_data(const std::string& data)
  {
    if (data.front() == '[' && data.back() == ']') {
      std::string content = data.substr(1, data.length() - 2);
      std::vector<std::string> tokens;
      size_t pos = 0;
      while ((pos = content.find(',')) != std::string::npos) {
        tokens.push_back(content.substr(0, pos));
        content.erase(0, pos + 1);
      }
      tokens.push_back(content);

      try {
        if (tokens.empty()) return;
        int direction_id = std::stoi(tokens[0]);

        auto msg = geometry_msgs::msg::TwistStamped();
        msg.header.stamp = this->get_clock()->now();
        msg.header.frame_id = "base_link";

        msg.twist.linear.x = 0.0;
        msg.twist.linear.y = 0.0;
        msg.twist.linear.z = 0.0;
        msg.twist.angular.x = 0.0;
        msg.twist.angular.y = 0.0;
        msg.twist.angular.z = 0.0;

        if (direction_id == 5 && tokens.size() >= 3) {
          // Game Mode: [5, linear_speed, angular_speed]
          double linear_speed = std::stod(tokens[1]);
          double angular_speed = std::stod(tokens[2]);
          msg.twist.linear.x = linear_speed * max_linear_speed_;
          msg.twist.angular.z = angular_speed * max_angular_speed_;
        } else if (tokens.size() >= 2) {
          double speed = std::stod(tokens[1]);
          switch (direction_id) {
            case 0: // STOP
              break;
            case 1: // FORWARD
              msg.twist.linear.x = speed * max_linear_speed_;
              break;
            case 2: // BACKWARD
              msg.twist.linear.x = -speed * max_linear_speed_;
              break;
            case 3: // LEFT
              msg.twist.angular.z = speed * max_angular_speed_;
              break;
            case 4: // RIGHT
              msg.twist.angular.z = -speed * max_angular_speed_;
              break;
          }
        } else {
          RCLCPP_WARN(this->get_logger(), "Insufficient arguments in JSON array.");
          return;
        }

        RCLCPP_INFO(this->get_logger(), "Publishing Twist: linear.x=%.2f, angular.z=%.2f", msg.twist.linear.x, msg.twist.angular.z);
        publisher_->publish(msg);
      } catch (const std::exception& e) {
        RCLCPP_WARN(this->get_logger(), "Failed to parse data (exception): %s", data.c_str());
      }
    } else {
      RCLCPP_WARN(this->get_logger(), "Data format invalid (no brackets): %s", data.c_str());
    }
  }

  std::string protocol_;
  int port_;
  double max_linear_speed_;
  double max_angular_speed_;

  std::thread listen_thread_;
  bool running_ = true;
  int server_fd_ = -1;

  rclcpp::Publisher<geometry_msgs::msg::TwistStamped>::SharedPtr publisher_;
};

int main(int argc, char * argv[])
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<MotusTeleopNode>());
  rclcpp::shutdown();
  return 0;
}