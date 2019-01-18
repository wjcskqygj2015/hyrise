#pragma once

#include <map>
#include <string>
#include <unordered_map>

#include "all_type_variant.hpp"

namespace opossum {
namespace cost_model {

struct AbstractFeatures {
  AbstractFeatures() = default;
  virtual ~AbstractFeatures() = default;
  AbstractFeatures(AbstractFeatures&&) noexcept = default;
  AbstractFeatures& operator=(AbstractFeatures&&) noexcept = default;
  AbstractFeatures(const AbstractFeatures&) = default;
  AbstractFeatures& operator=(const AbstractFeatures&) = default;

  virtual const std::map<std::string, AllTypeVariant> serialize() const = 0;
  virtual const std::unordered_map<std::string, float> to_cost_model_features() const = 0;
  const std::vector<std::string> feature_names() const;
};

}  // namespace cost_model
}  // namespace opossum