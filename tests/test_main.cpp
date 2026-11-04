#include "microgrid/core/Types.hpp"
#include "microgrid/storage/Battery.hpp"
#include "microgrid/generation/Generation.hpp"
#include "microgrid/demand/Demand.hpp"
#include "microgrid/forecast/Forecaster.hpp"
#include "microgrid/management/EnergyManager.hpp"
#include "microgrid/core/SimulationEngine.hpp"

#include <iostream>
#include <cmath>
#include <cassert>
#include <vector>

using namespace microgrid;

static int g_failed = 0;
static int g_passed = 0;

#define CHECK(cond, msg) \
    do { \
        if (cond) { ++g_passed; } \
        else { \
            ++g_failed; \
            std::cerr << "FAIL: " << msg << " (" << __FILE__ << ":" << __LINE__ << ")\n"; \
        } \
    } while(0)

#define CHECK_NEAR(a, b, tol, msg) CHECK(std::abs((a)-(b)) < (tol), msg)

void test_battery_charge_discharge() {
    Battery::Params p;
    p.capacity_kwh = 100.0;
    p.initial_energy = 50.0;
    p.max_charge_kw = 40.0;
    p.max_discharge_kw = 40.0;
    p.charge_eff = 0.95;
    p.discharge_eff = 0.95;
    p.min_soc = 0.10;
    p.max_soc = 0.95;
    Battery b(p);

    CHECK_NEAR(b.soc(), 0.5, 1e-6, "initial SOC");

    // Charge 20 kW for 1 h
    Power_kW actual = b.apply_power(20.0, 1.0);
    CHECK(actual > 0.0, "charging power positive");
    CHECK(b.soc() > 0.5, "SOC increased after charge");

    // Discharge
    double soc_before = b.soc();
    actual = b.apply_power(-30.0, 1.0);
    CHECK(actual < 0.0, "discharging power negative");
    CHECK(b.soc() < soc_before, "SOC decreased after discharge");

    // Cannot go below min SOC
    b.reset(p.min_soc * p.capacity_kwh + 0.1);
    actual = b.apply_power(-100.0, 1.0);
    CHECK(b.soc() >= p.min_soc - 1e-4, "respects min SOC");
}

void test_generation_patterns() {
    TimeConfig tc;
    tc.step_hours = 0.25;

    SolarPV solar(30.0, 123);
    // Night should be ~0
    Power_kW night = solar.available_power(0, tc); // hour 0
    CHECK(night < 1.0, "solar near zero at night");

    // Midday approx step for hour 12
    Timestamp midday = static_cast<Timestamp>(12.0 / tc.step_hours);
    Power_kW day = solar.available_power(midday, tc);
    CHECK(day > 5.0, "solar produces at midday");

    Microreactor r(50.0);
    CHECK_NEAR(r.available_power(0, tc), 50.0, 1e-6, "reactor baseload");
    r.set_outage(true);
    CHECK_NEAR(r.available_power(0, tc), 0.0, 1e-6, "reactor outage");
}

void test_demand_positive() {
    DemandModel d;
    TimeConfig tc;
    Power_kW res, ind, crit, flex;
    d.compute(0, tc, res, ind, crit, flex);
    CHECK(res >= 0.0 && ind >= 0.0, "demand non-negative");
    CHECK_NEAR(crit + flex, res + ind, 1e-6, "critical+flexible == total");
}

void test_forecaster_updates() {
    BaselineForecaster f;
    f.observe_demand(40.0);
    f.observe_demand(42.0);
    Power_kW fc = f.forecast_demand(1, TimeConfig{});
    CHECK(fc > 0.0, "forecast positive after observations");
}

void test_engine_runs_and_conserves() {
    SimConfig cfg;
    cfg.time.total_steps = 48; // 12 hours
    cfg.seed = 7;
    cfg.enable_logging = false;

    SimulationEngine eng(cfg);
    auto result = eng.run(48);

    CHECK(result.completed, "run completed");
    CHECK(result.history.size() == 48u, "correct history length");
    CHECK(result.total_gen_kwh > 0.0, "produced energy");
    CHECK(result.total_demand_kwh > 0.0, "consumed energy");

    // Battery SOC always in bounds
    for (const auto& s : result.history) {
        CHECK(s.battery_soc >= 0.09 && s.battery_soc <= 0.96, "SOC within limits");
        CHECK(s.total_gen_kw >= 0.0, "gen non-negative");
        CHECK(s.total_demand_kw >= 0.0, "demand non-negative");
    }
}

void test_reproducibility() {
    SimConfig cfg;
    cfg.time.total_steps = 24;
    cfg.seed = 99;
    cfg.enable_logging = false;

    SimulationEngine e1(cfg);
    auto r1 = e1.run(24);

    SimulationEngine e2(cfg);
    auto r2 = e2.run(24);

    CHECK(r1.history.size() == r2.history.size(), "same length");
    bool same = true;
    for (size_t i = 0; i < r1.history.size(); ++i) {
        if (std::abs(r1.history[i].total_gen_kw - r2.history[i].total_gen_kw) > 1e-9 ||
            std::abs(r1.history[i].total_demand_kw - r2.history[i].total_demand_kw) > 1e-9) {
            same = false;
            break;
        }
    }
    CHECK(same, "identical results with same seed");
}

int main() {
    std::cout << "Running microgrid unit tests...\n";
    test_battery_charge_discharge();
    test_generation_patterns();
    test_demand_positive();
    test_forecaster_updates();
    test_engine_runs_and_conserves();
    test_reproducibility();

    std::cout << "Passed: " << g_passed << "  Failed: " << g_failed << "\n";
    return g_failed > 0 ? 1 : 0;
}
