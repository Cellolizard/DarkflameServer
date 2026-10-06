#pragma once

#include <algorithm>
#include <cstdint>
#include <vector>

struct RacingProgress {
	uint64_t playerID;
	uint32_t finishedPlacement;
	uint32_t lap;
	uint32_t furthestPlane;
	uint64_t planeReachedOrder;
};

inline bool ShouldBroadcastRacingProgress(bool enabled, bool finished, uint32_t previousLap,
	uint32_t previousPlane, uint32_t lap, uint32_t plane) {
	return enabled && !finished && (previousLap != lap || previousPlane != plane);
}

inline bool RacingProgressAhead(const RacingProgress& lhs, const RacingProgress& rhs) {
	if (lhs.finishedPlacement != 0 || rhs.finishedPlacement != 0) {
		if (lhs.finishedPlacement == 0) return false;
		if (rhs.finishedPlacement == 0) return true;
		return lhs.finishedPlacement < rhs.finishedPlacement;
	}
	if (lhs.lap != rhs.lap) return lhs.lap > rhs.lap;
	if (lhs.furthestPlane != rhs.furthestPlane) return lhs.furthestPlane > rhs.furthestPlane;
	if (lhs.planeReachedOrder != rhs.planeReachedOrder) return lhs.planeReachedOrder < rhs.planeReachedOrder;
	return lhs.playerID < rhs.playerID;
}

inline std::vector<RacingProgress> OrderRacingProgress(std::vector<RacingProgress> racers) {
	std::sort(racers.begin(), racers.end(), RacingProgressAhead);
	return racers;
}
