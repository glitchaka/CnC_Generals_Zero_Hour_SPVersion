#include "SporePrototype/SporeVoxelWorld.h"
#include "SporePrototype/SporeCreature.h"
#include "Spore/SporeEvolution.h"
#include <iostream>

int main()
{
    using namespace SporePrototype;

    SporeVoxelWorld world;
    world.resize(64, 64);
    world.generate(1337u);
    world.addCrystalNode(32, 32, 8, 250, 99u);
    for (unsigned int frame = 0; frame < 600; ++frame)
        world.update(frame);

    int harvested = world.harvest(32, 32, 25);
    if (harvested <= 0) {
        std::cerr << "SP smoke failed: crystal node did not harvest\n";
        return 2;
    }

    SporeEvolutionState evolution;
    if (evolution.canSee() || evolution.canWalk()) {
        std::cerr << "SP smoke failed: visceroid started with organs\n";
        return 3;
    }

    int biomass = 100;
    int crystals = 100;
    SporePartUpgrade eye = { SPORE_PART_EYE, "eye_basic", 10, 5, 4, 0, 0, 0 };
    SporePartUpgrade leg = { SPORE_PART_LEG, "leg_basic", 15, 10, 0, 3, 0, 0 };
    if (!evolution.unlock(eye, biomass, crystals) || !evolution.unlock(leg, biomass, crystals)) {
        std::cerr << "SP smoke failed: evolution unlock failed\n";
        return 4;
    }
    if (!evolution.canSee() || !evolution.canWalk()) {
        std::cerr << "SP smoke failed: unlocked organs did not enable abilities\n";
        return 5;
    }

    SporeCreature creature;
    CreaturePart body;
    body.id = "visceroid_body";
    body.slot = PART_BODY;
    body.mass = 4.0f;
    body.health = 20.0f;
    creature.setPart(body);

    CreaturePart legs;
    legs.id = "legs_basic";
    legs.slot = PART_LEGS;
    legs.mass = 1.0f;
    legs.speed = 4.0f;
    creature.setPart(legs);
    creature.move(1.0f, 0.0f, 1.0f);

    if (creature.getX() <= 0.0f) {
        std::cerr << "SP smoke failed: creature locomotion failed\n";
        return 6;
    }

    std::cout << "SP core OK\n";
    std::cout << "harvested=" << harvested
              << " vision=" << evolution.getVision()
              << " speed=" << evolution.getSpeed()
              << " x=" << creature.getX() << "\n";
    return 0;
}
