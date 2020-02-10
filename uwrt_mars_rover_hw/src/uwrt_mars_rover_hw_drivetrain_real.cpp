#include "uwrt_mars_rover_hw/uwrt_mars_rover_hw_drivetrain_real.h"

#include "CanopenInterface.hpp"

namespace uwrt_mars_rover_hw {

bool UWRTRoverHWDrivetrainReal::init(ros::NodeHandle &root_nh, ros::NodeHandle &robot_hw_nh) {
  if (!UWRTRoverHWDrivetrain::init(root_nh, robot_hw_nh)) {
    return false;
  }

  if (!loadRoboteqIndicesFromParameterServer(robot_hw_nh)) {
    return false;
  }

  // TODO: controller to switch the roboteq communication mode
  std::unique_ptr<roboteq::CommunicationInterface> comm = std::make_unique<roboteq::CanopenInterface>(0x01, "can0");
  motor_controller_ = std::make_unique<roboteq::RoboteqController>(std::move(comm));
  return true;
}

void UWRTRoverHWDrivetrainReal::read(const ros::Time & /*time*/, const ros::Duration & /*period*/) {
  for (const auto &joint_name : joint_names_) {
    // TODO: change to use tpdos
    actuator_joint_states_[joint_name].actuator_position =
        motor_controller_->readAbsoluteEncoderCount(roboteq_actuator_index_[joint_name]);
    actuator_joint_states_[joint_name].actuator_velocity =
        motor_controller_->readEncoderMotorSpeed(roboteq_actuator_index_[joint_name]);
    actuator_joint_states_[joint_name].actuator_effort =
        motor_controller_->readMotorAmps(roboteq_actuator_index_[joint_name]);
  }
  actuator_to_joint_state_interface_.propagate();
}

void UWRTRoverHWDrivetrainReal::write(const ros::Time & /*time*/, const ros::Duration & /*period*/) {
  for (const auto &joint_name : joint_names_) {
    bool successful_joint_write = false;
    switch (actuator_joint_commands_[joint_name].type) {
      case UWRTRoverHWDrivetrain::DrivetrainActuatorJointCommand::Type::POSITION:
        joint_to_actuator_position_interface_.propagate();
        successful_joint_write =
            motor_controller_->setPosition(static_cast<int32_t>(actuator_joint_commands_[joint_name].actuator_data),
                                           roboteq_actuator_index_[joint_name]);
        break;

      case UWRTRoverHWDrivetrain::DrivetrainActuatorJointCommand::Type::VELOCITY:
        joint_to_actuator_velocity_interface_.propagate();
        successful_joint_write =
            motor_controller_->setVelocity(static_cast<int32_t>(actuator_joint_commands_[joint_name].actuator_data),
                                           roboteq_actuator_index_[joint_name]);
        break;

      case UWRTRoverHWDrivetrain::DrivetrainActuatorJointCommand::Type::EFFORT:
        joint_to_actuator_effort_interface_.propagate();
        // TODO: look into roboteq torque control
        break;

      case UWRTRoverHWDrivetrain::DrivetrainActuatorJointCommand::Type::NONE:
        ROS_WARN_STREAM_NAMED(name_, joint_name << " has a " << actuator_joint_commands_[joint_name].type
                                                << " command type_. Sending Stop Command to Roboteq Controller!");
        successful_joint_write = motor_controller_->stopInAllModes(roboteq_actuator_index_[joint_name]);
        break;

      default:
        ROS_ERROR_STREAM_NAMED(
            name_, joint_name << " has a joint command with index "
                              << static_cast<int>(actuator_joint_commands_[joint_name].type)
                              << ",which is an unknown command type. Sending Stop Command to Roboteq Controller!");
        successful_joint_write = motor_controller_->stopInAllModes(roboteq_actuator_index_[joint_name]);
    }
    if (!successful_joint_write) {
      ROS_ERROR_STREAM_NAMED(name_, "Failed to write " << actuator_joint_commands_[joint_name].type << " command to "
                                                       << joint_name << ".");
    }
  }
}

bool UWRTRoverHWDrivetrainReal::loadRoboteqIndicesFromParameterServer(ros::NodeHandle &robot_hw_nh) {
  XmlRpc::XmlRpcValue joints_list;
  bool param_fetched = robot_hw_nh.getParam("joints", joints_list);
  if (!param_fetched) {
    ROS_WARN_STREAM_NAMED(name_, robot_hw_nh.getNamespace() << "/joints could not be loaded from parameter server.");
    return false;
  }
  ROS_DEBUG_STREAM_NAMED(name_, robot_hw_nh.getNamespace() << "/joints loaded from parameter server.");
  ROS_ASSERT(joints_list.getType() == XmlRpc::XmlRpcValue::TypeArray);

  // NOLINTNEXTLINE(modernize-loop-convert): iterator only valid for XmlRpcValue::TypeStruct
  for (size_t joint_index = 0; joint_index < joints_list.size(); joint_index++) {
    ROS_ASSERT(joints_list[joint_index].getType() == XmlRpc::XmlRpcValue::TypeStruct);
    ROS_ASSERT(joints_list[joint_index].hasMember("name"));
    ROS_ASSERT(joints_list[joint_index]["name"].getType() == XmlRpc::XmlRpcValue::TypeString);
    std::string joint_name = joints_list[joint_index]["name"];

    ROS_ASSERT(joints_list[joint_index].hasMember("roboteq_index"));
    ROS_ASSERT(joints_list[joint_index]["roboteq_index"].getType() == XmlRpc::XmlRpcValue::TypeInt);
    roboteq_actuator_index_[joint_name] = static_cast<int>(joints_list[joint_index]["roboteq_index"]);
  }
  return true;
}
}  // namespace uwrt_mars_rover_hw
