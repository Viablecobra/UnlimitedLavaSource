/*
*   partially Open sourced btw :p
*         "I am Viable 🥀" 
*/

#include "../SDK/offsets.h"
#include "../SDK/BlockType.h"
#include "../SDK/HitResult.h"

#include "pl/memory/Vtable.hpp"
#include "pl/memory/Hook.hpp"

#include <cstdint>

#define LIBMC "libminecraftpe.so"

struct BlockChangeContext { uint64_t pad[8]; };

using NeighborChangedFn = void(*)(void*, void*, const BlockPos&, const BlockPos&);
using GetBlockFn        = const Block*(*)(void*, const BlockPos&);
using SetBlockFn        = bool(*)(void*, const BlockPos&, const Block*,
                                  int, const void*, const BlockChangeContext&);

static NeighborChangedFn      g_orig = nullptr;
static pl::memory::HookHandle g_hook;

static const BlockPos kDirs[4] = { {1,0,0},{-1,0,0},{0,0,1},{0,0,-1} };

static const Block* getBlock(void* region, const BlockPos& pos) {
    void** vft = *reinterpret_cast<void***>(region);
    return reinterpret_cast<GetBlockFn>(vft[mc_offsets::Slots::BS_getBlock])(region, pos);
}

static bool setBlock(void* region, const BlockPos& pos, const Block* block) {
    void** vft = *reinterpret_cast<void***>(region);
    BlockChangeContext ctx{};
    return reinterpret_cast<SetBlockFn>(vft[mc_offsets::Slots::BS_setBlock])(
        region, pos, block, 3, nullptr, ctx);
}

static constexpr int kFloodMax = 64;

static bool tryConvert(void* region, const BlockPos& pos) {
    const Block* block = getBlock(region, pos);
    if (!block || !block->blockType()) return false;
    if (!block->blockType()->isLava()) return false;
    if (block->data() == 0) return false;

    const Block* below = getBlock(region, {pos.x, pos.y-1, pos.z});
    if (!below || !below->blockType() || below->blockType()->isAir()) return false;

    int          sourceCnt   = 0;
    const Block* sourceBlock = nullptr;
    for (const BlockPos& d : kDirs) {
        const Block* adj = getBlock(region, pos + d);
        if (!adj || !adj->blockType()) continue;
        if (!adj->blockType()->isLava()) continue;
        if (adj->data() != 0) continue;
        if (sourceCnt == 0) sourceBlock = adj;
        if (++sourceCnt >= 2) break;
    }
    if (sourceCnt < 2 || !sourceBlock) return false;
    return setBlock(region, pos, sourceBlock);
}

static void floodConvert(void* region, const BlockPos& seed) {
    BlockPos visited[kFloodMax];
    BlockPos queue[kFloodMax];
    int vCount = 0, head = 0, tail = 0;

    queue[tail++]   = seed;
    visited[vCount++] = seed;

    while (head < tail) {
        BlockPos cur = queue[head++];
        const Block* block = getBlock(region, cur);
        if (!block || !block->blockType()) continue;
        if (!block->blockType()->isLava()) continue;

        if (block->data() != 0) tryConvert(region, cur);

        for (const BlockPos& d : kDirs) {
            BlockPos nb = cur + d;
            bool seen = false;
            for (int i = 0; i < vCount; i++) {
                if (visited[i] == nb) { seen = true; break; }
            }
            if (seen) continue;
            if (vCount < kFloodMax) visited[vCount++] = nb;
            const Block* nbBlock = getBlock(region, nb);
            if (!nbBlock || !nbBlock->blockType()) continue;
            if (!nbBlock->blockType()->isLava()) continue;
            if (tail < kFloodMax) queue[tail++] = nb;
        }
    }
}

static void hk_neighborChanged(void*           self,
                                void*           region,
                                const BlockPos& pos,
                                const BlockPos& neighborPos)
{
    g_orig(self, region, pos, neighborPos);

    const Block* posBlock = getBlock(region, pos);
    if (!posBlock || !posBlock->blockType()) return;
    if (!posBlock->blockType()->isLava()) return;

    const Block* nbBlock = getBlock(region, neighborPos);
    bool nbIsLava = nbBlock && nbBlock->blockType() && nbBlock->blockType()->isLava();
    bool nbIsAir  = !nbBlock || !nbBlock->blockType() || nbBlock->blockType()->isAir();
    if (!nbIsLava && !nbIsAir) return;

    floodConvert(region, pos);
}

__attribute__((constructor)) static void init() {
    uintptr_t fn = pl::memory::resolveVtableFunction(
    "11LiquidBlock",
    mc_offsets::Slots::LB_neighborChanged,
    LIBMC);
    if (!fn) { return; }
    g_hook = pl::memory::HookHandle((void*)fn, (void*)hk_neighborChanged, (void**)&g_orig);
    if (!g_hook.installed()) { return; }
}