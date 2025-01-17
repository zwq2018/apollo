/******************************************************************************
 * Copyright 2019 The Apollo Authors. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *****************************************************************************/

/**
 * @file
 **/

#include <string>

#include "modules/planning/scenarios/valet_parking/stage_approaching_parking_spot.h"

#include "modules/common/vehicle_state/vehicle_state_provider.h"

namespace apollo {
namespace planning {
namespace scenario {
namespace valet_parking {

Stage::StageStatus StageApproachingParkingSpot::Process(
    const common::TrajectoryPoint& planning_init_point, Frame* frame) {
  ADEBUG << "stage: StageApproachingParkingSpot";
  CHECK_NOTNULL(frame);
  GetContext()->target_parking_spot_id.clear();
  if (frame->local_view().routing->routing_request().has_parking_space() &&
      frame->local_view().routing->routing_request().parking_space().has_id()) {
    GetContext()->target_parking_spot_id = frame->local_view()
                                               .routing->routing_request()
                                               .parking_space()
                                               .id()
                                               .id();
  } else {
    AERROR << "No parking space id from routing";
    return StageStatus::ERROR;
  }

  if (GetContext()->target_parking_spot_id.empty()) {
    return StageStatus::ERROR;
  }

  frame->mutable_open_space_info()->set_open_space_pre_stop_finished(
      GetContext()->valet_parking_pre_stop_finished);
  *(frame->mutable_open_space_info()->mutable_target_parking_spot_id()) =
      GetContext()->target_parking_spot_id;
  bool plan_ok = ExecuteTaskOnReferenceLine(planning_init_point, frame);
  if (!plan_ok) {
    AERROR << "StopSignUnprotectedStagePreStop planning error";
    return StageStatus::ERROR;
  }
  GetContext()->valet_parking_pre_stop_fence_s =
      frame->open_space_info().open_space_pre_stop_fence_s();

  const auto& reference_line_info = frame->reference_line_info().front();

  if (CheckADCStop(reference_line_info)) {
    next_stage_ = ScenarioConfig::VALET_PARKING_PARKING;
    return Stage::FINISHED;
  }

  return Stage::RUNNING;
}

bool StageApproachingParkingSpot::CheckADCStop(
    const ReferenceLineInfo& reference_line_info) {
  const double adc_speed =
      common::VehicleStateProvider::Instance()->linear_velocity();
  if (adc_speed > scenario_config_.max_adc_stop_speed()) {
    ADEBUG << "ADC not stopped: speed[" << adc_speed << "]";
    return false;
  }

  // check stop close enough to stop line of the stop_sign
  const double adc_front_edge_s = reference_line_info.AdcSlBoundary().end_s();
  const double stop_fence_start_s =
      GetContext()->valet_parking_pre_stop_fence_s;
  const double distance_stop_line_to_adc_front_edge =
      stop_fence_start_s - adc_front_edge_s;

  if (distance_stop_line_to_adc_front_edge >
      scenario_config_.max_valid_stop_distance()) {
    ADEBUG << "not a valid stop. too far from stop line.";
    return false;
  }
  GetContext()->valet_parking_pre_stop_finished = true;
  return true;
}

}  // namespace valet_parking
}  // namespace scenario
}  // namespace planning
}  // namespace apollo
