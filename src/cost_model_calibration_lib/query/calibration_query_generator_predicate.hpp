#pragma once

#include "../configuration/calibration_column_specification.hpp"
#include "../configuration/calibration_configuration.hpp"
#include "expression/value_expression.hpp"
#include "logical_query_plan/predicate_node.hpp"
#include "logical_query_plan/stored_table_node.hpp"

namespace opossum {

struct CalibrationQueryGeneratorPredicateConfiguration {
  const std::string table_name;
  const DataType data_type;
  const EncodingType first_encoding_type;
  const std::optional<EncodingType> second_encoding_type;
  const std::optional<EncodingType> third_encoding_type;
  const float selectivity;
  const bool reference_column;
  const size_t row_count;
};

    inline bool operator==(const CalibrationQueryGeneratorPredicateConfiguration& lhs, const CalibrationQueryGeneratorPredicateConfiguration& rhs) {
      return std::tie(lhs.table_name, lhs.data_type, lhs.first_encoding_type, lhs.second_encoding_type, lhs.third_encoding_type, lhs.selectivity,
                      lhs.reference_column, lhs.row_count) == std::tie(rhs.table_name, rhs.data_type, rhs.first_encoding_type, rhs.second_encoding_type,
                                                rhs.third_encoding_type, rhs.selectivity, rhs.reference_column, rhs.row_count);
    }
    inline bool operator<(const CalibrationQueryGeneratorPredicateConfiguration& lhs, const CalibrationQueryGeneratorPredicateConfiguration& rhs) {
      return std::tie(lhs.table_name, lhs.data_type, lhs.first_encoding_type, lhs.second_encoding_type, lhs.third_encoding_type, lhs.selectivity,
                      lhs.reference_column, lhs.row_count) < std::tie(rhs.table_name, rhs.data_type, rhs.first_encoding_type, rhs.second_encoding_type,
                                                                       rhs.third_encoding_type, rhs.selectivity, rhs.reference_column, rhs.row_count);
    }

// So that google test prints readable error messages
inline std::ostream& operator<<(std::ostream& stream,
                                const CalibrationQueryGeneratorPredicateConfiguration& configuration) {
  const auto reference_column_string = configuration.reference_column ? "true" : "false";
  const auto second_encoding_string =
      configuration.second_encoding_type ? encoding_type_to_string.left.at(*configuration.second_encoding_type) : "{}";
  const auto third_encoding_string =
      configuration.third_encoding_type ? encoding_type_to_string.left.at(*configuration.third_encoding_type) : "{}";
  return stream << "CalibrationQueryGeneratorPredicateConfiguration(" << configuration.table_name << " - "
                << configuration.selectivity << " - "
                << encoding_type_to_string.left.at(configuration.first_encoding_type) << " - " << second_encoding_string
                << " - " << third_encoding_string << " - " << data_type_to_string.left.at(configuration.data_type)
                << " - " << reference_column_string << " - " << configuration.row_count << ")";
}

struct PredicateGeneratorFunctorConfiguration {
  const std::shared_ptr<StoredTableNode>& table;
  const std::vector<CalibrationColumnSpecification>& column_definitions;
  const CalibrationQueryGeneratorPredicateConfiguration& configuration;
};

using PredicateGeneratorFunctor =
    std::function<const std::shared_ptr<AbstractExpression>(const PredicateGeneratorFunctorConfiguration&)>;

class CalibrationQueryGeneratorPredicate {
 public:
  static const std::vector<CalibrationQueryGeneratorPredicateConfiguration> generate_predicate_permutations(
      const std::vector<std::pair<std::string, size_t>>& tables, const CalibrationConfiguration& configuration);

  static const std::vector<std::shared_ptr<PredicateNode>> generate_predicates(
      const PredicateGeneratorFunctor& predicate_generator,
      const std::vector<CalibrationColumnSpecification>& column_definitions,
      const std::shared_ptr<StoredTableNode>& table,
      const CalibrationQueryGeneratorPredicateConfiguration& configuration, bool generate_index_scan = false);

  /*
   * Functors to generate predicates.
   * They all implement 'PredicateGeneratorFunctor'
   */
  static const std::shared_ptr<AbstractExpression> generate_predicate_between_value_value(
      const PredicateGeneratorFunctorConfiguration& generator_configuration);

  static const std::shared_ptr<AbstractExpression> generate_predicate_between_column_column(
      const PredicateGeneratorFunctorConfiguration& generator_configuration);

  static const std::shared_ptr<AbstractExpression> generate_predicate_column_value(
      const PredicateGeneratorFunctorConfiguration& generator_configuration);

  static const std::shared_ptr<AbstractExpression> generate_predicate_column_column(
      const PredicateGeneratorFunctorConfiguration& generator_configuration);

  static const std::shared_ptr<AbstractExpression> generate_predicate_like(
      const PredicateGeneratorFunctorConfiguration& generator_configuration);

  static const std::shared_ptr<AbstractExpression> generate_predicate_equi_on_strings(
      const PredicateGeneratorFunctorConfiguration& generator_configuration);

  static const std::shared_ptr<AbstractExpression> generate_predicate_or(
      const PredicateGeneratorFunctorConfiguration& generator_configuration);

 private:
  static const std::optional<CalibrationColumnSpecification> _find_column_for_configuration(
      const std::vector<CalibrationColumnSpecification>& column_definitions, const DataType& data_type,
      const EncodingType& encoding_type);

  static const std::shared_ptr<ValueExpression> _generate_value_expression(const DataType& data_type,
                                                                           const float selectivity,
                                                                           const bool trailing_like = false);

  static const std::shared_ptr<LQPColumnExpression> _generate_column_expression(
      const std::shared_ptr<StoredTableNode>& table, const CalibrationColumnSpecification& filter_column);

  CalibrationQueryGeneratorPredicate() = default;
};

}  // namespace opossum