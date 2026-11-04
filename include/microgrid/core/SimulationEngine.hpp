#pragma once

#include "microgrid/core/Types.hpp"
#include "microgrid/core/Logger.hpp"
#include "microgrid/generation/Generation.hpp"
#include "microgrid/storage/Battery.hpp"
#include "microgrid/demand/Demand.hpp"
#include "microgrid/forecast/Forecaster.hpp"
#include "microgrid/management/EnergyManager.hpp"

#include <memory>
#include <vector>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace microgrid {

class SimulationEngine {
public:
    struct Components {
        Microreactor  reactor{50.0};
        SolarPV       solar{30.0};
        WindTurbine   wind{20.0};
        Battery       battery;
        DemandModel   demand;
        BaselineForecaster forecaster;
        EnergyManager manager;
    };

    explicit SimulationEngine(const SimConfig& cfg = SimConfig{})
        : cfg_(cfg), comps_() {
        Logger::instance().configure(cfg.log_path, cfg.enable_logging);
        // Re-seed RNGs for reproducibility
        comps_.solar = SolarPV(30.0, cfg.seed + 1);
        comps_.wind  = WindTurbine(20.0, cfg.seed + 2);
        comps_.demand = DemandModel({15.0, 25.0, 0.35, cfg.seed + 3});
    }

    void reset() {
        comps_.battery.reset();
        comps_.forecaster.reset();
        history_.clear();
        current_step_ = 0;
    }

    // Advance one timestep. Returns the new state.
    SystemState step() {
        SystemState st;
        st.step = current_step_;
        st.hour_of_day = hour_of_day(current_step_, cfg_.time);
        st.day_of_week = day_of_week(current_step_, cfg_.time);

        // 1. Generation
        st.reactor_online  = comps_.reactor.is_online(current_step_);
        st.solar_available = comps_.solar.is_online(current_step_);
        st.wind_available  = comps_.wind.is_online(current_step_);

        st.microreactor_kw = comps_.reactor.available_power(current_step_, cfg_.time);
        st.solar_kw        = comps_.solar.available_power(current_step_, cfg_.time);
        st.wind_kw         = comps_.wind.available_power(current_step_, cfg_.time);
        st.total_gen_kw    = st.microreactor_kw + st.solar_kw + st.wind_kw;

        // 2. Demand
        comps_.demand.compute(current_step_, cfg_.time,
                              st.residential_kw, st.industrial_kw,
                              st.critical_kw, st.flexible_kw);
        st.total_demand_kw = st.residential_kw + st.industrial_kw;

        // 3. Forecasts (for next step, using current observations later)
        st.forecast_demand_kw = comps_.forecaster.forecast_demand(current_step_ + 1, cfg_.time);
        st.forecast_solar_kw  = comps_.forecaster.forecast_solar(current_step_ + 1, cfg_.time);
        st.forecast_wind_kw   = comps_.forecaster.forecast_wind(current_step_ + 1, cfg_.time);

        Power_kW forecast_net = (st.forecast_solar_kw + st.forecast_wind_kw + st.microreactor_kw)
                                - st.forecast_demand_kw;

        // 4. Energy management decision
        auto decision = comps_.manager.decide(st, comps_.battery, cfg_.time.step_hours, forecast_net);

        // 5. Apply battery action
        Power_kW actual_batt = comps_.battery.apply_power(decision.battery_setpoint_kw,
                                                          cfg_.time.step_hours);
        st.battery_power_kw    = actual_batt;
        st.battery_energy_kwh  = comps_.battery.energy();
        st.battery_soc         = comps_.battery.soc();
        st.battery_charging    = actual_batt > kEpsilonPower;
        st.battery_discharging = actual_batt < -kEpsilonPower;

        // 6. Final balance
        st.net_power_kw  = st.total_gen_kw - st.total_demand_kw;
        st.imbalance_kw  = st.net_power_kw - actual_batt; // after battery
        st.curtailed_kw  = decision.curtailed_kw;
        st.unserved_kw   = decision.unserved_kw;
        st.shortage      = decision.shortage;
        st.excess        = decision.excess;
        st.decision_reason = decision.reason;

        // Update forecaster with observations
        comps_.forecaster.observe_demand(st.total_demand_kw);
        comps_.forecaster.observe_solar(st.solar_kw);
        comps_.forecaster.observe_wind(st.wind_kw);
        comps_.forecaster.record_errors(st.total_demand_kw, st.solar_kw,
                                        st.forecast_demand_kw, st.forecast_solar_kw);
        st.demand_forecast_error = comps_.forecaster.last_demand_error();
        st.solar_forecast_error  = comps_.forecaster.last_solar_error();

        history_.push_back(st);
        ++current_step_;
        return st;
    }

