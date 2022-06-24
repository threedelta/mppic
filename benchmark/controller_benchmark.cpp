// Copyright 2022 @artofnothingness Alexey Budyakov, Samsung Research

#include <benchmark/benchmark.h>
#include <string>

#include <geometry_msgs/msg/pose_stamped.hpp>
#include <geometry_msgs/msg/twist.hpp>
#include <nav_msgs/msg/path.hpp>

#include <nav2_costmap_2d/cost_values.hpp>
#include <nav2_costmap_2d/costmap_2d.hpp>
#include <nav2_costmap_2d/costmap_2d_ros.hpp>
#include <nav2_core/goal_checker.hpp>

#include <xtensor/xarray.hpp>
#include <xtensor/xio.hpp>
#include <xtensor/xview.hpp>

#include "mppic/motion_models.hpp"
#include "mppic/controller.hpp"

#include "utils.hpp"

class RosLockGuard
{
public:
  RosLockGuard() {rclcpp::init(0, nullptr);}
  ~RosLockGuard() {rclcpp::shutdown();}
};

RosLockGuard g_rclcpp;

void prepareAndRunBenchmark(
  bool consider_footprint, std::string motion_model,
  std::vector<std::string> critics, benchmark::State & state)
{

  bool visualize = false;

  int batch_size = 300;
  int time_steps = 12;
  unsigned int path_points = 50u;
  int iteration_count = 2;
  double lookahead_distance = 10.0;

  TestCostmapSettings costmap_settings{};
  auto costmap_ros = getDummyCostmapRos(costmap_settings);
  auto costmap = costmap_ros->getCostmap();

  TestPose start_pose = costmap_settings.getCenterPose();
  double path_step = costmap_settings.resolution;

  TestPathSettings path_settings{start_pose, path_points, path_step, path_step};
  TestOptimizerSettings optimizer_settings{batch_size, time_steps, iteration_count,
    lookahead_distance, motion_model, consider_footprint};

  unsigned int offset = 4;
  unsigned int obstacle_size = offset * 2;

  unsigned char obstacle_cost = 250;

  auto [obst_x, obst_y] = costmap_settings.getCenterIJ();

  obst_x = obst_x - offset;
  obst_y = obst_y - offset;
  addObstacle(costmap, {obst_x, obst_y, obstacle_size, obstacle_cost});

  printInfo(optimizer_settings, path_settings, critics);

  rclcpp::NodeOptions options;
  std::vector<rclcpp::Parameter> params;
  setUpControllerParams(visualize, params);
  setUpOptimizerParams(optimizer_settings, critics, params);
  options.parameter_overrides(params);
  auto node = getDummyNode(options);

  auto tf_buffer = std::make_shared<tf2_ros::Buffer>(node->get_clock());
  tf_buffer->setUsingDedicatedThread(true);  // One-thread broadcasting-listening model

  auto broadcaster =
    std::make_shared<tf2_ros::TransformBroadcaster>(node);
  auto tf_listener = std::make_shared<tf2_ros::TransformListener>(*tf_buffer);

  auto map_odom_broadcaster = std::async(
    std::launch::async, sendTf, "map", "odom", broadcaster, node,
    20);

  auto odom_base_link_broadcaster = std::async(
    std::launch::async, sendTf, "odom", "base_link", broadcaster, node,
    20);

  auto controller = getDummyController(node, tf_buffer, costmap_ros);

  // evalControl args
  auto pose = getDummyPointStamped(node, start_pose);
  auto velocity = getDummyTwist();
  auto path = getIncrementalDummyPath(node, path_settings);

  controller.setPlan(path);

  nav2_core::GoalChecker * dummy_goal_checker{nullptr};

  for (auto _ : state) {
    controller.computeVelocityCommands(pose, velocity, dummy_goal_checker);
  }
  map_odom_broadcaster.wait();
  odom_base_link_broadcaster.wait();
}

static void BM_DiffDrivePointFootprint(benchmark::State & state)
{
  bool consider_footprint = true;
  std::string motion_model = "DiffDrive";
  std::vector<std::string> critics = {{"GoalCritic"}, {"GoalAngleCritic"}, {"ObstaclesCritic"},
    {"PathAngleCritic"}, {"PathFollowCritic"}, {"PreferForwardCritic"}};

  prepareAndRunBenchmark(consider_footprint, motion_model, critics, state);
}

