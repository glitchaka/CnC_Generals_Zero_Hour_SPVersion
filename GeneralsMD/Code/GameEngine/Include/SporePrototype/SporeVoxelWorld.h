#ifndef SPORE_VOXEL_WORLD_H
#define SPORE_VOXEL_WORLD_H

#include <vector>

// Prototype voxel/resource layer for the creature-stage conversion.
// Kept independent from W3D so game logic can be tested without renderer code.

namespace SporePrototype
{

enum VoxelMaterial
{
	VOXEL_EMPTY = 0,
	VOXEL_SOIL,
	VOXEL_GRASS,
	VOXEL_ROCK,
	VOXEL_PEACH_BIOMASS,
	VOXEL_CRYSTAL_CYAN,
	VOXEL_CRYSTAL_MAGENTA,
	VOXEL_CRYSTAL_GREEN,
	VOXEL_WATER
};

struct VoxelCell
{
	VoxelCell();

	VoxelMaterial material;
	unsigned char density;
	unsigned char growth;
	unsigned char flags;
};

struct CrystalNode
{
	CrystalNode();

	int x;
	int y;
	int radius;
	int reserve;
	int maxReserve;
	unsigned int seed;
	unsigned int nextGrowthFrame;
};

class SporeVoxelWorld
{
public:
	SporeVoxelWorld();

	void resize(int width, int height);
	void clear();

	int getWidth() const { return m_width; }
	int getHeight() const { return m_height; }

	bool isInside(int x, int y) const;
	const VoxelCell &getCell(int x, int y) const;
	VoxelCell &getCell(int x, int y);

	void generate(unsigned int seed);
	void addCrystalNode(int x, int y, int radius, int reserve, unsigned int seed);
	void update(unsigned int frame);

	// Removes resource from the nearest crystal-bearing cells and returns
	// the amount actually harvested.
	int harvest(int worldX, int worldY, int amount);

	const std::vector<CrystalNode> &getCrystalNodes() const { return m_nodes; }

private:
	int indexOf(int x, int y) const;
	unsigned int hash(unsigned int x, unsigned int y, unsigned int seed) const;
	void paintBiomass(CrystalNode &node);
	void growCrystals(CrystalNode &node, unsigned int frame);
	void decayNode(CrystalNode &node);

	int m_width;
	int m_height;
	unsigned int m_seed;
	std::vector<VoxelCell> m_cells;
	std::vector<CrystalNode> m_nodes;
};

} // namespace SporePrototype

#endif
