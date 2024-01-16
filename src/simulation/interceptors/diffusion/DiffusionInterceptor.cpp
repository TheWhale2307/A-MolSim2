#include "DiffusionInterceptor.h"
#include "utils/ArrayUtils.h"
#include "simulation/SimulationParams.h"
DiffusionInterceptor::DiffusionInterceptor() = default;

void DiffusionInterceptor::onSimulationStart(Simulation &simulation) {

    csv_writer = std::make_unique<CSVWriter>(simulation.params.output_dir_path / "diffusion.csv");
    csv_writer->initialize({"iteration","variance"});
    every_nth_iteration = 1000;

    for(auto& pos:simulation.initial_pos_of_particles){
        previous_references.push_back(pos);
    }

}

void DiffusionInterceptor::operator()(size_t iteration, Simulation& simulation) {

    double var = 0;
    auto number_of_particles = static_cast<double>(simulation.particle_container->size());
    double coefficient = 1.0/number_of_particles;

    double sum = 0;
    size_t i = 0;
    for (auto & particle : *simulation.particle_container) {
        std::array<double,3> distance_pos = particle.getDistancePosition();
        double norm = ArrayUtils::L2Norm(distance_pos-previous_references.at(i));
        norm = std::pow(norm,0.5);
        previous_references.at(i) = particle.getX();
        sum+=norm;
        i++;

    }
    var = coefficient*sum;

    csv_writer->writeRow({iteration,var});

}

void DiffusionInterceptor::onSimulationEnd(size_t iteration, Simulation &simulation) {}

DiffusionInterceptor::operator std::string() const {
    return "Diffusion";
}

void DiffusionInterceptor::logSummary(int depth) const {
    std::string indent = std::string(depth * 2, ' ');

    Logger::logger->info("{}╟┤{}Diffusion: {}", indent, ansi_orange_bold, ansi_end);
}

