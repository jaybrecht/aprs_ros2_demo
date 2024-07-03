#include <fanuc_demo/fanuc_kitting.hpp>

FanucDemo::FanucDemo()
    : Node("fanuc_demo"),
      fanuc_arm_(std::shared_ptr<rclcpp::Node>(std::move(this)), "fanuc_arm"),
      planning_scene_()
{
  // Use upper joint velocity and acceleration limits
  fanuc_arm_.setMaxAccelerationScalingFactor(1.0);
  fanuc_arm_.setMaxVelocityScalingFactor(1.0);
  fanuc_arm_.setPlanningTime(10.0);
  fanuc_arm_.setNumPlanningAttempts(5);
  fanuc_arm_.allowReplanning(true);
  fanuc_arm_.setReplanAttempts(5);

  open_gripper_client_ = this->create_client<example_interfaces::srv::Trigger>("gripper_open");
  close_gripper_client_ = this->create_client<example_interfaces::srv::Trigger>("gripper_close");

  fanuc_position_publisher_ = this->create_publisher<std_msgs::msg::Float64MultiArray>("/forward_position_controller/commands", 10);
  
  RCLCPP_INFO(this->get_logger(), "Initialization successful.");
}

FanucDemo::~FanucDemo()
{
  fanuc_arm_.~MoveGroupInterface();
}

void FanucDemo::FanucSendHome()
{
  fanuc_arm_.setNamedTarget("home");
  auto plan = FanucPlantoTarget();
  if (plan.first)
  {
    FanucSendTrajectory(plan.second);
  }

}

std::pair<bool, moveit_msgs::msg::RobotTrajectory>  FanucDemo::FanucPlantoTarget()
{
  moveit::planning_interface::MoveGroupInterface::Plan plan;
  bool success = static_cast<bool>(fanuc_arm_.plan(plan));
  moveit_msgs::msg::RobotTrajectory trajectory = plan.trajectory;

  if (success)
  {
    return std::make_pair(true, trajectory);
  }
  else
  {
    RCLCPP_ERROR(get_logger(), "Unable to generate plan");
    return std::make_pair(false, trajectory);
  }
}

std::pair<bool,moveit_msgs::msg::RobotTrajectory> FanucDemo::FanucPlanCartesian(
    std::vector<geometry_msgs::msg::Pose> waypoints, double vsf, double asf, bool avoid_collisions)
{
  moveit_msgs::msg::RobotTrajectory trajectory;

  double path_fraction = fanuc_arm_.computeCartesianPath(waypoints, 0.01, 0.0, trajectory, avoid_collisions);

  if (path_fraction < 0.9)
  {
    RCLCPP_ERROR(get_logger(), "Unable to generate trajectory through waypoints");
    return std::make_pair(false, trajectory);
  }

  // Retime trajectory
  robot_trajectory::RobotTrajectory rt(fanuc_arm_.getCurrentState()->getRobotModel(), "floor_robot");
  rt.setRobotTrajectoryMsg(*fanuc_arm_.getCurrentState(), trajectory);
  totg_.computeTimeStamps(rt, vsf, asf);
  rt.getRobotTrajectoryMsg(trajectory);

  return std::make_pair(true, trajectory);
}

bool FanucDemo::FanucSendTrajectory(moveit_msgs::msg::RobotTrajectory trajectory)
{
  for (auto point : trajectory.joint_trajectory.points)
  {
    std_msgs::msg::Float64MultiArray joint_positions;
    joint_positions.data = point.positions;
    fanuc_position_publisher_->publish(joint_positions);
    usleep(100000);
  }

  return true;
}

bool FanucDemo::FanucOpenGripper()
{
  auto request = std::make_shared<example_interfaces::srv::Trigger::Request>();
  auto result = open_gripper_client_->async_send_request(request);
  return result.get()->success;
}

bool FanucDemo::FanucCloseGripper()
{
  auto request = std::make_shared<example_interfaces::srv::Trigger::Request>();
  auto result = close_gripper_client_->async_send_request(request);
  return result.get()->success;
}

bool FanucDemo::BuildTarget(){

  // geometry_msgs::msg::Pose msg;
  // msg.orientation.w = 1.0;
  // msg.position.x = 0.1;
  // msg.position.y = 0.1;
  // msg.position.z = 0.1;

  // fanuc_arm_.setPoseTarget(msg);
  // auto plan = FanucPlantoTarget();
  // if (plan.first)
  // {
  //   FanucSendTrajectory(plan.second);
  // }

  geometry_msgs::msg::Pose robot_pose = fanuc_arm_.getCurrentPose().pose;
  std::vector<geometry_msgs::msg::Pose> waypoints;
  robot_pose.position.x += 0.1;
  waypoints.push_back(robot_pose);

  auto plan = FanucPlanCartesian(waypoints, 0.1, 0.1, true);
  if (plan.first)
  {
    FanucSendTrajectory(plan.second);
  }
 
  return true;

}

int main(int argc, char *argv[])
{
  rclcpp::init(argc, argv);

  auto fanuc_demo = std::make_shared<FanucDemo>();

  fanuc_demo->FanucSendHome();

  rclcpp::shutdown();
}