static void BM_DiffDrive(benchmark::State & state)
{
  bool consider_footprint = true;
  std::string motion_model = "DiffDrive";
  std::vector<std::string> critics = {{"GoalCritic"}, {"GoalAngleCritic"}, {"ObstaclesCritic"},
    {"PathAngleCritic"}, {"PathFollowCritic"}, {"PreferForwardCritic"}};

  prepareAndRunBenchmark(consider_footprint, motion_model, critics, state);
}


static void BM_Omni(benchmark::State & state)
{
  bool consider_footprint = true;
  std::string motion_model = "Omni";
  std::vector<std::string> critics = {{"GoalCritic"}, {"GoalAngleCritic"}, {"ObstaclesCritic"},
    {"TwirlingCritic"}, {"PathFollowCritic"}, {"PreferForwardCritic"}};

  prepareAndRunBenchmark(consider_footprint, motion_model, critics, state);
}

static void BM_Ackermann(benchmark::State & state)
{
  bool consider_footprint = true;
  std::string motion_model = "Ackermann";
  std::vector<std::string> critics = {{"GoalCritic"}, {"GoalAngleCritic"}, {"ObstaclesCritic"},
    {"PathAngleCritic"}, {"PathFollowCritic"}, {"PreferForwardCritic"}};

  prepareAndRunBenchmark(consider_footprint, motion_model, critics, state);
}

static void BM_GoalCritic(benchmark::State & state)
{
  bool consider_footprint = true;
  std::string motion_model = "Ackermann";
  std::vector<std::string> critics = {{"GoalCritic"}};

  prepareAndRunBenchmark(consider_footprint, motion_model, critics, state);
}

static void BM_GoalAngleCritic(benchmark::State & state)
{
  bool consider_footprint = true;
  std::string motion_model = "Ackermann";
  std::vector<std::string> critics = {{"GoalAngleCritic"}};

  prepareAndRunBenchmark(consider_footprint, motion_model, critics, state);
}

static void BM_ObstaclesCritic(benchmark::State & state)
{
  bool consider_footprint = true;
  std::string motion_model = "Ackermann";
  std::vector<std::string> critics = {{"ObstaclesCritic"}};

  prepareAndRunBenchmark(consider_footprint, motion_model, critics, state);
}

static void BM_ObstaclesCriticPointFootprint(benchmark::State & state)
{
  bool consider_footprint = false;
  std::string motion_model = "Ackermann";
  std::vector<std::string> critics = {{"ObstaclesCritic"}};

  prepareAndRunBenchmark(consider_footprint, motion_model, critics, state);
}

static void BM_TwilringCritic(benchmark::State & state)
{
  bool consider_footprint = true;
  std::string motion_model = "Ackermann";
  std::vector<std::string> critics = {{"TwirlingCritic"}};

  prepareAndRunBenchmark(consider_footprint, motion_model, critics, state);
}

static void BM_PathFollowCritic(benchmark::State & state)
{
  bool consider_footprint = true;
  std::string motion_model = "Ackermann";
  std::vector<std::string> critics = {{"PathFollowCritic"}};

  prepareAndRunBenchmark(consider_footprint, motion_model, critics, state);
}

static void BM_PathAngleCritic(benchmark::State & state)
{
  bool consider_footprint = true;
  std::string motion_model = "Ackermann";
  std::vector<std::string> critics = {{"PathAngleCritic"}};

  prepareAndRunBenchmark(consider_footprint, motion_model, critics, state);
}

BENCHMARK(BM_DiffDrivePointFootprint)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_DiffDrive)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_Omni)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_Ackermann)->Unit(benchmark::kMillisecond);

BENCHMARK(BM_GoalCritic)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_GoalAngleCritic)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_PathAngleCritic)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_PathFollowCritic)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_ObstaclesCritic)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_ObstaclesCriticPointFootprint)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_TwilringCritic)->Unit(benchmark::kMillisecond);

BENCHMARK_MAIN();