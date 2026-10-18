# Spore Prototype on Zero Hour

This fork is being converted into a creature-stage prototype using the existing Zero Hour engine rather than a separate demo executable.

## Implemented core layer

- `SporeVoxelWorld`: logical voxel grid independent from W3D rendering.
- Peach biomass resource fields.
- Cyan / magenta / green crystal growth on top of biomass.
- Crystal reserves, regrowth cadence, depletion and harvesting.
- `SporeCreature`: modular body-part slots and derived gameplay stats.
- Player creature movement state independent from the original RTS unit definition.
- `SporePrototypeSystem`: owns world, player creature and stored crystal resource.
- Lifecycle hooks prepared for `GameLogic::init/reset/update`.

## Engine integration order

1. Add the new prototype sources to `GameEngine.dsp`.
2. Call the lifecycle hooks from Zero Hour `GameLogic`.
3. Add a W3D voxel renderer that batches visible voxel faces instead of spawning one engine Object per cube.
4. Add the peach biomass + crystal material set.
5. Bind direct creature control to Zero Hour input and camera systems.
6. Replace the starter creature representation with modular voxel body parts.
7. Reuse Zero Hour collision/pathing where appropriate; keep fine voxel occupancy in the prototype layer.

## Important design constraint

The voxel field is simulation data, not a collection of RTS Objects. Rendering each voxel as an Object would destroy performance and would fight the existing engine. W3D receives generated/batched geometry per chunk while the simulation keeps compact cells.

## Resource behavior

Crystal deposits are not bare crystals on grass. A deposit first creates an irregular peach/orange biomass mass. Crystals grow out of that mass. Harvesting removes visible crystals and drains the node reserve; exhausted deposits decay back toward soil.
