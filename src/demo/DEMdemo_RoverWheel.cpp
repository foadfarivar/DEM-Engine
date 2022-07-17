//  Copyright (c) 2021, SBEL GPU Development Team
//  Copyright (c) 2021, University of Wisconsin - Madison
//  All rights reserved.

#include <core/ApiVersion.h>
#include <core/utils/ThreadManager.h>
#include <DEM/API.h>
#include <DEM/HostSideHelpers.hpp>

#include <cstdio>
#include <chrono>
#include <filesystem>

using namespace sgps;
using namespace std::filesystem;

int main() {
    DEMSolver DEM_sim;
    DEM_sim.SetVerbosity(INFO);
    DEM_sim.SetOutputFormat(DEM_OUTPUT_FORMAT::CSV);
    DEM_sim.SetOutputContent(DEM_OUTPUT_CONTENT::ABSV);

    // Define materials
    auto mat_type_terrain = DEM_sim.LoadMaterialType(2e9, 0.3, 0.6);
    auto mat_type_wheel = DEM_sim.LoadMaterialType(1e9, 0.3, 0.5);

    // Define the simulation world
    double world_size = 1.5;
    DEM_sim.InstructBoxDomainNumVoxel(22, 21, 21, world_size / std::pow(2, 16) / std::pow(2, 21));
    // Add 5 bounding planes around the simulation world, and leave the top open
    DEM_sim.InstructBoxDomainBoundingBC("top_open", mat_type_terrain);

    // Define the wheel geometry
    float wheel_rad = 0.25;
    float wheel_width = 0.2;
    float wheel_mass = 10.0;
    // Our shelf wheel geometry is lying flat on ground with z being the axial direction
    float wheel_IZZ = wheel_mass * wheel_rad * wheel_rad / 2;
    float wheel_IXX = (wheel_mass / 12) * (3 * wheel_rad * wheel_rad + wheel_width * wheel_width);
    auto wheel_template = DEM_sim.LoadClumpType(wheel_mass, make_float3(wheel_IXX, wheel_IXX, wheel_IZZ),
                                                "./data/clumps/ViperWheelSimple.csv", mat_type_wheel);
    // The file contains no wheel particles size info, so let's manually set them
    wheel_template->radii = std::vector<float>(wheel_template->nComp, 0.02);

    // Then the ground particle template
    DEMClumpTemplate ellipsoid_template;
    ellipsoid_template.ReadComponentFromFile("./data/clumps/ellipsoid_2_1_1.csv");
    // Calculate its mass and MOI
    float mass = 2.6e3 * 4. / 3. * SGPS_PI * 2 * 1 * 1;
    float3 MOI = make_float3(1. / 5. * mass * (1 * 1 + 2 * 2), 1. / 5. * mass * (1 * 1 + 2 * 2),
                             1. / 5. * mass * (1 * 1 + 1 * 1));
    // Scale the template we just created
    float scaling = 0.01;
    ellipsoid_template.mass *= scaling * scaling * scaling;
    ellipsoid_template.MOI *= scaling * scaling * scaling * scaling * scaling;
    std::for_each(ellipsoid_template.radii.begin(), ellipsoid_template.radii.end(),
                  [scaling](float& r) { r *= scaling; });
    std::for_each(ellipsoid_template.relPos.begin(), ellipsoid_template.relPos.end(),
                  [scaling](float3& r) { r *= scaling; });
    ellipsoid_template.materials =
        std::vector<std::shared_ptr<DEMMaterial>>(ellipsoid_template.nComp, mat_type_terrain);
    auto ground_particle_template = DEM_sim.LoadClumpType(ellipsoid_template);

    // Instantiate this wheel
    auto wheel = DEM_sim.AddClumps(wheel_template, make_float3(0, 0, 0.35));
    // Let's `flip' the wheel's initial position so... yeah, it's like how wheel operates normally
    wheel->SetOriQ(make_float4(0.7071, 0.7071, 0, 0));
    // Sample and add ground particles
    float3 sample_center = make_float3(0, 0, -0.35);
    float sample_halfheight = 0.35;
    float sample_halfwidth = 0.7;
    auto ground_particles_xyz =
        DEMBoxGridSampler(sample_center, make_float3(sample_halfwidth, sample_halfwidth, sample_halfheight),
                          scaling * std::cbrt(2.0) * 2.1, scaling * std::cbrt(2.0) * 2.1, scaling * 2 * 2.1);
    auto ground_particles = DEM_sim.AddClumps(ground_particle_template, ground_particles_xyz);

    // Make ready for simulation
    float step_size = 5e-6;
    DEM_sim.InstructCoordSysOrigin("center");
    DEM_sim.SetTimeStepSize(step_size);
    DEM_sim.SetGravitationalAcceleration(make_float3(0, 0, -9.8));
    // If you want to use a large UpdateFreq then you have to expand spheres to ensure safety
    DEM_sim.SetCDUpdateFreq(40);
    // DEM_sim.SetExpandFactor(1e-3);
    DEM_sim.SuggestExpandFactor(2.);
    DEM_sim.SuggestExpandSafetyParam(1.2);
    DEM_sim.Initialize();

    path out_dir = current_path();
    out_dir += "/DEMdemo_RoverWheel";
    create_directory(out_dir);
    int currframe = 0;
    char filename[100];
    sprintf(filename, "%s/DEMdemo_output_%04d.csv", out_dir.c_str(), currframe);
    DEM_sim.WriteClumpFile(std::string(filename));

    std::cout << "DEMdemo_RoverWheel exiting..." << std::endl;
    // TODO: add end-game report APIs
    return 0;
}