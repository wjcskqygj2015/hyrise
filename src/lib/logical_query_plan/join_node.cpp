#include "join_node.hpp"

#include <limits>
#include <memory>
#include <numeric>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "constant_mappings.hpp"
#include "expression/binary_predicate_expression.hpp"
#include "expression/expression_utils.hpp"
#include "expression/lqp_column_expression.hpp"
#include "operators/operator_join_predicate.hpp"
#include "types.hpp"
#include "utils/assert.hpp"

namespace opossum {

JoinNode::JoinNode(const JoinMode init_join_mode) : AbstractLQPNode(LQPNodeType::Join), join_mode(init_join_mode) {
  Assert(join_mode == JoinMode::Cross, "Only Cross Joins can be constructed without predicate");
}

JoinNode::JoinNode(const JoinMode init_join_mode, const std::shared_ptr<AbstractExpression>& join_predicate)
    : JoinNode(init_join_mode, std::vector<std::shared_ptr<AbstractExpression>>{join_predicate}) {}

JoinNode::JoinNode(const JoinMode init_join_mode,
                   const std::vector<std::shared_ptr<AbstractExpression>>& init_join_predicates)
    : AbstractLQPNode(LQPNodeType::Join, init_join_predicates), join_mode(init_join_mode) {
  Assert(join_mode != JoinMode::Cross, "Cross Joins take no predicate");
  Assert(!join_predicates().empty(), "Non-Cross Joins require predicates");
}

std::string JoinNode::description(const DescriptionMode mode) const {
  const auto expression_mode = _expression_description_mode(mode);

  std::stringstream stream;
  stream << "[Join] Mode: " << join_mode;

  for (const auto& predicate : join_predicates()) {
    stream << " [" << predicate->description(expression_mode) << "]";
  }

  return stream.str();
}

std::vector<std::shared_ptr<AbstractExpression>> JoinNode::column_expressions() const {
  Assert(left_input() && right_input(), "Both inputs need to be set to determine a JoinNode's output expressions");

  /**
   * Update the JoinNode's output expressions every time they are requested. An overhead, but keeps the LQP code simple.
   * Previously we propagated _input_changed() calls through the LQP every time a node changed and that required a lot
   * of feeble code.
   */

  const auto& left_expressions = left_input()->column_expressions();
  const auto& right_expressions = right_input()->column_expressions();

  const auto output_both_inputs =
      join_mode != JoinMode::Semi && join_mode != JoinMode::AntiNullAsTrue && join_mode != JoinMode::AntiNullAsFalse;

  auto column_expressions = std::vector<std::shared_ptr<AbstractExpression>>{};
  column_expressions.resize(left_expressions.size() + (output_both_inputs ? right_expressions.size() : 0));

  auto right_begin = std::copy(left_expressions.begin(), left_expressions.end(), column_expressions.begin());

  if (output_both_inputs) std::copy(right_expressions.begin(), right_expressions.end(), right_begin);

  return column_expressions;
}

const std::shared_ptr<ExpressionsConstraintDefinitions> JoinNode::constraints() const {
  auto output_constraints = std::make_shared<ExpressionsConstraintDefinitions>();

  // The Semi-Join outputs input_left() without adding any rows or columns. But depending on the right table,
  // tuples may be filtered out. As a consequence, we can forward the constraints.
  if(join_mode == JoinMode::Semi) return forward_constraints();

  // No guarantees for multi predicate joins
  if(join_predicates().size() > 1) return std::make_shared<ExpressionsConstraintDefinitions>();

  // No guarantees for Non-Equi Joins
  const auto join_predicate = std::dynamic_pointer_cast<BinaryPredicateExpression>(join_predicates().front());
  if(!join_predicate || join_predicate->predicate_condition != PredicateCondition::Equals) return {};

  // Check for uniqueness of join key columns
  bool left_operand_unique = left_input()->has_unique_constraint({join_predicate->left_operand()});
  bool right_operand_unique = right_input()->has_unique_constraint({join_predicate->right_operand()});

  switch(join_mode) {
  case JoinMode::Inner: {
      if(left_operand_unique && right_operand_unique) {
        // Due to the one-to-one relationship, the constraints of both sides remain valid.
        auto left_constraints = left_input()->constraints();
        auto right_constraints = right_input()->constraints();

        for(auto it = left_constraints->begin(); it != left_constraints->end();) {
          output_constraints->emplace(std::move(left_constraints->extract(it++).value()));
        }
        for(auto it = right_constraints->begin(); it != right_constraints->end();) {
          output_constraints->emplace(std::move(right_constraints->extract(it++).value()));
        }
        return output_constraints;

      } else if(left_operand_unique) {
        // Uniqueness on the left prevents duplication of records on the right
        return right_input()->constraints();

      } else if(right_operand_unique) {
        // Uniqueness on the right prevents duplication of records on the left
        return left_input()->constraints();

      } else {
        // No constraints to return.
        return output_constraints;
      }
    }
    case JoinMode::Left: {
      // The Left (outer) Join adds null values for tuples not present in the right table.
      // Therefore, input constraints of the right table have to be discarded.

      // TODO Forward input constraints input_left() if applicable
      return output_constraints;
    }
    case JoinMode::Right: {
      // The Right (outer) Join adds null values for tuples not present in the right table.
      // Therefore, input constraints of the left table have to be discarded.

      // TODO Forward input constraints input_right() if applicable
      return output_constraints;
    }
    case JoinMode::FullOuter: {
      // The Full Outer Join might produce null values in all output columns.
      // Therefore, we have to discard all input constraints
      return output_constraints;
    }
    case JoinMode::Cross: {
      // No uniqueness guarantee possible
      return output_constraints;
    }
    case JoinMode::Semi: {
      Fail("JoinMode::Semi should already be handled.");
    }
    case JoinMode::AntiNullAsTrue: {
      // ?
      return output_constraints;
    }
    case JoinMode::AntiNullAsFalse: {
      // ?
      return output_constraints;
    }
  }
  Fail("Unhandled JoinMode");
}

bool JoinNode::is_column_nullable(const ColumnID column_id) const {
  Assert(left_input() && right_input(), "Need both inputs to determine nullability");

  const auto left_input_column_count = left_input()->column_expressions().size();
  const auto column_is_from_left_input = column_id < left_input_column_count;

  if (join_mode == JoinMode::Left && !column_is_from_left_input) {
    return true;
  }

  if (join_mode == JoinMode::Right && column_is_from_left_input) {
    return true;
  }

  if (join_mode == JoinMode::FullOuter) {
    return true;
  }

  if (column_is_from_left_input) {
    return left_input()->is_column_nullable(column_id);
  } else {
    ColumnID right_column_id =
        static_cast<ColumnID>(column_id - static_cast<ColumnID::base_type>(left_input_column_count));
    return right_input()->is_column_nullable(right_column_id);
  }
}

const std::vector<std::shared_ptr<AbstractExpression>>& JoinNode::join_predicates() const { return node_expressions; }

size_t JoinNode::_on_shallow_hash() const { return boost::hash_value(join_mode); }

std::shared_ptr<AbstractLQPNode> JoinNode::_on_shallow_copy(LQPNodeMapping& node_mapping) const {
  if (!join_predicates().empty()) {
    return JoinNode::make(join_mode, expressions_copy_and_adapt_to_different_lqp(join_predicates(), node_mapping));
  } else {
    return JoinNode::make(join_mode);
  }
}

bool JoinNode::_on_shallow_equals(const AbstractLQPNode& rhs, const LQPNodeMapping& node_mapping) const {
  const auto& join_node = static_cast<const JoinNode&>(rhs);
  if (join_mode != join_node.join_mode) return false;
  return expressions_equal_to_expressions_in_different_lqp(join_predicates(), join_node.join_predicates(),
                                                           node_mapping);
}

}  // namespace opossum
