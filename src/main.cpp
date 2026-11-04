#include "microgrid/core/SimulationEngine.hpp"
#include "microgrid/ui/ConsoleDashboard.hpp"
#include "microgrid/core/Logger.hpp"

#include <iostream>
#include <string>
#include <cstdlib>

using namespace microgrid;

void print_usage(const char* prog) {
    std::cout << "Usage: " << prog << " [options]\n"
              << "  --steps N          Number of simulation steps (default 672 = 7 days @ 15min)\n"
              << "  --seed S           RNG seed for reproducibility (default 42)\n"
              << "  --outage-reactor   Force microreactor outage for whole run\n"
              << "  --csv PATH         Export metrics CSV (default metrics.csv)\n"
              << "  --quiet            Less console output\n"
              << "  --help             Show this help\n";
}

int main(int argc, char** argv) {
    int steps = 96 * 7; // 7 days
    unsigned seed = 42;
    bool reactor_outage = false;
    bool quiet = false;
    std::string csv_path = "metrics.csv";

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help") { print_usage(argv[0]); return 0; }
        else if (arg == "--steps" && i+1 < argc) steps = std::atoi(argv[++i]);
        else if (arg == "--seed" && i+1 < argc) seed = static_cast<unsigned>(std::atoi(argv[++i]));
        else if (arg == "--outage-reactor") reactor_outage = true;
        else if (arg == "--csv" && i+1 < argc) csv_path = argv[++i];
        else if (arg == "--quiet") quiet = true;
        else {
            std::cerr << "Unknown argument: " << arg << "\n";
            print_usage(argv[0]);
            return 1;
        }
    }

    ConsoleDashboard::print_header();

    SimConfig cfg;
    cfg.time.step_hours = 0.25;
    cfg.time.total_steps = steps;
    cfg.seed = seed;
    cfg.enable_logging = !quiet;
    cfg.csv_path = csv_path;
    cfg.log_path = "simulation.log";

    SimulationEngine engine(cfg);

    if (reactor_outage) {
        engine.set_reactor_outage(true);
        LOG_WARN("Scenario: Microreactor forced offline for entire run");
    }

    // Live stepping with occasional prints
    engine.reset();
    const int print_every = quiet ? steps + 1 : std::max(1, steps / 12);

    for (int i = 0; i < steps; ++i) {
        auto st = engine.step();
        if (!quiet && (i % print_every == 0 || i == steps-1)) {
            ConsoleDashboard::print_state(st);
        }
    }

    // Build result summary from history
    SimulationResult result;
    result.history = engine.history();
    result.completed = true;

    Energy_kWh unserved = 0, curtailed = 0, gen = 0, dem = 0;
    double err_sum = 0;
    int err_n = 0;
    double dt = cfg.time.step_hours;
    for (const auto& s : result.history) {
        unserved  += s.unserved_kw * dt;
        curtailed  += s.curtailed_kw * dt;
        gen       += s.total_gen_kw * dt;
        dem       += s.total_demand_kw * dt;
        if (std::abs(s.demand_forecast_error) > 1e-6) {
            err_sum += std::abs(s.demand_forecast_error);
            ++err_n;
        }
    }
    result.total_unserved_kwh = unserved;
    result.total_curtailed_kwh = curtailed;
    result.total_gen_kwh = gen;
    result.total_demand_kwh = dem;
    result.avg_demand_error = err_n ? err_sum / err_n : 0.0;

    std::ostringstream sum;
    sum << std::fixed << std::setprecision(2)
        << "Total generation : " << gen << " kWh\n"
        << "Total demand     : " << dem << " kWh\n"
        << "Unserved energy  : " << unserved << " kWh\n"
        << "Curtailed energy : " << curtailed << " kWh\n"
        << "Avg |demand fc error| : " << (result.avg_demand_error*100) << " %\n"
        << "Final battery SOC: " << (engine.components().battery.soc()*100) << " %\n";
    result.summary = sum.str();

    ConsoleDashboard::print_summary(result);
    if (!quiet) {
        ConsoleDashboard::print_charts(result);
    }

    if (engine.export_csv(csv_path)) {
        std::cout << "Metrics exported to " << csv_path << "\n";
    } else {
        std::cerr << "Failed to write CSV\n";
    }

    std::cout << "\nEducational simulation finished. No real hardware was controlled.\n";
    return 0;
}
