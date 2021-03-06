#pragma once

#include <mbgl/style/class_dictionary.hpp>
#include <mbgl/style/property_value.hpp>
#include <mbgl/style/data_driven_property_value.hpp>
#include <mbgl/style/property_evaluator.hpp>
#include <mbgl/style/cross_faded_property_evaluator.hpp>
#include <mbgl/style/data_driven_property_evaluator.hpp>
#include <mbgl/style/property_evaluation_parameters.hpp>
#include <mbgl/style/transition_options.hpp>
#include <mbgl/style/cascade_parameters.hpp>
#include <mbgl/style/paint_property_binder.hpp>
#include <mbgl/util/constants.hpp>
#include <mbgl/util/interpolate.hpp>
#include <mbgl/util/indexed_tuple.hpp>
#include <mbgl/util/ignore.hpp>

#include <utility>

namespace mbgl {

class GeometryTileFeature;

namespace style {

template <class Value>
class UnevaluatedPaintProperty {
public:
    UnevaluatedPaintProperty() = default;

    UnevaluatedPaintProperty(Value value_,
                             UnevaluatedPaintProperty<Value> prior_,
                             TransitionOptions transition,
                             TimePoint now)
        : begin(now + transition.delay.value_or(Duration::zero())),
          end(begin + transition.duration.value_or(Duration::zero())),
          value(std::move(value_)) {
        if (transition.isDefined()) {
            prior = { std::move(prior_) };
        }
    }

    template <class Evaluator>
    auto evaluate(const Evaluator& evaluator, TimePoint now) {
        auto finalValue = value.evaluate(evaluator);
        if (!prior) {
            // No prior value.
            return finalValue;
        } else if (now >= end) {
            // Transition from prior value is now complete.
            prior = {};
            return finalValue;
        } else if (value.isDataDriven()) {
            // Transitions to data-driven properties are not supported.
            // We snap immediately to the data-driven value so that, when we perform layout,
            // we see the data-driven function and can use it to populate vertex buffers.
            prior = {};
            return finalValue;
        } else if (now < begin) {
            // Transition hasn't started yet.
            return prior->get().evaluate(evaluator, now);
        } else {
            // Interpolate between recursively-calculated prior value and final.
            float t = std::chrono::duration<float>(now - begin) / (end - begin);
            return util::interpolate(prior->get().evaluate(evaluator, now), finalValue, util::DEFAULT_TRANSITION_EASE.solve(t, 0.001));
        }
    }

    bool hasTransition() const {
        return bool(prior);
    }

    bool isUndefined() const {
        return value.isUndefined();
    }

    const Value& getValue() const {
        return value;
    }

private:
    optional<mapbox::util::recursive_wrapper<UnevaluatedPaintProperty<Value>>> prior;
    TimePoint begin;
    TimePoint end;
    Value value;
};

template <class Value>
class CascadingPaintProperty {
public:
    bool isUndefined() const {
        return values.find(ClassID::Default) == values.end();
    }

    const Value& get(const optional<std::string>& klass) const {
        static const Value staticValue{};
        const auto it = values.find(klass ? ClassDictionary::Get().lookup(*klass) : ClassID::Default);
        return it == values.end() ? staticValue : it->second;
    }

    void set(const Value& value_, const optional<std::string>& klass) {
        values[klass ? ClassDictionary::Get().lookup(*klass) : ClassID::Default] = value_;
    }

    const TransitionOptions& getTransition(const optional<std::string>& klass) const {
        static const TransitionOptions staticValue{};
        const auto it = transitions.find(klass ? ClassDictionary::Get().lookup(*klass) : ClassID::Default);
        return it == transitions.end() ? staticValue : it->second;
    }

    void setTransition(const TransitionOptions& transition, const optional<std::string>& klass) {
        transitions[klass ? ClassDictionary::Get().lookup(*klass) : ClassID::Default] = transition;
    }

