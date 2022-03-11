#include "mppic/critic_scorer.hpp"

namespace mppi
{


void CriticScorer::on_configure(
  rclcpp_lifecycle::LifecycleNode::WeakPtr parent, const std::string & name,
  std::shared_ptr<nav2_costmap_2d::Costmap2DROS> costmap_ros)
{
  parent_ = parent;
  costmap_ros_ = costmap_ros;
  name_ = name;

  getParams();
  loadCritics();
}

std::string CriticScorer::getFullName(const std::string & name)
{

  return "mppi::critics::" + name;
}

void CriticScorer::getParams()
{
  auto node = parent_.lock();
  logger_ = node->get_logger();
  
  auto getParam = utils::getParamGetter(node, name_);
  getParam(critics_names_, "critics_names", std::vector<std::string>{});
}

void CriticScorer::loadCritics()
{
  loader_ = std::make_unique<pluginlib::ClassLoader<critics::CriticFunction>>(
    "mppic", getFullName(base_name_));

  critics_.clear();
  for (auto name : critics_names_) {
    std::string fullname = getFullName(name);
    auto instance = std::unique_ptr<critics::CriticFunction>(
      loader_->createUnmanagedInstance(fullname));
    critics_.push_back(std::move(instance));
    critics_.back()->on_configure(parent_, name_ + "." + name, costmap_ros_);
    RCLCPP_INFO(logger_, "Critic loaded : %s", fullname.c_str());
  }
}

xt::xtensor<double, 1> CriticScorer::evalTrajectoriesScores(
  const xt::xtensor<double, 3> & trajectories, const nav_msgs::msg::Path & global_plan,
  const geometry_msgs::msg::PoseStamped & robot_pose) const
{
  // Create evalated costs tensor 
  size_t trajectories_count = trajectories.shape()[0];
  xt::xtensor<double, 1> costs = xt::zeros<double>({trajectories_count});

  if (global_plan.poses.empty()) {
    return costs;
  }

  // Transform path into tensor for evaluation
  xt::xtensor<double, 2> path = utils::toTensor<double>(global_plan);

  // Evaluate each trajectory by the critics
  for (size_t q = 0; q < critics_.size(); q++) {
    critics_[q]->score(robot_pose, trajectories, path, costs);
  }

  return costs;
}

} // namespace mppi
