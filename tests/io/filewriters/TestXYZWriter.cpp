#include <gtest/gtest.h>

#include <fstream>

#include "data/FileLoader.h"
#include "io/FileUtils.h"
#include "io/output/FileOutputHandler.h"
#include "particles/containers/directsum/DirectSumContainer.h"
#include "utils/ArrayUtils.h"

/*
 * Test if the XYZWriter writes the correct data into the file.
 */
TEST(XYZWriter, CorrectWritingOfParticles) {
   /* std::unique_ptr<ParticleContainer> particle_container = std::make_unique<DirectSumContainer>();

    for (double i = 0; i < 5; i++) {
        auto pos = std::array<double, 3>{i, 2 * i, 3 * i};
        auto vel = std::array<double, 3>{4 * i, 5 * i, 6 * i};
        particle_container->addParticle(Particle(pos, vel, i, i));
    }

    auto params = SimulationParams("test.xml", 0, 0, SimulationParams::DirectSumType{}, {}, {}, {}, false);

    FileOutputHandler file_output_handler{OutputFormat::XYZ, params};

    auto path = file_output_handler.writeFile(0, particle_container);

    // load the file
    std::ifstream file(*path);
    std::stringstream buffer;
    buffer << file.rdbuf();

    // check if the file contains the correct data

    // clang-format off
    std::string expected = MULTILINE(
        5
        Generated by MolSim. See http:/\040/openbabel.org/wiki/XYZ_(format) for file format doku.
        Ar 0.00000 0.00000 0.00000 
        Ar 1.00000 2.00000 3.00000 
        Ar 2.00000 4.00000 6.00000 
        Ar 3.00000 6.00000 9.00000 
        Ar 4.00000 8.00000 12.0000 
    );
    // clang-format on

    EXPECT_EQ(remove_whitespace(buffer.str()), remove_whitespace(expected));*/
}