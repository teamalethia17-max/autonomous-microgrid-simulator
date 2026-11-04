#pragma once

#include "microgrid/core/Types.hpp"
#include <deque>
#include <numeric>
#include <cmath>
#include <vector>

namespace microgrid {

// Transparent baseline forecaster: exponential moving average + simple diurnal template
// No black-box ML claims. Fully deterministic given history + seed.
class BaselineForecaster {
public:
    struct Params {
        int    history_window = 96;      // one day of 15-min steps
        double ema_alpha      = 0.25;    // smoothing
        double noise_std      = 0.08;    // residual error model
    };

    BaselineForecaster() : p_() {}
    explicit BaselineForecaster(const Params& p) : p_(p) {}

    void observe_demand(Power_kW actual) {
        demand_hist_.push_back(actual);
        if (static_cast<int>(demand_hist_.size()) > p_.history_window)
            demand_hist_.pop_front();
        if (demand_ema_ < 0) demand_ema_ = actual;
        else demand_ema_ = p_.ema_alpha * actual + (1.0 - p_.ema_alpha) * demand_ema_;
    }

    void observe_solar(Power_kW actual) {
        solar_hist_.push_back(actual);
        if (static_cast<int>(solar_hist_.size()) > p_.history_window)
            solar_hist_.pop_front();
        if (solar_ema_ < 0) solar_ema_ = actual;
        else solar_ema_ = p_.ema_alpha * actual + (1.0 - p_.ema_alpha) * solar_ema_;
    }

    void observe_wind(Power_kW actual) {
        wind_hist_.push_back(actual);
        if (static_cast<int>(wind_hist_.size()) > p_.history_window)
            wind_hist_.pop_front();
        if (wind_ema_ < 0) wind_ema_ = actual;
        else wind_ema_ = p_.ema_alpha * actual + (1.0 - p_.ema_alpha) * wind_ema_;
    }

    // Forecast next step (very short horizon, transparent)
    Power_kW forecast_demand(Timestamp /*next_step*/, const TimeConfig& /*tc*/) const {
        if (demand_hist_.empty()) return 30.0; // prior
        // EMA + slight persistence
        return demand_ema_;
    }

    Power_kW forecast_solar(Timestamp next_step, const TimeConfig& tc) const {
        // Use same clear-sky shape scaled by recent EMA
        Time_h h = hour_of_day(next_step, tc);
        double shape = 0.0;
        if (h >= 6.0 && h <= 18.0) {
            double x = (h - 12.0) / 6.0;
            shape = std::max(0.0, 1.0 - x * x);
        }
        double scale = (solar_ema_ > 0) ? solar_ema_ / 0.45 : 20.0; // rough
        return scale * shape * 0.9; // slight under-forecast bias typical of day-ahead
    }

    Power_kW forecast_wind(Timestamp /*next_step*/, const TimeConfig& /*tc*/) const {
        if (wind_hist_.empty()) return 7.0;
        return wind_ema_ * 0.95;
    }

    double last_demand_error() const { return last_demand_err_; }
    double last_solar_error() const  { return last_solar_err_; }

    void record_errors(Power_kW actual_demand, Power_kW actual_solar,
                       Power_kW fc_demand, Power_kW fc_solar) {
        last_demand_err_ = (actual_demand > kEpsilonPower)
            ? (fc_demand - actual_demand) / actual_demand : 0.0;
        last_solar_err_ = (actual_solar > kEpsilonPower)
            ? (fc_solar - actual_solar) / actual_solar : 0.0;
    }

    void reset() {
        demand_hist_.clear();
        solar_hist_.clear();
        wind_hist_.clear();
        demand_ema_ = -1.0;
        solar_ema_  = -1.0;
        wind_ema_   = -1.0;
        last_demand_err_ = 0.0;
        last_solar_err_  = 0.0;
    }

private:
    Params p_;
    std::deque<Power_kW> demand_hist_;
    std::deque<Power_kW> solar_hist_;
    std::deque<Power_kW> wind_hist_;
    double demand_ema_ = -1.0;
    double solar_ema_  = -1.0;
    double wind_ema_   = -1.0;
    double last_demand_err_ = 0.0;
    double last_solar_err_  = 0.0;
};

} // namespace microgrid
