// Copyright 2021 Open Source Robotics Foundation, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "rclcpp/detail/add_guard_condition_to_rcl_wait_set.hpp"
#include "rclcpp/experimental/service_intra_process_base.hpp"

using rclcpp::experimental::ServiceIntraProcessBase;

bool
ServiceIntraProcessBase::add_to_wait_set(rcl_wait_set_t * wait_set)
{
  detail::add_guard_condition_to_rcl_wait_set(*wait_set, gc_);
  return true;
}

const char *
ServiceIntraProcessBase::get_service_name() const
{
  return service_name_.c_str();
}

rclcpp::QoS
ServiceIntraProcessBase::get_actual_qos() const
{
  return qos_profile_;
}

void
ServiceIntraProcessBase::add_intra_process_client(
  rclcpp::experimental::ClientIntraProcessBase::SharedPtr client,
  uint64_t client_id)
{
  std::unique_lock<std::recursive_mutex> lock(reentrant_mutex_);
  clients_[client_id] = client;
}
