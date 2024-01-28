#include "Simulation.h"

#include <spdlog/fmt/chrono.h>

#include <chrono>
#include <filesystem>
#include <iostream>
#include <tuple>


#include "integration/IntegrationMethods.h"
#include "io/csv/CSVWriter.h"
#include "io/logger/Logger.h"
#include "particles/containers/directsum/DirectSumContainer.h"
#include "particles/containers/linkedcells/LinkedCellsContainer.h"
#include "simulation/SimulationParams.h"
#include "simulation/interceptors/SimulationInterceptor.h"
#include "simulation/interceptors/frame_writer/FrameWriterInterceptor.h"
#include "simulation/interceptors/particle_update_counter/ParticleUpdateCounterInterceptor.h"
#include "simulation/interceptors/progress_bar/ProgressBarInterceptor.h"
#include "simulation/interceptors/radial_distribution_function/RadialDistributionFunctionInterceptor.h"
#include "simulation/interceptors/thermostat/ThermostatInterceptor.h"

Simulation::Simulation(const std::vector<Particle> &initial_particles, const SimulationParams &params,
                       IntegrationMethod integration_method)
        : params(params), integration_functor(get_integration_functor(integration_method)) {
    // std::cout << "begin simulation constructor" << std::endl;
    // Create particle container
    strategy = 2;
    if (std::holds_alternative<SimulationParams::LinkedCellsType>(params.container_type)) {
        auto lc_type = std::get<SimulationParams::LinkedCellsType>(params.container_type);
        if (strategy == 1 || strategy == 2) {
            linkedCellsContainer = std::make_unique<LinkedCellsContainer>(lc_type.domain_size, lc_type.cutoff_radius,lc_type.boundary_conditions);
            linkedCellsContainer->reserve(initial_particles.size());
            for (auto &particle: initial_particles) {
                linkedCellsContainer->addParticle(particle);
                initial_pos_of_particles.push_back(particle.getX());
            }
        }
        particle_container =
                std::make_unique<LinkedCellsContainer>(lc_type.domain_size, lc_type.cutoff_radius,
                                                       lc_type.boundary_conditions);

    } else if (std::holds_alternative<SimulationParams::DirectSumType>(params.container_type)) {
        particle_container = std::make_unique<DirectSumContainer>();
    } else {
        throw std::runtime_error("Unknown container type");
    }

    // Add particles to container
    particle_container->reserve(initial_particles.size());
    for (auto &particle: initial_particles) {
        particle_container->addParticle(particle);
        initial_pos_of_particles.push_back(particle.getX());
    }
    // std::cout << "end simulation constructor" << std::endl;
}

Simulation::~Simulation() = default;

