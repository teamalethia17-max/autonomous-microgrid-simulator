#pragma once

#include "microgrid/core/Types.hpp"
#include <algorithm>
#include <string>

namespace microgrid {

class Battery {
public:
    struct Params {
        Energy_kWh capacity_kwh     = 100.0;
        Power_kW   max_charge_kw    = 40.0;
        Power_kW   max_discharge_kw = 40.0;
        Efficiency charge_eff       = 0.95;
        Efficiency discharge_eff    = 0.95;
        Soc        min_soc          = 0.10;
        Soc        max_soc          = 0.95;
        Energy_kWh initial_energy   = 50.0;
    };

    Battery() : p_(), energy_(clamp(p_.initial_energy, p_.min_soc * p_.capacity_kwh,
                                    p_.max_soc * p_.capacity_kwh)) {}
    explicit Battery(const Params& p)
        : p_(p), energy_(clamp(p.initial_energy, p.min_soc * p.capacity_kwh,
                               p.max_soc * p.capacity_kwh)) {}

    // Attempt to charge/discharge. Positive power = charge into battery.
    // Returns actual power applied to the DC bus (positive = absorbed by battery).
    Power_kW apply_power(Power_kW desired_kw, Time_h dt_h) {
        if (dt_h <= 0.0) return 0.0;

        Power_kW actual = 0.0;

        if (desired_kw > kEpsilonPower) {
            // Charging
            Power_kW max_p = p_.max_charge_kw;
            Energy_kWh room = (p_.max_soc * p_.capacity_kwh) - energy_;
            Power_kW room_kw = room / dt_h / p_.charge_eff; // account for losses
            actual = std::min({desired_kw, max_p, std::max(0.0, room_kw)});
            Energy_kWh delta = actual * dt_h * p_.charge_eff;
            energy_ += delta;
        } else if (desired_kw < -kEpsilonPower) {
            // Discharging (desired_kw negative)
            Power_kW max_p = p_.max_discharge_kw;
            Energy_kWh available = energy_ - (p_.min_soc * p_.capacity_kwh);
            Power_kW avail_kw = available / dt_h * p_.discharge_eff;
            actual = -std::min({-desired_kw, max_p, std::max(0.0, avail_kw)});
            Energy_kWh delta = (-actual) * dt_h / p_.discharge_eff; // energy leaving battery
            energy_ -= delta;
        }

        energy_ = clamp(energy_, 0.0, p_.capacity_kwh);
        last_power_ = actual;
        return actual;
    }

    Soc soc() const {
        return energy_ / p_.capacity_kwh;
    }

    Energy_kWh energy() const { return energy_; }
    Energy_kWh capacity() const { return p_.capacity_kwh; }
    Power_kW last_power() const { return last_power_; }
    bool can_charge() const { return soc() < p_.max_soc - 1e-4; }
    bool can_discharge() const { return soc() > p_.min_soc + 1e-4; }

    const Params& params() const { return p_; }

    void reset(Energy_kWh e = -1.0) {
        if (e < 0) e = p_.initial_energy;
        energy_ = clamp(e, p_.min_soc * p_.capacity_kwh, p_.max_soc * p_.capacity_kwh);
        last_power_ = 0.0;
    }

private:
    Params p_;
    Energy_kWh energy_;
    Power_kW last_power_ = 0.0;
};

} // namespace microgrid
