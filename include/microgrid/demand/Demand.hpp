#pragma once

#include "microgrid/core/Types.hpp"
#include <random>
#include <cmath>

namespace microgrid {

class DemandModel {
public:
    struct Params {
        Power_kW residential_base = 15.0;
        Power_kW industrial_base  = 25.0;
        Power_kW critical_fraction = 0.35; // of total that is critical
        unsigned seed = 44;
    };

    DemandModel() : p_(), rng_(p_.seed) {}
    explicit DemandModel(const Params& p)
        : p_(p), rng_(p.seed) {}

    // Returns residential, industrial, critical, flexible
    void compute(Timestamp step, const TimeConfig& tc,
                 Power_kW& res, Power_kW& ind,
                 Power_kW& critical, Power_kW& flexible) const {
        Time_h h = hour_of_day(step, tc);
        int dow = day_of_week(step, tc);
        bool weekend = (dow >= 5);

        // Residential profile: morning + evening peaks
        double res_factor = 0.4;
        if (h >= 6.0 && h < 9.0) res_factor = 0.9 + 0.3 * std::sin((h-6)*3.14/3);
        else if (h >= 17.0 && h < 22.0) res_factor = 1.1 + 0.2 * std::sin((h-17)*3.14/5);
        else if (h >= 22.0 || h < 6.0) res_factor = 0.25;
        else res_factor = 0.55;

        if (weekend) res_factor *= 1.15; // higher residential on weekend

        // Industrial: weekday daytime heavy, lower nights/weekends
        double ind_factor = 0.3;
        if (!weekend) {
            if (h >= 7.0 && h < 18.0) ind_factor = 1.0;
            else if (h >= 18.0 && h < 22.0) ind_factor = 0.6;
            else ind_factor = 0.25;
        } else {
            ind_factor = 0.35;
        }

        // Stochastic noise
        std::normal_distribution<double> noise(1.0, 0.06);
        double n = clamp(noise(const_cast<std::mt19937&>(rng_)), 0.8, 1.25);

        // Occasional demand spike (rare)
        std::uniform_real_distribution<double> spike_chance(0.0, 1.0);
        double spike = 1.0;
        if (spike_chance(const_cast<std::mt19937&>(rng_)) < 0.015) {
            spike = 1.4 + 0.3 * spike_chance(const_cast<std::mt19937&>(rng_));
        }

        res = p_.residential_base * res_factor * n * spike;
        ind = p_.industrial_base  * ind_factor * n;

        Power_kW total = res + ind;
        critical = total * p_.critical_fraction;
        flexible = total - critical;
    }

private:
    Params p_;
    mutable std::mt19937 rng_;
};

} // namespace microgrid
