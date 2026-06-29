TurtleBot3 Custom Navigation & Teleoperation Stack

A comprehensive, containerized ROS 2 Jazzy workspace for the TurtleBot3 (Burger). This project features a custom C++ autonomous navigation stack and a real-time network bridge that allows the robot to be teleoperated directly from a custom Android application via UDP/TCP.

🏗️ Architecture & Environment Setup

1. Docker Virtualization (Native Engine)

To bypass GUI and hardware isolation limitations, this project uses a native Linux Docker daemon on an Arch Linux host.

Display Routing: X11/XWayland display socket is passed directly into the Ubuntu 24.04 container.

Network Bridging: Host networking (--net=host) is utilized so the Android phone can send UDP/TCP packets directly to the ROS 2 container over the local Wi-Fi.

2. Custom Dockerfile Environment

The environment is built from osrf/ros:jazzy-desktop. The setup automates:

Installing ros-jazzy-turtlebot3 and ros-jazzy-ros-gz dependencies.

Cloning the official Robotis TurtleBot3 Jazzy simulation workspace.

Compiling the entire workspace via colcon build.

🚀 Custom ROS 2 Packages (my_nav_nodes)

This workspace includes a custom ament_cmake C++ package containing the following nodes:

Node 1: wall_follower (Autonomous Driving)

An event-driven C++ node that utilizes raw LiDAR data to navigate a maze dynamically.

Subscriber: Listens to /scan (sensor_msgs/msg/LaserScan).

Publisher: Broadcasts to /cmd_vel (geometry_msgs/msg/TwistStamped).

Logic:

Triggers an emergency hard-left pivot ($0.6$ rad/s) if a frontal obstacle is detected within $0.5$ meters.

Maintains a cruise speed of $0.15$ m/s while smoothly adjusting yaw to maintain a safe gap from the right-hand wall.

Node 2: motus_teleop (Mobile App Bridge)

A high-frequency, multithreaded networking node that acts as the translator between a custom Android app and Gazebo Harmonic.

Network Listener: Runs detached POSIX socket threads to intercept incoming UDP or TCP packets on port 8080.

Dynamic Parameters: Network protocol, port, and maximum speed limits can be adjusted dynamically at runtime via ROS 2 parameters.

Command Translation: Parses incoming JSON arrays ([direction_id, speed_fraction]) and scales them against the robot's physical limits.

Gazebo Integration: Wraps raw standard Twist kinematics into heavily-typed TwistStamped envelopes with system clock timestamps required by the ros_gz_bridge.

💻 How to Run the Project

1. Launch the Container

On the Linux host machine, authenticate X11 and start the container:

xhost +local:docker
docker run -it --name turtlebot_sim_native --net=host --ipc=host --privileged -e DISPLAY=$DISPLAY -v /tmp/.X11-unix:/tmp/.X11-unix:rw turtlebot_jazzy_env:latest


2. Build the Workspace

Inside the container, ensure the workspace is fully compiled and sourced:

cd ~/turtlebot3_ws
colcon build --symlink-install
source install/setup.bash


3. Start the Simulation

Spin up the Gazebo 3D physics world in your first terminal:

ros2 launch turtlebot3_gazebo turtlebot3_world.launch.py


4. Execute a Navigation Node

Open a second terminal pane, source the workspace (source install/setup.bash), and choose your driving mode:

Option A: Autonomous Wall Following

ros2 run my_nav_nodes wall_follower


Option B: Mobile App Teleoperation (Default: UDP)

ros2 run my_nav_nodes motus_teleop


(Optional) Run teleop with TCP and custom speed limits:

ros2 run my_nav_nodes motus_teleop --ros-args -p protocol:=tcp -p max_linear_speed:=0.22 -p max_angular_speed:=2.84


5. Connect the Android App

Ensure the Android device and the host computer are on the same Wi-Fi network.

Enter the Host's local IP address (e.g., 192.168.1.x) and port 8080 into the Motus Android app.

Use the mobile joystick to drive the virtual TurtleBot3!
