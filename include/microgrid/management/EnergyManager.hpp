#pragma once

#include "microgrid/core/Types.hpp"
#include "microgrid/storage/Battery.hpp"
#include <string>
#include <sstream>
#include <iomanip>

namespace microgrid {

// Deterministic priority-based energy manager with transparent decisions.
// Fallback is pure rule-based; forecasts only bias battery setpoints.
class EnergyManager {
public:
    struct Decision {
        Power_kW battery_setpoint_kw = 0.0; // +charge, -discharge
        Power_kW served_critical_kw  = 0.0;
        Power_kW served_flexible_kw  = 0.0;
        Power_kW curtailed_kw        = 0.0;
        Power_kW unserved_kw         = 0.0;
        bool     shortage            = false;
        bool     excess              = false;
        std::string reason;
    };

    Decision decide(const SystemState& st,
                    Battery& battery,
                    Time_h dt_h,
                    Power_kW forecast_net = 0.0) const {
        Decision d;
        std::ostringstream why;

        Power_kW gen = st.total_gen_kw;
        Power_kW crit = st.critical_kw;
        Power_kW flex = st.flexible_kw;
        Power_kW demand = crit + flex;

        // Net before battery
        Power_kW net = gen - demand;

        // Forecast bias: if we expect shortage soon, prefer keeping SOC higher
        double soc = battery.soc();
        double target_soc = 0.55;
        if (forecast_net < -5.0) target_soc = 0.70; // expect deficit
        if (forecast_net > 10.0) target_soc = 0.40; // expect surplus

        Power_kW batt_cmd = 0.0;

        if (net >= 0.0) {
            // Surplus: charge battery toward target, then curtail
            if (soc < target_soc && battery.can_charge()) {
                Power_kW room = (target_soc - soc) * battery.capacity() / dt_h;
                batt_cmd = std::min(net, std::min(battery.params().max_charge_kw, room));
            }
            Power_kW after_charge = net - std::max(0.0, batt_cmd);
            d.curtailed_kw = std::max(0.0, after_charge);
            d.excess = d.curtailed_kw > kEpsilonPower;
            d.served_critical_kw = crit;
            d.served_flexible_kw = flex;
            why << "Surplus " << std::fixed << std::setprecision(1) << net
                << " kW. Charge batt to target SOC=" << target_soc
                << " (cmd=" << batt_cmd << " kW). Curtail " << d.curtailed_kw << " kW.";
        } else {
            // Deficit: discharge battery, then shed flexible, protect critical
            Power_kW deficit = -net;
            Power_kW discharge = 0.0;
            if (battery.can_discharge()) {
                Power_kW max_d = battery.params().max_discharge_kw;
                Power_kW from_soc = (soc - battery.params().min_soc) * battery.capacity() / dt_h
                                    * battery.params().discharge_eff;
                discharge = std::min(deficit, std::min(max_d, std::max(0.0, from_soc)));
                batt_cmd = -discharge;
            }
            Power_kW remaining = deficit - discharge;

            if (remaining <= kEpsilonPower) {
                d.served_critical_kw = crit;
                d.served_flexible_kw = flex;
                why << "Deficit " << deficit << " kW covered by battery discharge "
                    << discharge << " kW.";
            } else {
                // Shed flexible first
                if (remaining <= flex) {
                    d.served_flexible_kw = flex - remaining;
                    d.served_critical_kw = crit;
                    d.unserved_kw = remaining;
                    why << "Deficit " << deficit << " kW. Battery " << discharge
                        << " kW. Shed flexible " << remaining << " kW. Critical fully served.";
                } else {
                    d.served_flexible_kw = 0.0;
                    remaining -= flex;
                    d.served_critical_kw = std::max(0.0, crit - remaining);
                    d.unserved_kw = flex + (crit - d.served_critical_kw);
                    d.shortage = true;
                    why << "SEVERE shortage. Battery " << discharge
                        << " kW. Flexible shed fully. Critical served "
                        << d.served_critical_kw << " of " << crit << " kW.";
                }
            }
        }

        d.battery_setpoint_kw = batt_cmd;
        d.reason = why.str();
        return d;
    }
};

} // namespace microgrid