    template <class UnevaluatedPaintProperty>
    UnevaluatedPaintProperty cascade(const CascadeParameters& params, UnevaluatedPaintProperty prior) const {
        TransitionOptions transition;
        Value value;

        for (const auto classID : params.classes) {
            if (values.find(classID) != values.end()) {
                value = values.at(classID);
                break;
            }
        }

        for (const auto classID : params.classes) {
            if (transitions.find(classID) != transitions.end()) {
                transition = transitions.at(classID).reverseMerge(transition);
                break;
            }
        }

        return UnevaluatedPaintProperty(std::move(value),
                                        std::move(prior),
                                        transition.reverseMerge(params.transition),
                                        params.now);
    }

private:
    std::map<ClassID, Value> values;
    std::map<ClassID, TransitionOptions> transitions;
};

template <class T>
class PaintProperty {
public:
    using ValueType = PropertyValue<T>;
    using CascadingType = CascadingPaintProperty<ValueType>;
    using UnevaluatedType = UnevaluatedPaintProperty<ValueType>;
    using EvaluatorType = PropertyEvaluator<T>;
    using EvaluatedType = T;
    static constexpr bool IsDataDriven = false;
};

template <class T, class A>
class DataDrivenPaintProperty {
public:
    using ValueType = DataDrivenPropertyValue<T>;
    using CascadingType = CascadingPaintProperty<ValueType>;
    using UnevaluatedType = UnevaluatedPaintProperty<ValueType>;
    using EvaluatorType = DataDrivenPropertyEvaluator<T>;
    using EvaluatedType = PossiblyEvaluatedPropertyValue<T>;
    static constexpr bool IsDataDriven = true;

    using Type = T;
    using Attribute = A;
};

template <class T>
class CrossFadedPaintProperty {
public:
    using ValueType = PropertyValue<T>;
    using CascadingType = CascadingPaintProperty<ValueType>;
    using UnevaluatedType = UnevaluatedPaintProperty<ValueType>;
    using EvaluatorType = CrossFadedPropertyEvaluator<T>;
    using EvaluatedType = Faded<T>;
    static constexpr bool IsDataDriven = false;
};

template <class P>
struct IsDataDriven : std::integral_constant<bool, P::IsDataDriven> {};

template <class... Ps>
class PaintProperties {
public:
    using Properties = TypeList<Ps...>;
    using DataDrivenProperties = FilteredTypeList<Properties, IsDataDriven>;
    using Binders = PaintPropertyBinders<DataDrivenProperties>;

    using EvaluatedTypes = TypeList<typename Ps::EvaluatedType...>;
    using UnevaluatedTypes = TypeList<typename Ps::UnevaluatedType...>;
    using CascadingTypes = TypeList<typename Ps::CascadingType...>;

    template <class TypeList>
    using Tuple = IndexedTuple<Properties, TypeList>;

    class Evaluated : public Tuple<EvaluatedTypes> {
    public:
        using Tuple<EvaluatedTypes>::Tuple;
    };

    class Unevaluated : public Tuple<UnevaluatedTypes> {
    public:
        using Tuple<UnevaluatedTypes>::Tuple;

        bool hasTransition() const {
            bool result = false;
            util::ignore({ result |= this->template get<Ps>().hasTransition()... });
            return result;
        }

        template <class P>
        auto evaluate(const PropertyEvaluationParameters& parameters) {
            using Evaluator = typename P::EvaluatorType;

            return this->template get<P>().evaluate(
                    Evaluator(parameters, P::defaultValue()),
                    parameters.now
            );
        }

        Evaluated evaluate(const PropertyEvaluationParameters& parameters) {
            return Evaluated {
                evaluate<Ps>(parameters)...
            };
        }

    };

    class Cascading : public Tuple<CascadingTypes> {
    public:
        using Tuple<CascadingTypes>::Tuple;

        Unevaluated cascade(const CascadeParameters& parameters, Unevaluated&& prior) const {
            return Unevaluated {
                this->template get<Ps>().cascade(
                        parameters,
                        std::move(prior.template get<Ps>())
                )...
            };
        }
    };
};

} // namespace style
} // namespace mbgl
