#include "PreRTS.h"
#include "SporePrototype/SporePrototypeHooks.h"
#include "SporePrototype/SporePrototypeSystem.h"

using namespace SporePrototype;

void SporePrototype_Init(void)
{
	if (g_sporePrototypeSystem == 0)
		g_sporePrototypeSystem = NEW SporePrototypeSystem;

	// Initial dimensions are in logical voxel cells, not W3D world units.
	g_sporePrototypeSystem->reset(0x53A09E11u, 128, 128);
}

void SporePrototype_Reset(void)
{
	if (g_sporePrototypeSystem == 0)
		SporePrototype_Init();
	else
		g_sporePrototypeSystem->reset(0x53A09E11u, 128, 128);
}

void SporePrototype_Update(unsigned int frame)
{
	if (g_sporePrototypeSystem != 0)
		g_sporePrototypeSystem->update(frame);
}

void SporePrototype_Shutdown(void)
{
	if (g_sporePrototypeSystem != 0)
	{
		delete g_sporePrototypeSystem;
		g_sporePrototypeSystem = 0;
	}
}
