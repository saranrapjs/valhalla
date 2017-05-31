#include <cmath>
#include <iomanip>
#include <iostream>
#include <locale>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

#include <google/protobuf/util/json_util.h>

#include "proto/route.pb.h"
#include "proto/navigator.pb.h"
#include "midgard/constants.h"
#include "midgard/encoded.h"
#include "midgard/logging.h"
#include "midgard/pointll.h"
#include "baldr/json.h"
#include "baldr/location.h"
#include "baldr/errorcode_util.h"
#include "tyr/navigator.h"

using namespace valhalla;
using namespace valhalla::baldr;
using namespace valhalla::midgard;

namespace valhalla {
namespace tyr {

Navigator::Navigator(const std::string& route_json_str) {
  route_state_ = NavigationStatus_RouteState_kInvalid;
  set_route(route_json_str);
}

const Route& Navigator::route() const {
  return route_;
}

void Navigator::set_route(const std::string& route_json_str) {
  google::protobuf::util::JsonStringToMessage(route_json_str, &route_);
  leg_index_ = 0;
  maneuver_index_ = 0;
  SetUnits();
  SetShapeLengthTime();
  SetUsedInstructions();
  route_state_ = NavigationStatus_RouteState_kInitialized;
}

NavigationStatus Navigator::OnLocationChanged(const FixLocation& fix_location) {
  NavigationStatus nav_status;

  // Set the previous route state prior to snapping to route
  NavigationStatus_RouteState prev_route_state = route_state_;

  // Snap the fix location to the route
  SnapToRoute(fix_location, nav_status);

  // Only process a valid route state
  if (nav_status.route_state() != NavigationStatus_RouteState_kInvalid) {
    // If starting navigation and close to origin
    // and origin maneuver index and instruction has not been used
    // then set route state to kPreTransition
    if (StartingNavigation(prev_route_state, route_state_)
        && OnRouteLocationCloseToOrigin(nav_status) && (maneuver_index_ == 0)
        && !(std::get<kPreTransition>(used_instructions_.at(maneuver_index_)))) {
      // Set route state
      route_state_ = NavigationStatus_RouteState_kPreTransition;
      nav_status.set_route_state(route_state_);

      // Set the instruction maneuver index for the start maneuver
      nav_status.set_instruction_maneuver_index(maneuver_index_);

      // Mark that the pre transition was used
      std::get<kPreTransition>(used_instructions_.at(maneuver_index_)) = true;
    }
    // else if instruction has not been used
    // and route location is pre transition
    // then set route state to kPreTransition
//    else if (!(std::get<kPreTransition>(used_instructions_.at(maneuver_index_ + 1)))
//        && (nav_status.remaining_maneuver_time() < GetPreTransitionThreshold(maneuver_index_ + 1))) {
      // Set route state
//      route_state_ = NavigationStatus_RouteState_kPreTransition;
//      nav_status.set_route_state(route_state_);

      // Set the instruction maneuver index for the next maneuver
//      nav_status.set_instruction_maneuver_index(maneuver_index_ + 1);

      // Mark that the pre transition was used
//      std::get<kPreTransition>(used_instructions_.at(maneuver_index_ + 1)) = true;
//    }

    // else if route location is post transition
    // and maneuver has a post transition
    // and instruction has not been used
    // then set route state to kPostTransition
    // TODO

    // else if route location is transition alert
    // and maneuver has a transition alert
    // and instruction has not been used
    // then set route state to kTransitionAlert
    // TODO
  }
  return nav_status;
}

void Navigator::SetUnits() {
  if (route_.has_trip() && (route_.trip().has_units())
      && route_.trip().units() == "miles") {
    kilometer_units_ = false;
  } else {
    kilometer_units_ = true;
  }
}

bool Navigator::HasKilometerUnits() const {
  return kilometer_units_;
}

void Navigator::SetShapeLengthTime() {
  if (route_.has_trip() && (route_.trip().legs_size() > 0)
      && route_.trip().legs(leg_index_).has_shape()) {
    shape_ = midgard::decode<std::vector<PointLL> >(
        route_.trip().legs(leg_index_).shape());

    // Create maneuver speeds in units per second
    maneuver_speeds_.clear();
    //GDG
    std::cout << std::endl << "SPEED ======================================" << std::endl;
    size_t zzz = 0;
    for (const auto& maneuver : route_.trip().legs(leg_index_).maneuvers()) {
      // Calculate speed in units per second - protect against divide by zero
      float time = (maneuver.time() == 0) ? 0.000028f : maneuver.time();
      float speed = (maneuver.length()/time);
      //GDG
      std::cout << "index=" << zzz++ << " | maneuver.length()=" << maneuver.length() << " | maneuver.time()=" << maneuver.time() << " | time=" << time << " | speed(units/sec)=" << speed << " | speed(units/hour)=" << (speed*3600) << std::endl;
      maneuver_speeds_.emplace_back(speed);
    }

    // Initialize the remaining leg length and time
    float km_length = 0.0f;
    float length = 0.0f;
    float total_remaining_leg_length = 0.0f;
    uint32_t total_remaining_leg_time = 0;

    // Resize vector for current shape
    remaining_leg_values_.resize(shape_.size());

    // Initialize indexes and set destination shape values
    size_t maneuver_speed_index = (maneuver_speeds_.size() - 1);
    int i = (remaining_leg_values_.size() - 1);
    remaining_leg_values_[i--] = {total_remaining_leg_length, total_remaining_leg_time};

    //GDG
    std::cout << std::endl << "remaining_leg_values ======================================" << std::endl;
    // Process shape to set remaining length and time
    if (remaining_leg_values_.size() > 1) {
      for (; i >= 0; --i) {
        // Determine length between shape points and convert to appropriate units
        km_length = (shape_[i].Distance(shape_[i+1]) * midgard::kKmPerMeter);
        length = (HasKilometerUnits() ? km_length : (km_length * midgard::kMilePerKm));

        // Find the maneuver index that corresponds to the shape index
        maneuver_speed_index = RfindManeuverIndex(maneuver_speed_index, i);

        // Update the total remaining values
        total_remaining_leg_length += length;
        total_remaining_leg_time += static_cast<uint32_t>(round(length/maneuver_speeds_.at(maneuver_speed_index)));

        //GDG
        std::cout << "i=" << i << " | maneuver_speed_index=" << maneuver_speed_index << " | total_remaining_leg_length=" << total_remaining_leg_length << " | total_remaining_leg_time=" << total_remaining_leg_time << std::endl;
        remaining_leg_values_[i] = {total_remaining_leg_length, total_remaining_leg_time};
      }
    }
  } else {
    shape_.resize(0);
    remaining_leg_values_.resize(0);
  }
  current_shape_index_ = 0;
}

void Navigator::SetUsedInstructions() {
  used_instructions_.clear();
  for (size_t i = 0; i < route_.trip().legs(leg_index_).maneuvers_size(); ++i) {
    used_instructions_.emplace_back(false, false, false, false);
  }
}

bool Navigator::IsDestinationShapeIndex(size_t idx) const {
  return (idx == (shape_.size() - 1));
}

size_t Navigator::FindManeuverIndex(size_t begin_search_index,
    size_t shape_index) const {

  // Set the destination maneuver index - since destination maneuver is a special case
  size_t destination_maneuver_index =
      (route_.trip().legs(leg_index_).maneuvers_size() - 1);

  // Validate the begin_search_index
  if ((route_.trip().legs(leg_index_).maneuvers_size() == 0)
      || (begin_search_index > destination_maneuver_index))
    throw valhalla_exception_t{400, 502};

  // Check for destination shape index and return destination maneuver index
  if (IsDestinationShapeIndex(shape_index))
    return destination_maneuver_index;

  // Loop over maneuvers - starting at specified maneuver index and return
  // the maneuver index that contains the specified shape index
  for (size_t i = begin_search_index; i < destination_maneuver_index; ++i) {
    const auto& maneuver = route_.trip().legs(leg_index_).maneuvers(i);
    if ((shape_index >= maneuver.begin_shape_index())
        && (shape_index < maneuver.end_shape_index()))
      return i;
  }
  // If not found, throw exception
  throw valhalla_exception_t{400, 502};
}

size_t Navigator::RfindManeuverIndex(size_t rbegin_search_index,
    size_t shape_index) const {

  // Set the destination maneuver index - since destination maneuver is a special case
  size_t destination_maneuver_index =
      (route_.trip().legs(leg_index_).maneuvers_size() - 1);

  // Validate the rbegin_search_index
  if ((route_.trip().legs(leg_index_).maneuvers_size() == 0)
      || (rbegin_search_index > destination_maneuver_index))
    throw valhalla_exception_t { 400, 502 };

  // Check for destination shape index and rbegin search index
  // if so, return destination maneuver index
  if (IsDestinationShapeIndex(shape_index)
      && (destination_maneuver_index == rbegin_search_index))
    return destination_maneuver_index;

  // Loop over maneuvers in reverse - starting at specified maneuver index
  // and return the maneuver index that contains the specified shape index
  for (size_t i = rbegin_search_index; (i >= 0 && i <= destination_maneuver_index); --i) {
    const auto& maneuver = route_.trip().legs(leg_index_).maneuvers(i);
    if ((shape_index >= maneuver.begin_shape_index()) && (shape_index < maneuver.end_shape_index()))
      return i;
  }
  // If not found, throw exception
  throw valhalla_exception_t{400, 502};
}

void Navigator::SnapToRoute(const FixLocation& fix_location,
    NavigationStatus& nav_status) {

  // Find the closest point on the route that corresponds to the fix location
  PointLL fix_pt = PointLL(fix_location.lon(), fix_location.lat());
  auto closest = fix_pt.ClosestPoint(shape_, current_shape_index_);

  // GDG - rm
  std::cout << std::endl << "--------------------------------------------------------------------------------------------" << std::endl;
  std::cout << "LL=" << std::get<kClosestPoint>(closest).lat() << "," << std::get<kClosestPoint>(closest).lng() << " | distance=" << std::get<kClosestPointDistance>(closest) << " | index=" << std::get<kClosestPointSegmentIndex>(closest) << std::endl;

  // If the fix point distance from route is greater than the off route threshold
  // then return invalid route state
  if (std::get<kClosestPointDistance>(closest) > kOffRouteThreshold) {
    route_state_ = NavigationStatus_RouteState_kInvalid;
    nav_status.set_route_state(route_state_);
    return;
  }

  // If not off route then set closest point and current shape index
  PointLL closest_ll = std::get<kClosestPoint>(closest);
  current_shape_index_ = std::get<kClosestPointSegmentIndex>(closest);

  // If approximately equal to the next shape point then snap to it and set flag
  bool snapped_to_shape_point = false;
  if (!IsDestinationShapeIndex(current_shape_index_)
      && closest_ll.ApproximatelyEqual(shape_.at(current_shape_index_ + 1))) {
    // Increment current shape index
    ++current_shape_index_;
    // Set at shape point flag
    snapped_to_shape_point = true;
  }

  // Set the remaining index
  size_t remaining_index = 0;
  if (snapped_to_shape_point || IsDestinationShapeIndex(current_shape_index_))
    remaining_index = current_shape_index_;
  else
    remaining_index = (current_shape_index_ + 1);

  // Calculate the partial length, if needed
  float partial_length = 0.0f;
  if (!snapped_to_shape_point && !IsDestinationShapeIndex(current_shape_index_)) {
    partial_length = (closest_ll.Distance(shape_.at(remaining_index)) * midgard::kKmPerMeter);
    // Convert to miles, if needed
    if (!HasKilometerUnits())
      partial_length = (partial_length * midgard::kMilePerKm);
  }

  // Set the maneuver index and maneuver end shape index
  maneuver_index_ = FindManeuverIndex(maneuver_index_, current_shape_index_);
  uint32_t maneuver_end_shape_index = route_.trip().legs(leg_index_).maneuvers(maneuver_index_).end_shape_index();

  // Set the remaining leg length and time values
  float remaining_leg_length = (remaining_leg_values_.at(remaining_index).first
      + partial_length);
  uint32_t remaining_leg_time = (remaining_leg_values_.at(remaining_index).second
      + static_cast<uint32_t>(round(
          partial_length / maneuver_speeds_.at(maneuver_index_))));

  // GDG - rm
  std::cout << "current_shape_index_=" << current_shape_index_ << " | remaining_index=" << remaining_index << " | maneuver_index_=" << maneuver_index_ << " | snapped_to_shape_point=" << (snapped_to_shape_point ? "true" : "false") << std::endl;
  std::cout << "remaining_leg_length=" << remaining_leg_length << " | remaining_leg_lengths_.at(remaining_index)=" << remaining_leg_values_.at(remaining_index).first << " | partial_length=" << partial_length << " | remaining_maneuver_length=" << (remaining_leg_length - remaining_leg_values_.at(maneuver_end_shape_index).first) << " | remaining_leg_lengths_.at(maneuver_end_shape_index)=" << remaining_leg_values_.at(maneuver_end_shape_index).first << std::endl;
  std::cout << "remaining_leg_time=" << remaining_leg_time << " | remaining_leg_values_.at(remaining_index).second=" << remaining_leg_values_.at(remaining_index).second << " | partial time=" <<  static_cast<uint32_t>(round(partial_length / maneuver_speeds_.at(maneuver_index_))) << " | remaining_maneuver_time=" << (remaining_leg_time - remaining_leg_values_.at(maneuver_end_shape_index).second) << " | speed=" << maneuver_speeds_.at(maneuver_index_) << std::endl;

  // Populate navigation status
  route_state_ = NavigationStatus_RouteState_kTracking;
  nav_status.set_route_state(route_state_);
  nav_status.set_lon(closest_ll.lng());
  nav_status.set_lat(closest_ll.lat());
  nav_status.set_leg_index(leg_index_);
  nav_status.set_remaining_leg_length(remaining_leg_length);
  nav_status.set_remaining_leg_time(remaining_leg_time);
  nav_status.set_maneuver_index(maneuver_index_);
  nav_status.set_remaining_maneuver_length(remaining_leg_length - remaining_leg_values_.at(maneuver_end_shape_index).first);
  nav_status.set_remaining_maneuver_time(remaining_leg_time - remaining_leg_values_.at(maneuver_end_shape_index).second);
}

bool Navigator::StartingNavigation(
    const NavigationStatus_RouteState& prev_route_state,
    const NavigationStatus_RouteState& curr_route_state) const {
  return ((prev_route_state == NavigationStatus_RouteState_kInitialized)
      && (curr_route_state == NavigationStatus_RouteState_kTracking));
}

bool Navigator::OnRouteLocationCloseToOrigin(
    const NavigationStatus& nav_status) const {
  if ((remaining_leg_values_.size() > 0)
      && nav_status.has_remaining_leg_length()) {
    float meters = UnitsToMeters(
        remaining_leg_values_.at(0).first - nav_status.remaining_leg_length());
    return (meters <= kOnRouteCloseToOriginThreshold);
  }
  return false;
}

float Navigator::UnitsToMeters(float units) const {
  float km_length = 0.0f;
  if (HasKilometerUnits())
    km_length = units;
  else
    km_length = units * midgard::kKmPerMile;

  return (km_length * kMetersPerKm);
}

size_t Navigator::GetWordCount(const std::string& instruction) const {
  size_t word_count = 0;
  std::string::const_iterator pos = instruction.begin();
  std::string::const_iterator end = instruction.end();

  while(pos != end)
  {
    // Skip over space, white space, and punctuation
    while (pos != end
        && ((*pos == ' ') || std::isspace(*pos) || std::ispunct(*pos)))
      ++pos;

    // Word found - increment
    word_count += (pos != end);

    // Skip over letters in word
    while (pos != end
        && ((*pos != ' ') && (!std::isspace(*pos)) && (!std::ispunct(*pos))))
      ++pos;
  }
  return word_count;
}

}
}

