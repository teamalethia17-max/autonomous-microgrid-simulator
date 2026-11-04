# Autonomous Microgrid Simulator

**Educational software-only simulation** of an autonomous electricity microgrid.

It models:

- **Generation**: abstract microreactor (baseload), solar PV (diurnal), wind (stochastic)
- **Storage**: battery with SOC limits, charge/discharge power limits, round-trip efficiency
- **Demand**: residential + industrial profiles with time-of-day and weekday/weekend patterns, occasional spikes
- **AI / Forecasting**: transparent baseline (exponential moving average + clear-sky shape). No black-box claims.
- **Energy Management**: priority-based (critical loads first), battery arbitrage biased by short-horizon forecasts, deterministic explanations for every decision
- **Monitoring**: full state history, CSV export, console dashboard with ASCII charts

> **Safety / Scope**  
> This is purely educational simulation software. It does **not** connect to, control, or interface with any real reactors, nuclear systems, electrical grids, or physical infrastructure.

## Architecture

```
include/microgrid/
  core/          Types, Logger, SimulationEngine
  generation/    Microreactor, SolarPV, WindTurbine
  storage/       Battery
  demand/        DemandModel
  forecast/      BaselineForecaster (transparent)
  management/    EnergyManager (rule-based + forecast bias)
  ui/            ConsoleDashboard
src/main.cpp     CLI entry point
tests/           Self-contained unit tests (no external framework)
```

All numeric metrics shown in the UI or exported to CSV are produced by the simulation engine. There is no hardcoded “live” data.

## Build

Requirements: C++17 compiler, CMake ≥ 3.16.

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

## Run

```bash
./build/microgrid_sim                    # 7-day simulation @ 15-min steps
./build/microgrid_sim --steps 96         # 1 day
./build/microgrid_sim --seed 123         # different stochastic realization
./build/microgrid_sim --outage-reactor   # scenario: reactor offline
./build/microgrid_sim --csv out.csv      # custom metrics file
./build/microgrid_sim --quiet            # summary + charts only
```

## Tests

```bash
./build/microgrid_tests
# or
ctest --test-dir build
```

## Assumptions (documented)

- Discrete time, fixed step (default 15 min).
- Microreactor is an abstract constant-power source with high availability; outages are injected for scenarios only.
- Solar uses a simple parabolic clear-sky model modulated by random cloud factor.
- Wind uses a capacity-factor distribution with mild diurnal bias.
- Battery is a single aggregate pool (no cell-level chemistry).
- Forecast horizon is one step; model is EMA + template (fully inspectable).
- Load shedding priority: flexible → critical. No network power-flow or voltage constraints.
- Seeded RNGs make runs reproducible.

## Extending

- Replace `BaselineForecaster` with any model that implements the same observe/forecast interface.
- Add more generation assets by implementing `IGenerationSource`.
- Scenario scripts can call `set_*_outage()` at arbitrary steps (currently whole-run for simplicity).
- CSV columns are stable; downstream plotting (Python/R/gnuplot) is straightforward.

## License

MIT (educational use encouraged).
