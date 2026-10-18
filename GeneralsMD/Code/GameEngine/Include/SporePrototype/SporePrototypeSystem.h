#ifndef SPORE_PROTOTYPE_SYSTEM_H
#define SPORE_PROTOTYPE_SYSTEM_H

#include "SporePrototype/SporeCreature.h"
#include "SporePrototype/SporeVoxelWorld.h"

namespace SporePrototype
{

class SporePrototypeSystem
{
public:
	SporePrototypeSystem();

	void reset(unsigned int seed, int width, int height);
	void update(unsigned int frame);

	SporeVoxelWorld &world() { return m_world; }
	const SporeVoxelWorld &world() const { return m_world; }

	SporeCreature &playerCreature() { return m_playerCreature; }
	const SporeCreature &playerCreature() const { return m_playerCreature; }

	int harvestAtPlayer(int amount);
	int getStoredCrystal() const { return m_storedCrystal; }

private:
	void createStarterCreature();
	void seedCrystalFields(unsigned int seed);

	SporeVoxelWorld m_world;
	SporeCreature m_playerCreature;
	int m_storedCrystal;
};

extern SporePrototypeSystem *g_sporePrototypeSystem;

} // namespace SporePrototype

#endif
