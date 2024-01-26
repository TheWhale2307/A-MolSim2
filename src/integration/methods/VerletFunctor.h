#pragma once

#include "integration/IntegrationFunctor.h"
#include "particles/containers/linkedcells/boundaries/ReflectiveBoundaryType.h"
#include "particles/containers/linkedcells/boundaries/OutflowBoundaryType.h"
#include "particles/containers/linkedcells/boundaries/PeriodicBoundaryType.h"


/**
 * @brief Implementation of the Strömer-Verlet integration method. Implements the interface IntegrationFunctor.
 *
 * Implements the IntegrationFunctor interface, and therefore updates all particles in the linkedCellsContainer according to the Verlet
 * integration method.
 */
class VerletFunctor : public IntegrationFunctor {
   public:
    /**
     * @brief Performs one step with the respective integration method.
     *
     * @param particleContainer Container of particles on which the integration step is applied
     * @param simple_force_sources Vector of simple force sources which are used to calculate the new forces
     * @param pairwise_force_sources Vector of pairwise force sources which are used to calculate the new forces
     * @param delta_t Time step
     */
    void step(std::unique_ptr<ParticleContainer>& particleContainer,
              const std::vector<std::shared_ptr<SimpleForceSource>>& simple_force_sources,
              const std::vector<std::shared_ptr<PairwiseForceSource>>& pairwise_force_sources, double delta_t) const override;

    template<unsigned N>
    void templated_step(std::unique_ptr<LinkedCellsContainer>& linkedCellsContainer, // maybe change this to LinkedCellsContainer
                                 const std::vector<std::shared_ptr<SimpleForceSource>>& simple_force_sources,
                                 const std::vector<std::shared_ptr<PairwiseForceSource>>& pairwise_force_sources, double delta_t) {
        static_assert(0 < N < 3);  // N should only be 1 (Subdomains) or 2 (Particles)

        if constexpr (N == 1) { // parallelization strategy 1: subdomains
            auto numDomains = 10;
            std::map<unsigned int, Subdomain *> subdomainsStep = linkedCellsContainer->getSubdomains();
            // update die positions
            // dann barrier
            // dann update den rest so gut es geht
#pragma omp parallel
            {
#pragma omp for schedule(static, 1)
                // TODO: assure that there are more subdomains than threads

                /// particles position calculations
                for (int i = 0; i < linkedCellsContainer->getSubdomains().size(); ++i) {
                    subdomainsStep.at(i)->updateParticlePositions();
                }  // <-- omp puts barrier automatically here
                //linkedCellsContainer->prepareForceCalculation();

                /// cell updates and boundary conditions
                for (Cell& cell : linkedCellsContainer->getCells()) {
                    cell.clearParticleReferences();
                }


                /*method:
                 * linkedCellsContainer.updateCellsParticleReferences();
                 * implementation:
                for (Cell& cell : cells) {
                    cell.clearParticleReferences();
                }

                // clear the set of used cells
                occupied_cells_references.clear();

                // add the particle references to the cells
                for (Particle& p : particles) {
                    Cell* cell = particlePosToCell(p.getX());

                    occupied_cells_references.insert(cell);
                    cell->addParticleReference(&p);
                }*/


                //TODO maybe parallelize
                ReflectiveBoundaryType::pre(*linkedCellsContainer);
                OutflowBoundaryType::pre(*linkedCellsContainer);
                PeriodicBoundaryType::pre(*linkedCellsContainer);
                // update the particle references in the cells in case the particles have moved
                updateCellsParticleReferences();


                ReflectiveBoundaryType::applyBoundaryConditions(*linkedCellsContainer);
                OutflowBoundaryType::applyBoundaryConditions(*linkedCellsContainer);
                PeriodicBoundaryType::applyBoundaryConditions(*linkedCellsContainer);


                /// force calculations
#pragma omp for schedule(static, 1)
                for (int i = 0; i < linkedCellsContainer->getSubdomains().size(); ++i) {
                    subdomainsStep.at(i)->updateSubdomain(pairwise_force_sources);
                }
            }
            linkedCellsContainer.deleteHaloParticles();
            linkedCellsContainer.updateCellsParticleReferences();














        } else { // parallelization strategy 2 : particles parallelization
            /**
             * @brief: for parallelizaton strategy 2, each cell will be processed sequentielly,
             * within the cells threads are spawned to update the particles
             */
            /// update the positions
#pragma omp parallel for schedule(dynamic)
            for (auto &p: *linkedCellsContainer) {
                // update position
                const std::array<double, 3> new_x =
                        p.getX() + delta_t * p.getV() + (delta_t * delta_t / (2 * p.getM())) * p.getF();
                p.setX(new_x);

                // reset forces
                p.setOldF(p.getF());
                p.setF({0, 0, 0});
            }

            linkedCellsContainer->prepareForceCalculation();

            /// apply simple forces
#pragma omp parallel for schedule(dynamic)
            for (auto &p: *linkedCellsContainer) { //directly apply gravitational
                //force here for performance
                auto currentF = p.getF();
                currentF[1] += p.getM() * (-gravityConstant); //somehow the verlet functor will need the gravityConstant
                p.setF(currentF);
            }

            /// apply pairwise forces
            linkedCellsContainer->applyPairwiseForces(pairwise_force_sources);

            // update the forces
            /*
             *  particle_container->prepareForceCalculation();  // here the boundary condition pre functions will be called
             *  updateCellsParticleReferences();
                 //TODO maybe parallelize
             *  ReflectiveBoundaryType::pre(*this);
                OutflowBoundaryType::pre(*this);
                PeriodicBoundaryType::pre(*this);
                // update the particle references in the cells in case the particles have moved
                 updateCellsParticleReferences();
                particle_container->applySimpleForces(simple_force_sources);
                particle_container->applyPairwiseForces(pairwise_force_sources); // here the boundary condition apply
             */


            //update the velocity
#pragma omp parallel for schedule(dynamic)
            for (auto &p: *linkedCellsContainer) {
                const std::array<double, 3> new_v = p.getV() + (delta_t / (2 * p.getM())) * (p.getF() + p.getOldF());
                p.setV(new_v);
            }
        }
    }

/*private:
    unsigned parallelization strategy*/
};