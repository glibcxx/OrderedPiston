#include "OrderedPiston.h"

#include "ll/api/memory/Hook.h"
#include "ll/api/mod/RegisterHelper.h"
#include "mc/util/Random.h"
#include "mc/world/level/BlockSource.h"
#include "mc/world/level/ChunkBlockPos.h"
#include "mc/world/level/Level.h"
#include "mc/world/level/block/actor/BlockActor.h"
#include "mc/world/level/chunk/ChunkSource.h"
#include "mc/world/level/chunk/LevelChunk.h"
#include "mc/world/redstone/circuit/CircuitSystem.h"


namespace ordered_piston {

static std::unique_ptr<OrderedPiston> instance;

OrderedPiston& OrderedPiston::getInstance() { return *instance; }

bool OrderedPiston::load() {
    getSelf().getLogger().debug("Loading...");
    // Code for loading the mod goes here.
    return true;
}

bool OrderedPiston::enable() {
    getSelf().getLogger().debug("Enabling...");
    // Code for enabling the mod goes here.
    return true;
}

bool OrderedPiston::disable() {
    getSelf().getLogger().debug("Disabling...");
    // Code for disabling the mod goes here.
    return true;
}

LL_AUTO_TYPE_INSTANCE_HOOK(
    PistonHook,
    HookPriority::Normal,
    LevelChunk,
    &LevelChunk::tickBlockEntities,
    void,
    BlockSource& tickRegion
) {
    std::vector<std::pair<ChunkBlockPos, BlockActor*>> orderedBlockActor;
    std::set<BlockActor*>                              search;

    auto& mBlockEntities = this->getBlockEntities();
    orderedBlockActor.reserve(mBlockEntities.size());

    CircuitSystem& circuitSystem = tickRegion.getDimension().getCircuitSystem();
    BlockPos       pos{this->getPosition(), 0};
    auto&          chunkList = circuitSystem.mSceneGraph.mActiveComponentsPerChunk[pos];

    for (auto&& item : chunkList.mComponents) {
        ChunkBlockPos cbp{item.mPos.x & 15, item.mPos.y + 64, item.mPos.z & 15};
        BlockActor*   found = this->getBlockEntity(cbp);
        if (found) {
            orderedBlockActor.emplace_back(cbp, found);
            search.emplace(found);
        }
    }

    std::stable_sort(
        orderedBlockActor.begin(),
        orderedBlockActor.end(),
        [&circuitSystem](const std::pair<ChunkBlockPos, BlockActor*>& l, const std::pair<ChunkBlockPos, BlockActor*>& r)
            -> bool {
            int lv = circuitSystem.getStrength(l.second->getPosition());
            int rv = circuitSystem.getStrength(r.second->getPosition());
            if (lv == 0 && rv != 0) return true;
            if (lv != 0 && rv == 0) return false;
            return lv > rv;
        }
    );

    for (auto&& it : mBlockEntities) {
        auto found = search.find(it.second.get());
        if (found == search.end()) {
            orderedBlockActor.emplace_back(it.first, it.second.get());
        }
    }

    // std::shuffle(blockEntities.begin(), blockEntities.end(), tickRegion.getLevel().getRandom());

    for (auto& blockEntityPair : orderedBlockActor) {
        BlockActor* maybeBadBlockEntity = blockEntityPair.second;
        BlockActor* foundInMap          = this->getBlockEntity(blockEntityPair.first);
        if (foundInMap != nullptr && foundInMap == maybeBadBlockEntity) {
            maybeBadBlockEntity->tick(tickRegion);
        }
    }

    std::vector<std::shared_ptr<BlockActor>> stillPreserve;
    auto&                                    mPreservedBlockEntities =
        const_cast<std::vector<std::shared_ptr<BlockActor>>&>(this->getPreservedBlockEntities());

    for (const auto& blockActor : mPreservedBlockEntities) {
        if (blockActor->shouldPreserve(tickRegion)) {
            stillPreserve.emplace_back(blockActor);
        }
    }
    mPreservedBlockEntities.swap(stillPreserve);
}

} // namespace ordered_piston

LL_REGISTER_MOD(ordered_piston::OrderedPiston, ordered_piston::instance);
