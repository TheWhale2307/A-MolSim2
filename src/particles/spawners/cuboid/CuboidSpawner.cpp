#include "CuboidSpawner.h"

#include "particles/Particle.h"
#include "physics/thermostats/Thermostat.h"
#include "utils/ArrayUtils.h"

CuboidSpawner::CuboidSpawner(const std::array<double, 3> &lower_left_corner, const std::array<int, 3> &grid_dimensions, bool is_membrane, double grid_spacing,
                             double mass, const std::array<double, 3> &initial_velocity, int type, double epsilon, double sigma,
                             bool third_dimension, double initial_temperature)
    : lower_left_corner(lower_left_corner),
      grid_dimensions(grid_dimensions),
      is_membrane(is_membrane),
      grid_spacing(grid_spacing),
      mass(mass),
      type(type),
      epsilon(epsilon),
      sigma(sigma),
      initial_velocity(initial_velocity),
      third_dimension(third_dimension),
      initial_temperature(initial_temperature) {}

int CuboidSpawner::spawnParticles(std::vector<Particle> &particles) const
{
    size_t start_of_this_cuboid = particles.size();
    particles.reserve(particles.size() + getEstimatedNumberOfParticles());
    for (int i = 0; i < grid_dimensions[0]; i++)
    {
        for (int j = 0; j < grid_dimensions[1]; j++)
        {
            for (int k = 0; k < grid_dimensions[2]; k++)
            {
                const auto grid_pos = std::array<double, 3>{static_cast<double>(i), static_cast<double>(j), static_cast<double>(k)};

                const auto x = lower_left_corner + grid_spacing * grid_pos;

                Particle particle(x, initial_velocity, mass, type, epsilon, sigma);
                particle.setDisplacementToAdd({0, 0, 0});
                Thermostat::setParticleTemperature(initial_temperature, particle, third_dimension ? 3 : 2);
                particles.push_back(std::move(particle));
            }
        }
    }
    if (is_membrane)
    {
        auto convertLambda = [&](int index, int dx, int dy, int dz) {
        };
        for (int index = start_of_this_cuboid; index < particles.size(); index++)
        {
            // TODO: implement other dimensions
            if (grid_dimensions[1] == 1)
            {
                Particle &p1 = particles[index];
                // Particle &p2 = particles[convertLambda(index, 1,0,)];
                // convertLambda
            }
        }
    }
    return grid_dimensions[0] * grid_dimensions[1] * grid_dimensions[2];
}

size_t CuboidSpawner::getEstimatedNumberOfParticles() const
{
    return static_cast<size_t>(grid_dimensions[0]) * grid_dimensions[1] * grid_dimensions[2];
}