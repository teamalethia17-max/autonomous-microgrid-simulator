#pragma once

#include "microgrid/core/Types.hpp"
#include "microgrid/core/SimulationEngine.hpp"
#include <iostream>
#include <iomanip>
#include <string>
#include <sstream>
#include <vector>
#include <algorithm>

namespace microgrid {

class ConsoleDashboard {
public:
    static void print_header() {
        std::cout << "\n"
                  << "╔══════════════════════════════════════════════════════════════════╗\n"
                  << "║         AUTONOMOUS MICROGRID SIMULATOR  (Educational)            ║\n"
                  << "║   Generation · Storage · Demand · Forecast · Energy Management   ║\n"
                  << "╚══════════════════════════════════════════════════════════════════╝\n\n";
    }

    static void print_state(const SystemState& s) {
        std::cout << std::fixed << std::setprecision(1);
        std::cout << "── Step " << std::setw(4) << s.step
                  << " | Day " << s.day_of_week
                  << " | Hour " << std::setw(5) << s.hour_of_day << " ──\n";

        std::cout << "  Generation : Reactor " << std::setw(6) << s.microreactor_kw
                  << " | Solar " << std::setw(6) << s.solar_kw
                  << " | Wind " << std::setw(6) << s.wind_kw
                  << " | TOTAL " << std::setw(6) << s.total_gen_kw << " kW\n";

        std::cout << "  Demand     : Resid.  " << std::setw(6) << s.residential_kw
                  << " | Indust " << std::setw(6) << s.industrial_kw
                  << " | TOTAL " << std::setw(6) << s.total_demand_kw << " kW\n";

        std::cout << "  Battery    : SOC " << std::setw(5) << (s.battery_soc * 100.0) << "%"
                  << " (" << std::setw(6) << s.battery_energy_kwh << " kWh)"
                  << " | P=" << std::setw(6) << s.battery_power_kw << " kW"
                  << (s.battery_charging ? " [CHG]" : "")
                  << (s.battery_discharging ? " [DIS]" : "") << "\n";

        std::cout << "  Balance    : Net " << std::setw(6) << s.net_power_kw << " kW";
        if (s.shortage) std::cout << "  *** SHORTAGE unserved=" << s.unserved_kw << " kW ***";
        if (s.excess)   std::cout << "  (excess curtailed=" << s.curtailed_kw << " kW)";
        std::cout << "\n";

        std::cout << "  Forecast   : Demand " << s.forecast_demand_kw
                  << " (err " << std::setprecision(1) << (s.demand_forecast_error*100) << "%)"
                  << " | Solar " << s.forecast_solar_kw << "\n";

        std::cout << "  Decision   : " << s.decision_reason << "\n\n";
    }

    static void print_summary(const SimulationResult& r) {
        std::cout << "\n========== SIMULATION SUMMARY ==========\n"
                  << r.summary
                  << "========================================\n";
    }

    // Simple ASCII chart of a series (last N points)
    static void ascii_chart(const std::vector<double>& data, const std::string& title,
                            int width = 60, int height = 10) {
        if (data.empty()) return;
        double mn = *std::min_element(data.begin(), data.end());
        double mx = *std::max_element(data.begin(), data.end());
        if (mx - mn < 1e-6) { mx = mn + 1.0; }

        std::cout << title << " [" << std::fixed << std::setprecision(1)
                  << mn << " .. " << mx << "]\n";

        int n = static_cast<int>(data.size());
        int step = std::max(1, n / width);

        for (int row = height; row >= 0; --row) {
            double thresh = mn + (mx - mn) * row / height;
            std::cout << std::setw(7) << std::setprecision(1) << thresh << " |";
            for (int i = 0; i < n; i += step) {
                char c = (data[i] >= thresh) ? '#' : ' ';
                std::cout << c;
            }
            std::cout << "\n";
        }
        std::cout << "         +" << std::string(width, '-') << "\n\n";
    }

    static void print_charts(const SimulationResult& r) {
        if (r.history.empty()) return;
        std::vector<double> gen, dem, soc, net;
        for (const auto& s : r.history) {
            gen.push_back(s.total_gen_kw);
            dem.push_back(s.total_demand_kw);
            soc.push_back(s.battery_soc * 100.0);
            net.push_back(s.net_power_kw);
        }
        ascii_chart(gen, "Total Generation (kW)");
        ascii_chart(dem, "Total Demand (kW)");
        ascii_chart(soc, "Battery SOC (%)");
        ascii_chart(net, "Net Power (gen-demand) kW");
    }
};

} // namespace microgrid
