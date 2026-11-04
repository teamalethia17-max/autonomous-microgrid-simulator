#pragma once

#include "microgrid/core/Types.hpp"
#include <random>
#include <string>

namespace microgrid {

// Abstract generation source
class IGenerationSource {
public:
    virtual ~IGenerationSource() = default;
    virtual Power_kW available_power(Timestamp step, const TimeConfig& tc) const = 0;
    virtual bool is_online(Timestamp step) const = 0;
    virtual std::string name() const = 0;
    virtual void set_outage(bool outage) = 0;
};

// Simulated microreactor: baseload with high availability, rare outages
class Microreactor final : public IGenerationSource {
public:
    explicit Microreactor(Power_kW rated_kw = 50.0, double availability = 0.98)
        : rated_kw_(rated_kw), availability_(availability), online_(true) {}

    Power_kW available_power(Timestamp /*step*/, const TimeConfig& /*tc*/) const override {
        return online_ ? rated_kw_ : 0.0;
    }

    bool is_online(Timestamp /*step*/) const override { return online_; }
    std::string name() const override { return "Microreactor"; }
    void set_outage(bool outage) override { online_ = !outage; }

    Power_kW rated() const { return rated_kw_; }

private:
    Power_kW rated_kw_;
    double   availability_;
    bool     online_;
};

// Solar: diurnal pattern + weather noise
class SolarPV final : public IGenerationSource {
public:
    SolarPV(Power_kW peak_kw = 30.0, unsigned seed = 42)
        : peak_kw_(peak_kw), rng_(seed), online_(true) {}

    Power_kW available_power(Timestamp step, const TimeConfig& tc) const override {
        if (!online_) return 0.0;
        Time_h h = hour_of_day(step, tc);
        // Simple clear-sky model: peak at solar noon ~12:00, zero at night
        double solar_factor = 0.0;
        if (h >= 6.0 && h <= 18.0) {
            double x = (h - 12.0) / 6.0; // -1 .. +1
            solar_factor = std::max(0.0, 1.0 - x * x); // parabolic
        }
        // Weather variability (cloud cover)
        std::uniform_real_distribution<double> weather(0.55, 1.0);
        double w = weather(const_cast<std::mt19937&>(rng_));
        return peak_kw_ * solar_factor * w;
    }

    bool is_online(Timestamp /*step*/) const override { return online_; }
    std::string name() const override { return "SolarPV"; }
    void set_outage(bool outage) override { online_ = !outage; }

private:
    Power_kW peak_kw_;
    mutable std::mt19937 rng_;
    bool online_;
};

// Wind: more stochastic, day/night mild bias
class WindTurbine final : public IGenerationSource {
public:
    WindTurbine(Power_kW rated_kw = 20.0, unsigned seed = 43)
        : rated_kw_(rated_kw), rng_(seed), online_(true) {}

    Power_kW available_power(Timestamp step, const TimeConfig& tc) const override {
        if (!online_) return 0.0;
        // Base capacity factor ~0.35 with variation
        std::normal_distribution<double> dist(0.35, 0.18);
        double cf = clamp(dist(const_cast<std::mt19937&>(rng_)), 0.0, 1.0);
        // Mild diurnal: slightly higher at night
        Time_h h = hour_of_day(step, tc);
        if (h < 6.0 || h > 20.0) cf *= 1.15;
        return rated_kw_ * clamp(cf, 0.0, 1.0);
    }

    bool is_online(Timestamp /*step*/) const override { return online_; }
    std::string name() const override { return "Wind"; }
    void set_outage(bool outage) override { online_ = !outage; }

private:
    Power_kW rated_kw_;
    mutable std::mt19937 rng_;
    bool online_;
};

} // namespace microgrid