    SimulationResult run(int num_steps = -1) {
        if (num_steps < 0) num_steps = cfg_.time.total_steps;
        reset();
        LOG_INFO("Starting simulation for " + std::to_string(num_steps) + " steps");

        SimulationResult result;
        Energy_kWh unserved_acc = 0.0, curtailed_acc = 0.0;
        Energy_kWh gen_acc = 0.0, dem_acc = 0.0;
        double err_acc = 0.0;
        int err_n = 0;

        for (int i = 0; i < num_steps; ++i) {
            auto st = step();
            double dt = cfg_.time.step_hours;
            unserved_acc  += st.unserved_kw * dt;
            curtailed_acc += st.curtailed_kw * dt;
            gen_acc       += st.total_gen_kw * dt;
            dem_acc       += st.total_demand_kw * dt;
            if (std::abs(st.demand_forecast_error) > 0) {
                err_acc += std::abs(st.demand_forecast_error);
                ++err_n;
            }
        }

        result.history = history_;
        result.total_unserved_kwh  = unserved_acc;
        result.total_curtailed_kwh = curtailed_acc;
        result.total_gen_kwh       = gen_acc;
        result.total_demand_kwh    = dem_acc;
        result.avg_demand_error    = (err_n > 0) ? err_acc / err_n : 0.0;
        result.completed = true;

        std::ostringstream sum;
        sum << std::fixed << std::setprecision(2)
            << "Simulation complete.\n"
            << "  Total generation : " << gen_acc << " kWh\n"
            << "  Total demand     : " << dem_acc << " kWh\n"
            << "  Unserved energy  : " << unserved_acc << " kWh\n"
            << "  Curtailed energy : " << curtailed_acc << " kWh\n"
            << "  Avg |demand forecast error| : " << (result.avg_demand_error * 100.0) << " %\n"
            << "  Final battery SOC: " << (comps_.battery.soc() * 100.0) << " %\n";
        result.summary = sum.str();
        LOG_INFO(result.summary);
        return result;
    }

    // Inject simulated outages for scenario testing
    void set_reactor_outage(bool outage) { comps_.reactor.set_outage(outage); }
    void set_solar_outage(bool outage)   { comps_.solar.set_outage(outage); }
    void set_wind_outage(bool outage)    { comps_.wind.set_outage(outage); }

    const std::vector<SystemState>& history() const { return history_; }
    const Components& components() const { return comps_; }
    Components& components() { return comps_; }
    const SimConfig& config() const { return cfg_; }

    // Export metrics to CSV (all values from engine)
    bool export_csv(const std::string& path) const {
        std::ofstream f(path);
        if (!f) return false;
        f << "step,hour,day,reactor_kw,solar_kw,wind_kw,total_gen_kw,"
          << "res_kw,ind_kw,total_demand_kw,critical_kw,flexible_kw,"
          << "batt_energy_kwh,batt_soc,batt_power_kw,"
          << "net_kw,imbalance_kw,curtailed_kw,unserved_kw,shortage,excess,"
          << "fc_demand,fc_solar,demand_err,solar_err,decision\n";
        f << std::fixed << std::setprecision(3);
        for (const auto& s : history_) {
            f << s.step << "," << s.hour_of_day << "," << s.day_of_week << ","
              << s.microreactor_kw << "," << s.solar_kw << "," << s.wind_kw << ","
              << s.total_gen_kw << ","
              << s.residential_kw << "," << s.industrial_kw << "," << s.total_demand_kw << ","
              << s.critical_kw << "," << s.flexible_kw << ","
              << s.battery_energy_kwh << "," << s.battery_soc << "," << s.battery_power_kw << ","
              << s.net_power_kw << "," << s.imbalance_kw << ","
              << s.curtailed_kw << "," << s.unserved_kw << ","
              << (s.shortage?1:0) << "," << (s.excess?1:0) << ","
              << s.forecast_demand_kw << "," << s.forecast_solar_kw << ","
              << s.demand_forecast_error << "," << s.solar_forecast_error << ","
              << "\"" << s.decision_reason << "\"\n";
        }
        return true;
    }

private:
    SimConfig cfg_;
    Components comps_;
    std::vector<SystemState> history_;
    Timestamp current_step_ = 0;
};

} // namespace microgrid
