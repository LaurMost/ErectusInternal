#pragma once
#include "../utils.h"
#include <vector>
#include <cstdint>

enum class MissileWeaponType : int
{
	Fatman = 0,
	AutoGrenadeLauncher = 1,
	M79GrenadeLauncher = 2,
	COUNT
};

struct MissileWeaponData
{
	const char* name;
	std::uint32_t projectileFormId;
	float gravity;
	float speed;
	float range;
};

struct TrajectoryPoint
{
	Vector3 position;
	float timeAtPoint;
};

class ProjectileSpawner final
{
public:
	// Hardcoded weapon data
	static constexpr MissileWeaponData MISSILE_WEAPONS[] = {
		{ "Fatman (Mini Nuke)",      0x000E6B2F, 0.6f, 1900.0f, 22000.0f },
		{ "Auto Grenade Launcher",   0x0020706D, 0.6f, 1900.0f, 22000.0f },
		{ "M79 Grenade Launcher",    0x0008F0EF, 0.6f, 1900.0f, 22000.0f },
	};
	
	// Core functions
	static bool SpawnAtTarget();
	static void UpdateTrajectory();
	
	// Get selected weapon data
	static const MissileWeaponData& GetSelectedWeapon();
	
	// Position helpers
	static Vector3 GetSpawnPosition();  // LocalPlayer.position + headHeightOffset
	static bool GetTargetPosition(Vector3& outPosition);
	
	// Trajectory calculation
	static std::vector<TrajectoryPoint> CalculateTrajectory(
		const Vector3& spawnPos,
		const Vector3& targetPos,
		float projectileSpeed,
		float gravity,
		int segments
	);
	
	// State for rendering
	inline static std::vector<TrajectoryPoint> currentTrajectory{};
	inline static Vector3 currentImpactPoint{};
	inline static float currentDistance = 0.0f;
	inline static float currentTravelTime = 0.0f;
	inline static bool hasValidTrajectory = false;

private:
	static constexpr float FO76_GRAVITY_MULTIPLIER = 686.3786621f;
	
	static void DirectionToRotation(const Vector3& direction, float& pitch, float& yaw);
	
	virtual void Dummy() = 0;
};
