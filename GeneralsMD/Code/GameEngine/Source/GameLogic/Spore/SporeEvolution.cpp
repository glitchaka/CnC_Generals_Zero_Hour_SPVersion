#include "Spore/SporeEvolution.h"

SporeEvolutionState::SporeEvolutionState()
{
	resetToVisceroid();
}

void SporeEvolutionState::resetToVisceroid()
{
	m_parts.clear();
	m_vision = 0;
	m_speed = 1;
	m_damage = 1;
	m_armor = 0;
}

bool SporeEvolutionState::unlock(const SporePartUpgrade &upgrade, int &biomassPool, int &crystalPool)
{
	if (biomassPool < upgrade.biomassCost || crystalPool < upgrade.crystalCost)
		return false;

	biomassPool -= upgrade.biomassCost;
	crystalPool -= upgrade.crystalCost;
	m_parts.push_back(upgrade);
	m_vision += upgrade.visionDelta;
	m_speed += upgrade.speedDelta;
	m_damage += upgrade.damageDelta;
	m_armor += upgrade.armorDelta;
	return true;
}

bool SporeEvolutionState::hasPart(SporePartType type) const
{
	return getPartCount(type) > 0;
}

int SporeEvolutionState::getPartCount(SporePartType type) const
{
	int count = 0;
	for (std::vector<SporePartUpgrade>::const_iterator it = m_parts.begin(); it != m_parts.end(); ++it)
		if (it->type == type)
			++count;
	return count;
}
