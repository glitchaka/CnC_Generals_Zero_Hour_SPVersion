#pragma once
#ifndef _SPORE_EVOLUTION_H_
#define _SPORE_EVOLUTION_H_

#include <vector>
#include <string>

enum SporePartType
{
	SPORE_PART_EYE = 0,
	SPORE_PART_LEG,
	SPORE_PART_ARM,
	SPORE_PART_MOUTH,
	SPORE_PART_TAIL,
	SPORE_PART_BACK,
	SPORE_PART_ANTENNA,
	SPORE_PART_SHELL,
	SPORE_PART_MISC
};

struct SporePartUpgrade
{
	SporePartType type;
	std::string id;
	int biomassCost;
	int crystalCost;
	int visionDelta;
	int speedDelta;
	int damageDelta;
	int armorDelta;
};

class SporeEvolutionState
{
public:
	SporeEvolutionState();

	void resetToVisceroid();
	bool unlock(const SporePartUpgrade &upgrade, int &biomassPool, int &crystalPool);

	bool hasPart(SporePartType type) const;
	int getPartCount(SporePartType type) const;

	bool canSee() const { return m_vision > 0; }
	bool canWalk() const { return getPartCount(SPORE_PART_LEG) > 0; }
	bool canManipulate() const { return getPartCount(SPORE_PART_ARM) > 0; }
	bool canBite() const { return getPartCount(SPORE_PART_MOUTH) > 0; }

	int getVision() const { return m_vision; }
	int getSpeed() const { return m_speed; }
	int getDamage() const { return m_damage; }
	int getArmor() const { return m_armor; }

private:
	std::vector<SporePartUpgrade> m_parts;
	int m_vision;
	int m_speed;
	int m_damage;
	int m_armor;
};

#endif
