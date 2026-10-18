#ifndef SPORE_CREATURE_H
#define SPORE_CREATURE_H

#include <string>
#include <vector>

namespace SporePrototype
{

enum CreaturePartSlot
{
	PART_BODY = 0,
	PART_HEAD,
	PART_EYES,
	PART_MOUTH,
	PART_ARMS,
	PART_LEGS,
	PART_TAIL,
	PART_BACK,
	PART_ANTENNAE,
	PART_SLOT_COUNT
};

struct CreaturePart
{
	CreaturePart();

	std::string id;
	CreaturePartSlot slot;
	float mass;
	float speed;
	float health;
	float bite;
	float harvest;
	float social;
};

struct CreatureStats
{
	CreatureStats();

	float mass;
	float moveSpeed;
	float maxHealth;
	float attackPower;
	float harvestRate;
	float socialPower;
};

class SporeCreature
{
public:
	SporeCreature();

	void clear();
	void setPart(const CreaturePart &part);
	bool hasPart(CreaturePartSlot slot) const;
	const CreaturePart *getPart(CreaturePartSlot slot) const;

	const CreatureStats &getStats() const { return m_stats; }
	void recalculateStats();

	void setPosition(float x, float y, float z);
	void setFacing(float radians) { m_facing = radians; }
	void move(float dx, float dy, float dt);

	float getX() const { return m_x; }
	float getY() const { return m_y; }
	float getZ() const { return m_z; }
	float getFacing() const { return m_facing; }

private:
	CreaturePart m_parts[PART_SLOT_COUNT];
	bool m_hasPart[PART_SLOT_COUNT];
	CreatureStats m_stats;
	float m_x;
	float m_y;
	float m_z;
	float m_facing;
};

} // namespace SporePrototype

#endif
