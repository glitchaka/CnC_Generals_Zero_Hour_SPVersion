#include "PreRTS.h"
#include "SporePrototype/SporeVoxelWorld.h"

namespace SporePrototype
{

static VoxelCell s_invalidCell;

VoxelCell::VoxelCell() :
	material(VOXEL_EMPTY),
	density(0),
	growth(0),
	flags(0)
{
}

CrystalNode::CrystalNode() :
	x(0),
	y(0),
	radius(0),
	reserve(0),
	maxReserve(0),
	seed(0),
	nextGrowthFrame(0)
{
}

SporeVoxelWorld::SporeVoxelWorld() :
	m_width(0),
	m_height(0),
	m_seed(0)
{
}

void SporeVoxelWorld::resize(int width, int height)
{
	m_width = width > 0 ? width : 0;
	m_height = height > 0 ? height : 0;
	m_cells.clear();
	m_cells.resize(m_width * m_height);
	m_nodes.clear();
}

void SporeVoxelWorld::clear()
{
	int i;
	for (i = 0; i < (int)m_cells.size(); ++i)
		m_cells[i] = VoxelCell();
	m_nodes.clear();
}

int SporeVoxelWorld::indexOf(int x, int y) const
{
	return y * m_width + x;
}

bool SporeVoxelWorld::isInside(int x, int y) const
{
	return x >= 0 && y >= 0 && x < m_width && y < m_height;
}

const VoxelCell &SporeVoxelWorld::getCell(int x, int y) const
{
	if (!isInside(x, y))
		return s_invalidCell;
	return m_cells[indexOf(x, y)];
}

VoxelCell &SporeVoxelWorld::getCell(int x, int y)
{
	if (!isInside(x, y))
		return s_invalidCell;
	return m_cells[indexOf(x, y)];
}

unsigned int SporeVoxelWorld::hash(unsigned int x, unsigned int y, unsigned int seed) const
{
	unsigned int h = x * 374761393u + y * 668265263u + seed * 2246822519u;
	h = (h ^ (h >> 13)) * 1274126177u;
	return h ^ (h >> 16);
}

void SporeVoxelWorld::generate(unsigned int seed)
{
	m_seed = seed;
	m_nodes.clear();

	int y;
	for (y = 0; y < m_height; ++y)
	{
		int x;
		for (x = 0; x < m_width; ++x)
		{
			VoxelCell &cell = m_cells[indexOf(x, y)];
			unsigned int h = hash((unsigned int)x, (unsigned int)y, seed);
			cell.material = ((h >> 6) & 7u) == 0u ? VOXEL_SOIL : VOXEL_GRASS;
			cell.density = 255;
			cell.growth = 0;
			cell.flags = 0;

			if ((h & 511u) == 0u)
				cell.material = VOXEL_ROCK;
		}
	}
}

void SporeVoxelWorld::addCrystalNode(int x, int y, int radius, int reserve, unsigned int seed)
{
	if (!isInside(x, y) || radius <= 0 || reserve <= 0)
		return;

	CrystalNode node;
	node.x = x;
	node.y = y;
	node.radius = radius;
	node.reserve = reserve;
	node.maxReserve = reserve;
	node.seed = seed;
	node.nextGrowthFrame = 0;
	m_nodes.push_back(node);
	paintBiomass(m_nodes.back());
}

void SporeVoxelWorld::paintBiomass(CrystalNode &node)
{
	int yy;
	for (yy = -node.radius; yy <= node.radius; ++yy)
	{
		int xx;
		for (xx = -node.radius; xx <= node.radius; ++xx)
		{
			int px = node.x + xx;
			int py = node.y + yy;
			if (!isInside(px, py))
				continue;

			int d2 = xx * xx + yy * yy;
			int r2 = node.radius * node.radius;
			if (d2 > r2)
				continue;

			unsigned int h = hash((unsigned int)px, (unsigned int)py, node.seed);
			if ((h & 3u) == 0u && d2 > (r2 * 3) / 4)
				continue;

			VoxelCell &cell = getCell(px, py);
			cell.material = VOXEL_PEACH_BIOMASS;
			cell.density = (unsigned char)(170 + (h % 86u));
			cell.growth = (unsigned char)(h & 31u);
		}
	}
}

void SporeVoxelWorld::growCrystals(CrystalNode &node, unsigned int frame)
{
	if (node.reserve <= 0 || frame < node.nextGrowthFrame)
		return;

	node.nextGrowthFrame = frame + 90u + (hash(frame, node.seed, m_seed) % 90u);

	unsigned int h = hash(frame + node.seed, (unsigned int)node.reserve, m_seed);
	int ox = (int)(h % (unsigned int)(node.radius * 2 + 1)) - node.radius;
	int oy = (int)((h >> 8) % (unsigned int)(node.radius * 2 + 1)) - node.radius;
	int px = node.x + ox;
	int py = node.y + oy;

	if (!isInside(px, py) || ox * ox + oy * oy > node.radius * node.radius)
		return;

	VoxelCell &cell = getCell(px, py);
	if (cell.material != VOXEL_PEACH_BIOMASS)
		return;

	switch ((h >> 16) % 3u)
	{
		case 0: cell.material = VOXEL_CRYSTAL_CYAN; break;
		case 1: cell.material = VOXEL_CRYSTAL_MAGENTA; break;
		default: cell.material = VOXEL_CRYSTAL_GREEN; break;
	}
	cell.growth = 255;
}

void SporeVoxelWorld::decayNode(CrystalNode &node)
{
	if (node.reserve > 0)
		return;

	int yy;
	for (yy = -node.radius; yy <= node.radius; ++yy)
	{
		int xx;
		for (xx = -node.radius; xx <= node.radius; ++xx)
		{
			int px = node.x + xx;
			int py = node.y + yy;
			if (!isInside(px, py))
				continue;

			VoxelCell &cell = getCell(px, py);
			if (cell.material == VOXEL_PEACH_BIOMASS ||
				cell.material == VOXEL_CRYSTAL_CYAN ||
				cell.material == VOXEL_CRYSTAL_MAGENTA ||
				cell.material == VOXEL_CRYSTAL_GREEN)
			{
				cell.material = VOXEL_SOIL;
				cell.growth = 0;
			}
		}
	}
}

void SporeVoxelWorld::update(unsigned int frame)
{
	int i;
	for (i = 0; i < (int)m_nodes.size(); ++i)
	{
		CrystalNode &node = m_nodes[i];
		if (node.reserve > 0)
			growCrystals(node, frame);
		else
			decayNode(node);
	}
}

int SporeVoxelWorld::harvest(int worldX, int worldY, int amount)
{
	if (amount <= 0)
		return 0;

	int best = -1;
	int bestD2 = 0x7fffffff;
	int i;
	for (i = 0; i < (int)m_nodes.size(); ++i)
	{
		CrystalNode &node = m_nodes[i];
		if (node.reserve <= 0)
			continue;
		int dx = node.x - worldX;
		int dy = node.y - worldY;
		int d2 = dx * dx + dy * dy;
		if (d2 <= node.radius * node.radius && d2 < bestD2)
		{
			best = i;
			bestD2 = d2;
		}
	}

	if (best < 0)
		return 0;

	CrystalNode &node = m_nodes[best];
	int harvested = amount < node.reserve ? amount : node.reserve;
	node.reserve -= harvested;

	// Remove one visible crystal close to the harvesting point.
	int radius = node.radius;
	int y;
	for (y = worldY - radius; y <= worldY + radius; ++y)
	{
		int x;
		for (x = worldX - radius; x <= worldX + radius; ++x)
		{
			if (!isInside(x, y))
				continue;
			VoxelCell &cell = getCell(x, y);
			if (cell.material == VOXEL_CRYSTAL_CYAN ||
				cell.material == VOXEL_CRYSTAL_MAGENTA ||
				cell.material == VOXEL_CRYSTAL_GREEN)
			{
				cell.material = VOXEL_PEACH_BIOMASS;
				cell.growth = 0;
				return harvested;
			}
		}
	}

	return harvested;
}

} // namespace SporePrototype
