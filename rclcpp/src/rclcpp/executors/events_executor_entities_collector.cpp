// Copyright 2020 Open Source Robotics Foundation, Inc.
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

#include "rclcpp/executors/events_executor.hpp"
#include "rclcpp/executors/events_executor_entities_collector.hpp"

using rclcpp::executors::EventsExecutorEntitiesCollector;

EventsExecutorEntitiesCollector::~EventsExecutorEntitiesCollector()
{
  // Disassociate all nodes
  for (const auto & weak_node : weak_nodes_) {
    auto node = weak_node.lock();
    if (node) {
      std::atomic_bool & has_executor = node->get_associated_with_executor_atomic();
      has_executor.store(false);
    }
  }
  weak_nodes_.clear();
}

void
EventsExecutorEntitiesCollector::init(
  void * executor_context,
  ExecutorEventCallback executor_callback,
  TimerFn push_timer,
  TimerFn clear_timer,
  ClearTimersFn clear_all_timers)
{
  // These callbacks are used when:
  // 1. A new entity is added/removed to/from a new node
  // 2. A node is removed from the executor
  executor_context_ = executor_context;
  executor_callback_ = executor_callback;
  push_timer_ = push_timer;
  clear_timer_ = clear_timer;
  clear_all_timers_ = clear_all_timers;
}

// The purpose of "execute" is handling the situation of a new entity added to
// a node, while the executor is already spinning.
// With the new approach, "execute" should only take care of setting that
// entitiy's callback.
// If a entity is removed from a node, we should unset its callback
void
EventsExecutorEntitiesCollector::execute()
{
  clear_all_timers_();
  set_entities_callbacks();
}

void
EventsExecutorEntitiesCollector::add_node(
    rclcpp::node_interfaces::NodeBaseInterface::SharedPtr node_ptr)
{
  // If the node already has an executor
  std::atomic_bool & has_executor = node_ptr->get_associated_with_executor_atomic();

  if (has_executor.exchange(true)) {
    throw std::runtime_error("Node has already been added to an executor.");
  }

  weak_nodes_.push_back(node_ptr);
}

// Here we unset the node entities callback.
void
EventsExecutorEntitiesCollector::remove_node(
  rclcpp::node_interfaces::NodeBaseInterface::SharedPtr node_ptr)
{
  auto node_it = weak_nodes_.begin();

  while (node_it != weak_nodes_.end()) {
    bool matched = (node_it->lock() == node_ptr);
    if (matched) {
      // Node found: unset its entities callbacks
      rcl_ret_t ret = rcl_guard_condition_set_events_executor_callback(
                        nullptr, nullptr, nullptr,
                        node_ptr->get_notify_guard_condition(),
                        false);

      if (ret != RCL_RET_OK) {
        throw std::runtime_error(std::string("Couldn't set guard condition callback"));
      }

      // Unset entities callbacks
      for (auto & weak_group : node_ptr->get_callback_groups()) {
        auto group = weak_group.lock();
        if (!group || !group->can_be_taken_from().load()) {
          continue;
        }
        group->find_timer_ptrs_if(
          [this](const rclcpp::TimerBase::SharedPtr & timer) {
            if (timer) {
              clear_timer_(timer);
            }
            return false;
          });
        group->find_subscription_ptrs_if(
          [this](const rclcpp::SubscriptionBase::SharedPtr & subscription) {
            if (subscription) {
              subscription->set_events_executor_callback(nullptr, nullptr);
            }
            return false;
          });
        group->find_service_ptrs_if(
          [this](const rclcpp::ServiceBase::SharedPtr & service) {
            if (service) {
              service->set_events_executor_callback(nullptr, nullptr);
            }
            return false;
          });
        group->find_client_ptrs_if(
          [this](const rclcpp::ClientBase::SharedPtr & client) {
            if (client) {
              client->set_events_executor_callback(nullptr, nullptr);
            }
            return false;
          });
        group->find_waitable_ptrs_if(
          [this](const rclcpp::Waitable::SharedPtr & waitable) {
            if (waitable) {
              waitable->set_events_executor_callback(nullptr, nullptr);
            }
            return false;
          });
      }

      weak_nodes_.erase(node_it);
      return;
    } else {
      ++node_it;
    }
  }
}

void
EventsExecutorEntitiesCollector::set_entities_callbacks()
{
  for (auto & weak_node : weak_nodes_) {
    auto node = weak_node.lock();
    if (!node) {
      continue;
    }
    // Check in all the callback groups
    for (auto & weak_group : node->get_callback_groups()) {
      auto group = weak_group.lock();
      if (!group || !group->can_be_taken_from().load()) {
        continue;
      }
      group->find_timer_ptrs_if(
        [this](const rclcpp::TimerBase::SharedPtr & timer) {
          if (timer) {
            push_timer_(timer);
          }
          return false;
        });
      group->find_subscription_ptrs_if(
        [this](const rclcpp::SubscriptionBase::SharedPtr & subscription) {
          if (subscription) {
            subscription->set_events_executor_callback(executor_context_, executor_callback_);
          }
          return false;
        });
      group->find_service_ptrs_if(
        [this](const rclcpp::ServiceBase::SharedPtr & service) {
          if (service) {
            service->set_events_executor_callback(executor_context_, executor_callback_);
          }
          return false;
        });
      group->find_client_ptrs_if(
        [this](const rclcpp::ClientBase::SharedPtr & client) {
          if (client) {
            client->set_events_executor_callback(executor_context_, executor_callback_);
          }
          return false;
        });
      group->find_waitable_ptrs_if(
        [this](const rclcpp::Waitable::SharedPtr & waitable) {
          if (waitable) {
            waitable->set_events_executor_callback(executor_context_, executor_callback_);
          }
          return false;
        });
    }
  }
}