#include "PreRTS.h"
#include "SporePrototype/SporeCreature.h"
#include <math.h>

namespace SporePrototype
{

CreaturePart::CreaturePart() :
	slot(PART_BODY),
	mass(0.0f),
	speed(0.0f),
	health(0.0f),
	bite(0.0f),
	harvest(0.0f),
	social(0.0f)
{
}

CreatureStats::CreatureStats() :
	mass(1.0f),
	moveSpeed(1.0f),
	maxHealth(100.0f),
	attackPower(1.0f),
	harvestRate(1.0f),
	socialPower(1.0f)
{
}

SporeCreature::SporeCreature() :
	m_x(0.0f),
	m_y(0.0f),
	m_z(0.0f),
	m_facing(0.0f)
{
	clear();
}

void SporeCreature::clear()
{
	int i;
	for (i = 0; i < PART_SLOT_COUNT; ++i)
		m_hasPart[i] = false;
	m_stats = CreatureStats();
}

void SporeCreature::setPart(const CreaturePart &part)
{
	if (part.slot < 0 || part.slot >= PART_SLOT_COUNT)
		return;
	m_parts[part.slot] = part;
	m_hasPart[part.slot] = true;
	recalculateStats();
}

bool SporeCreature::hasPart(CreaturePartSlot slot) const
{
	return slot >= 0 && slot < PART_SLOT_COUNT && m_hasPart[slot];
}

const CreaturePart *SporeCreature::getPart(CreaturePartSlot slot) const
{
	if (!hasPart(slot))
		return 0;
	return &m_parts[slot];
}

void SporeCreature::recalculateStats()
{
	CreatureStats out;
	out.mass = 0.0f;
	out.moveSpeed = 1.0f;
	out.maxHealth = 50.0f;
	out.attackPower = 0.0f;
	out.harvestRate = 0.0f;
	out.socialPower = 0.0f;

	int i;
	for (i = 0; i < PART_SLOT_COUNT; ++i)
	{
		if (!m_hasPart[i])
			continue;
		const CreaturePart &p = m_parts[i];
		out.mass += p.mass;
		out.moveSpeed += p.speed;
		out.maxHealth += p.health;
		out.attackPower += p.bite;
		out.harvestRate += p.harvest;
		out.socialPower += p.social;
	}

	if (out.mass < 1.0f)
		out.mass = 1.0f;

	// Heavy creatures accelerate more slowly; legs/parts can compensate.
	out.moveSpeed = out.moveSpeed / (0.75f + out.mass * 0.08f);
	if (out.moveSpeed < 0.25f)
		out.moveSpeed = 0.25f;

	m_stats = out;
}

void SporeCreature::setPosition(float x, float y, float z)
{
	m_x = x;
	m_y = y;
	m_z = z;
}

void SporeCreature::move(float dx, float dy, float dt)
{
	float len2 = dx * dx + dy * dy;
	if (len2 <= 0.000001f || dt <= 0.0f)
		return;

	float invLen = 1.0f / (float)sqrt(len2);
	dx *= invLen;
	dy *= invLen;
	m_facing = (float)atan2(dy, dx);

	float speed = m_stats.moveSpeed * dt;
	m_x += dx * speed;
	m_y += dy * speed;
}

} // namespace SporePrototype
