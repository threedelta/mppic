// Copyright 2022 @artofnothingness Aleksei Budyakov, Samsung Research

#pragma once

#include <xtensor/xtensor.hpp>

#include "geometry_msgs/msg/pose_stamped.hpp"
#include "nav2_core/goal_checker.hpp"
#include "mppic/models/state.hpp"


namespace mppi::models
{

struct CriticFunctionData
{
  const models::State & state;
  const xt::xtensor<double, 3> & trajectories;
  const xt::xtensor<double, 2> & path;

  xt::xtensor<double, 1> & costs;

  bool fail_flag;
  nav2_core::GoalChecker * goal_checker;
  std::optional<size_t> furthest_reached_path_point;
};

}  // namespace mppi::models