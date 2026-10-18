#include "PreRTS.h"
#include "SporePrototype/SporePrototypeSystem.h"

namespace SporePrototype
{

SporePrototypeSystem *g_sporePrototypeSystem = 0;

SporePrototypeSystem::SporePrototypeSystem() :
	m_storedCrystal(0)
{
}

void SporePrototypeSystem::reset(unsigned int seed, int width, int height)
{
	m_world.resize(width, height);
	m_world.generate(seed);
	m_storedCrystal = 0;
	createStarterCreature();
	seedCrystalFields(seed);
}

void SporePrototypeSystem::update(unsigned int frame)
{
	m_world.update(frame);
}

int SporePrototypeSystem::harvestAtPlayer(int amount)
{
	int x = (int)m_playerCreature.getX();
	int y = (int)m_playerCreature.getY();
	int got = m_world.harvest(x, y, amount);
	m_storedCrystal += got;
	return got;
}

void SporePrototypeSystem::createStarterCreature()
{
	m_playerCreature.clear();

	CreaturePart body;
	body.id = "StarterBody";
	body.slot = PART_BODY;
	body.mass = 6.0f;
	body.health = 80.0f;
	body.speed = 0.4f;
	body.harvest = 0.2f;
	m_playerCreature.setPart(body);

	CreaturePart legs;
	legs.id = "StarterLegs";
	legs.slot = PART_LEGS;
	legs.mass = 2.0f;
	legs.health = 15.0f;
	legs.speed = 3.8f;
	m_playerCreature.setPart(legs);

	CreaturePart eyes;
	eyes.id = "StarterEyes";
	eyes.slot = PART_EYES;
	eyes.mass = 0.5f;
	eyes.social = 0.6f;
	m_playerCreature.setPart(eyes);

	CreaturePart mouth;
	mouth.id = "StarterMouth";
	mouth.slot = PART_MOUTH;
	mouth.mass = 0.6f;
	mouth.bite = 4.0f;
	mouth.harvest = 1.5f;
	m_playerCreature.setPart(mouth);

	m_playerCreature.setPosition(32.0f, 32.0f, 0.0f);
}

void SporePrototypeSystem::seedCrystalFields(unsigned int seed)
{
	if (m_world.getWidth() < 16 || m_world.getHeight() < 16)
		return;

	int w = m_world.getWidth();
	int h = m_world.getHeight();

	m_world.addCrystalNode(w / 4, h / 3, 7, 600, seed ^ 0x19A34F21u);
	m_world.addCrystalNode((w * 2) / 3, h / 2, 10, 1200, seed ^ 0xA2D5C381u);
	m_world.addCrystalNode((w * 4) / 5, (h * 4) / 5, 6, 450, seed ^ 0x4F208E17u);
}

} // namespace SporePrototype
