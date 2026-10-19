#pragma once
#include <cstdint>
namespace KayKitKnight {
static constexpr uint32_t VertexCount=1666, IndexCount=10851, FrameCount=6;
static constexpr float PositionInvScale=0.0001f;
static const char IndicesB64[] =
#include "KnightIndices0.inc"
#include "KnightIndices1.inc"
#include "KnightIndices2.inc"
#include "KnightIndices3.inc"
;
static const char PositionsB64[] =
#include "KnightPositions0.inc"
#include "KnightPositions1.inc"
#include "KnightPositions2.inc"
#include "KnightPositions3.inc"
#include "KnightPositions4.inc"
;
}