SimulationOverview Simulation::runSimulation() {
    size_t iteration = 0;
    double simulated_time = 0;

    if (strategy == 1 || strategy == 2) {
        linkedCellsContainer->prepareForceCalculation();
        linkedCellsContainer->applySimpleForces(params.simple_forces);
        linkedCellsContainer->applyPairwiseForces(params.pairwise_forces);
    }

    // Calculate initial forces  //TODO: has also to be done for the parallelization strategies
    else {
        particle_container->prepareForceCalculation();
        particle_container->applySimpleForces(params.simple_forces);
        particle_container->applyPairwiseForces(params.pairwise_forces);
    }
    // params.simple_forces.at(0).

    Logger::logger->info("Simulation started...");

    std::time_t t_start_helper = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    Logger::logger->info("Start time: {}", fmt::format("{:%A %Y-%m-%d %H:%M:%S}", fmt::localtime(t_start_helper)));

    // Notify interceptors that the simulation is about to start
    for (auto &interceptor: params.interceptors) {
        (*interceptor).onSimulationStart(*this);
    }

    auto t_start = std::chrono::high_resolution_clock::now();

    std::unique_ptr<VerletFunctor> verletFunctor = std::make_unique<VerletFunctor>();

    // get the gravity variable so that it can be passed to the parallel verletFunctor step

    double gravityConstant;
    if (!params.simple_forces.empty()) {
        gravityConstant = params.simple_forces.at(0)->getGravityConstant();
    }
    ///////////////////////////////////////// here almost all the computing work is done /////////////////////////////////////////////////////////////////////////////

#pragma omp parallel
    {
        omp_set_num_threads(8);
    }

    while (simulated_time < params.end_time) {
        if (strategy == 1) {
            verletFunctor->parallel_step(linkedCellsContainer, params.simple_forces, params.pairwise_forces, params.delta_t, gravityConstant, 1);
        }
        else if (strategy == 2){
            verletFunctor->parallel_step(linkedCellsContainer, params.simple_forces, params.pairwise_forces, params.delta_t, gravityConstant, 2);
        }
        else if (strategy == 3) {
            verletFunctor->parallel_step(linkedCellsContainer, params.simple_forces, params.pairwise_forces, params.delta_t, gravityConstant, 3);
        }
        else {
            integration_functor->step(particle_container, params.simple_forces, params.pairwise_forces, params.delta_t);
        }
        ++iteration;
        simulated_time += params.delta_t;

        //TODO: maybe also parallelize this

        // Notify interceptors of the current iteration
        for (auto &interceptor: params.interceptors) {
            (*interceptor).notify(iteration, *this);
        }
    }


    //////////////////////////////////////////////// end of the most computing work //////////////////////////////////////////////////////////////////////////////////

    auto t_end = std::chrono::high_resolution_clock::now();

    // Notify interceptors that the simulation has ended
    for (auto &interceptor: params.interceptors) {
        (*interceptor).onSimulationEnd(iteration, *this);
    }

    Logger::logger->info("Simulation finished.");
    Logger::logger->info("End time: {}", fmt::format("{:%A %Y-%m-%d %H:%M:%S}", fmt::localtime(t_end)));

    std::vector<std::string> interceptor_summaries;
    for (auto &interceptor: params.interceptors) {
        auto summary = std::string(*interceptor);
        if (!summary.empty()) {
            interceptor_summaries.push_back(summary);
        }
    }

    auto total_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(t_end - t_start).count();

    SimulationOverview overview{params, total_time_ms / 1000.0, iteration, interceptor_summaries,
                                std::vector<Particle>(particle_container->begin(), particle_container->end())};

    if (params.performance_test) {
        savePerformanceTest(overview, params);
    }

    return overview;
}

void Simulation::savePerformanceTest(const SimulationOverview &overview, const SimulationParams &params) {
    // write the results to the file
    std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    auto formatted_time = fmt::format("{:%d.%m.%Y-%H:%M:%S}", fmt::localtime(now));

    CSVWriter csv_writer(
            params.output_dir_path / ("performance_test_" + formatted_time + ".csv"),
            {"num_particles", "particle_container", "delta_t", "total_time[s]", "particle_updates_per_second[1/s]",
             "total_iterations"});

    // find ParticleUpdateCounterInterceptor
    auto particle_update_counter = std::find_if(params.interceptors.begin(), params.interceptors.end(),
                                                [](auto &interceptor) {
                                                    return std::dynamic_pointer_cast<ParticleUpdateCounterInterceptor>(
                                                            interceptor) != nullptr;
                                                });

    auto particle_updates_per_second =
            particle_update_counter != params.interceptors.end()
            ? std::dynamic_pointer_cast<ParticleUpdateCounterInterceptor>(
                    *particle_update_counter)->getParticleUpdatesPerSecond()
            : -1;

    std::string container_type_string = std::visit([](auto &&arg) { return std::string(arg); }, params.container_type);

    csv_writer.writeRow({params.num_particles, container_type_string, params.delta_t, overview.total_time_seconds,
                         particle_updates_per_second, overview.total_iterations});
}


//bool Simulation::getIsStrategy1() {
//    return is_strategy_1;
//}
//
//void Simulation::setStrategy1(bool to_use) {
//    is_strategy_1 = to_use;
//}
