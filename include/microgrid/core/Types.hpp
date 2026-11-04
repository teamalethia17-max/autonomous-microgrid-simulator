#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <optional>
#include <chrono>
#include <cmath>
#include <stdexcept>

namespace microgrid {

// Physical units (SI-ish, educational)
using Power_kW     = double;   // kilowatts
using Energy_kWh   = double;   // kilowatt-hours
using Time_h       = double;   // hours (fractional allowed)
using Soc          = double;   // state of charge [0,1]
using Efficiency   = double;   // [0,1]
using Timestamp    = int64_t;  // simulation step index (discrete)

constexpr Power_kW  kEpsilonPower  = 1e-6;
constexpr Energy_kWh kEpsilonEnergy = 1e-6;

struct TimeConfig {
    Time_h   step_hours     = 0.25;   // 15-minute steps by default
    int      steps_per_day  = 96;
    int      total_steps    = 96 * 7; // one week default
};

struct SimConfig {
    TimeConfig time;
    unsigned   seed = 42;             // for reproducibility
    bool       enable_logging = true;
    std::string log_path = "simulation.log";
    std::string csv_path = "metrics.csv";
};

// Snapshot of system state at one timestep (all metrics come from engine)
struct SystemState {
    Timestamp   step          = 0;
    Time_h      hour_of_day   = 0.0;  // 0..24
    int         day_of_week   = 0;    // 0=Mon .. 6=Sun

    // Generation
    Power_kW    microreactor_kw = 0.0;
    Power_kW    solar_kw        = 0.0;
    Power_kW    wind_kw         = 0.0;
    Power_kW    total_gen_kw    = 0.0;
    bool        reactor_online  = true;
    bool        solar_available = true;
    bool        wind_available  = true;

    // Demand
    Power_kW    residential_kw  = 0.0;
    Power_kW    industrial_kw   = 0.0;
    Power_kW    total_demand_kw = 0.0;
    Power_kW    critical_kw     = 0.0; // priority loads
    Power_kW    flexible_kw     = 0.0;

    // Storage
    Energy_kWh  battery_energy_kwh = 0.0;
    Soc         battery_soc        = 0.0;
    Power_kW    battery_power_kw   = 0.0; // +charge, -discharge
    bool        battery_charging   = false;
    bool        battery_discharging= false;

    // Balance
    Power_kW    net_power_kw       = 0.0; // gen - demand (before battery)
    Power_kW    imbalance_kw       = 0.0; // after battery action
    bool        shortage           = false;
    bool        excess             = false;
    Power_kW    curtailed_kw       = 0.0;
    Power_kW    unserved_kw        = 0.0;

    // Forecasts (baseline model)
    Power_kW    forecast_demand_kw = 0.0;
    Power_kW    forecast_solar_kw  = 0.0;
    Power_kW    forecast_wind_kw   = 0.0;
    double      demand_forecast_error = 0.0;
    double      solar_forecast_error  = 0.0;

    // Decision explanation (deterministic + AI-assisted)
    std::string decision_reason;
};

struct HistoricalRecord {
    SystemState state;
};

// Simple result for a full run
struct SimulationResult {
    std::vector<SystemState> history;
    double   avg_demand_error   = 0.0;
    double   max_unserved_kwh   = 0.0;
    Energy_kWh total_unserved_kwh = 0.0;
    Energy_kWh total_curtailed_kwh = 0.0;
    Energy_kWh total_gen_kwh    = 0.0;
    Energy_kWh total_demand_kwh = 0.0;
    bool     completed          = false;
    std::string summary;
};

// Utility
inline double clamp(double v, double lo, double hi) {
    return std::max(lo, std::min(hi, v));
}

inline Time_h hour_of_day(Timestamp step, const TimeConfig& tc) {
    double hours = static_cast<double>(step) * tc.step_hours;
    return std::fmod(hours, 24.0);
}

inline int day_of_week(Timestamp step, const TimeConfig& tc) {
    int day = static_cast<int>(static_cast<double>(step) * tc.step_hours / 24.0) % 7;
    return day;
}

} // namespace microgrid
