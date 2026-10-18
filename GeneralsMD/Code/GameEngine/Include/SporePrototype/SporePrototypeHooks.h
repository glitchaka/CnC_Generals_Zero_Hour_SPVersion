#ifndef SPORE_PROTOTYPE_HOOKS_H
#define SPORE_PROTOTYPE_HOOKS_H

// Thin integration seam for Zero Hour's GameLogic lifecycle.
// These functions are intentionally engine-facing and keep the prototype
// implementation isolated from GameLogic.cpp.

void SporePrototype_Init(void);
void SporePrototype_Reset(void);
void SporePrototype_Update(unsigned int frame);
void SporePrototype_Shutdown(void);

#endif
