#include "ProjectileSpawner.h"
#include "MsgSender.h"
#include "../settings.h"
#include "../ErectusMemory.h"
#include "../ErectusProcess.h"
#include "../game/Game.h"
#include "../common.h"
#include <cmath>
#include <memory>

// CreateProjectileMessageClient structure matching game's expected format
struct CreateProjectileMessageClient
{
	std::uintptr_t vtable;           // 0x00
	float positionX;                 // 0x08
	float positionY;                 // 0x0C
	float positionZ;                 // 0x10
	char padding14[0x4];             // 0x14
	std::uintptr_t rotationArrayPtr; // 0x18
	std::uintptr_t rotationArrayEnd; // 0x20
	std::uintptr_t rotationArrayPad; // 0x28
	std::uint32_t itemId;            // 0x30
	std::uint32_t unknownA;          // 0x34
	std::uint32_t unknownB;          // 0x38
	std::uint32_t unknownC;          // 0x3C
	float unknownD;                  // 0x40
	std::uint32_t unknownE;          // 0x44
	std::uintptr_t unknownArrayPtrA; // 0x48
	std::uintptr_t unknownArrayEndA; // 0x50
	std::uintptr_t unknownArrayPadA; // 0x58
	std::uint8_t unknownF;           // 0x60
	char padding61[0x7];             // 0x61
	std::uintptr_t unknownArrayPtrB; // 0x68
	std::uintptr_t unknownArrayEndB; // 0x70
	std::uintptr_t unknownArrayPadB; // 0x78
	std::uint8_t unknownG;           // 0x80
};

struct ProjectilePayload
{
	CreateProjectileMessageClient message;
	float rotation[3];
	std::uint16_t unknownArrayValueA;
	std::uint8_t unknownArrayValueB;
};

const MissileWeaponData& ProjectileSpawner::GetSelectedWeapon()
{
	const int idx = Settings::projectileSpawner.selectedWeapon;
	if (idx >= 0 && idx < static_cast<int>(MissileWeaponType::COUNT))
		return MISSILE_WEAPONS[idx];
	return MISSILE_WEAPONS[0];  // Default to Fatman
}

Vector3 ProjectileSpawner::GetSpawnPosition()
{
	auto localPlayer = Game::GetLocalPlayer();
	
	// LocalPlayer.position is at feet level (TesObjectRefr offset 0x70)
	// Add configurable head height offset
	Vector3 spawnPos = localPlayer.position;
	spawnPos.z += Settings::projectileSpawner.headHeightOffset;
	
	return spawnPos;
}

bool ProjectileSpawner::GetTargetPosition(Vector3& outPosition)
{
	if (!ErectusMemory::targetLockedEntityPtr)
		return false;
	
	TesObjectRefr targetData{};
	if (!ErectusProcess::Rpm(ErectusMemory::targetLockedEntityPtr, &targetData, sizeof targetData))
		return false;
	
	outPosition = targetData.position;
	return true;
}

void ProjectileSpawner::DirectionToRotation(const Vector3& direction, float& pitch, float& yaw)
{
	const float horizontalDist = sqrtf(direction.x * direction.x + direction.y * direction.y);
	pitch = -atan2f(direction.z, horizontalDist);
	yaw = atan2f(-direction.x, direction.y);
}

std::vector<TrajectoryPoint> ProjectileSpawner::CalculateTrajectory(
	const Vector3& spawnPos,
	const Vector3& targetPos,
	float projectileSpeed,
	float gravity,
	int segments)
{
	std::vector<TrajectoryPoint> trajectory;
	
	const float adjustedGravity = gravity * FO76_GRAVITY_MULTIPLIER;
	const float distance = spawnPos.DistanceTo(targetPos);
	
	if (distance < 1.0f || projectileSpeed < 1.0f)
		return trajectory;
	
	const float travelTime = distance / projectileSpeed;
	
	// Initial velocity to hit target with gravity compensation
	Vector3 direction = targetPos - spawnPos;
	Vector3 velocity;
	velocity.x = direction.x / travelTime;
	velocity.y = direction.y / travelTime;
	velocity.z = (direction.z + 0.5f * adjustedGravity * travelTime * travelTime) / travelTime;
	
	// Generate arc points
	const float timeStep = travelTime / static_cast<float>(segments);
	for (int i = 0; i <= segments; ++i)
	{
		const float t = timeStep * static_cast<float>(i);
		TrajectoryPoint point;
		point.timeAtPoint = t;
		point.position.x = spawnPos.x + velocity.x * t;
		point.position.y = spawnPos.y + velocity.y * t;
		point.position.z = spawnPos.z + velocity.z * t - 0.5f * adjustedGravity * t * t;
		trajectory.push_back(point);
	}
	
	return trajectory;
}

void ProjectileSpawner::UpdateTrajectory()
{
	hasValidTrajectory = false;
	currentTrajectory.clear();
	
	if (!Settings::projectileSpawner.enabled)
		return;
	
	if (!Settings::projectileSpawner.drawTrajectory && !Settings::projectileSpawner.drawImpactMarker)
		return;
	
	Vector3 targetPos;
	if (!GetTargetPosition(targetPos))
		return;
	
	const Vector3 spawnPos = GetSpawnPosition();
	const auto& weapon = GetSelectedWeapon();
	
	currentDistance = spawnPos.DistanceTo(targetPos);
	
	if (currentDistance < 1.0f)
		return;
	
	currentTravelTime = currentDistance / weapon.speed;
	
	currentTrajectory = CalculateTrajectory(
		spawnPos,
		targetPos,
		weapon.speed,
		weapon.gravity,
		Settings::projectileSpawner.trajectorySegments
	);
	
	if (!currentTrajectory.empty())
	{
		currentImpactPoint = currentTrajectory.back().position;
		hasValidTrajectory = true;
	}
}

bool ProjectileSpawner::SpawnAtTarget()
{
	if (!Settings::projectileSpawner.enabled)
		return false;
	
	if (!MsgSender::IsEnabled())
		return false;
	
	const auto& weapon = GetSelectedWeapon();
	if (weapon.projectileFormId == 0)
		return false;
	
	Vector3 targetPos;
	if (!GetTargetPosition(targetPos))
		return false;
	
	const Vector3 spawnPos = GetSpawnPosition();
	const float distance = spawnPos.DistanceTo(targetPos);
	
	if (distance < 1.0f)
		return false;
	
	// Calculate aim direction with gravity compensation
	const float travelTime = distance / weapon.speed;
	const float adjustedGravity = weapon.gravity * FO76_GRAVITY_MULTIPLIER;
	
	Vector3 aimDir = targetPos - spawnPos;
	aimDir.z += 0.5f * adjustedGravity * travelTime * travelTime;
	
	float pitch, yaw;
	DirectionToRotation(aimDir, pitch, yaw);
	
	// Build the projectile message
	ProjectilePayload payload{};
	
	// We need to allocate memory for the arrays and set up pointers
	// For simplicity, we'll use a different approach - allocate the full structure
	const auto allocSize = sizeof(ExternalFunction) + sizeof(ProjectilePayload) + 0x20;
	const auto allocAddress = ErectusProcess::AllocEx(allocSize);
	if (!allocAddress)
		return false;
	
	// Calculate array offsets within allocated memory
	const auto payloadOffset = sizeof(ExternalFunction);
	const auto rotationArrayOffset = payloadOffset + offsetof(ProjectilePayload, rotation);
	const auto unknownArrayAOffset = payloadOffset + sizeof(ProjectilePayload);
	const auto unknownArrayBOffset = unknownArrayAOffset + sizeof(std::uint16_t);
	
	payload.message.vtable = ErectusProcess::exe + VTABLE_CREATEPROJECTILEMESSAGECLIENT;
	payload.message.positionX = spawnPos.x;
	payload.message.positionY = spawnPos.y;
	payload.message.positionZ = spawnPos.z;
	payload.message.rotationArrayPtr = allocAddress + rotationArrayOffset;
	payload.message.rotationArrayEnd = allocAddress + rotationArrayOffset + sizeof(float) * 3;
	payload.message.rotationArrayPad = allocAddress + rotationArrayOffset + sizeof(float) * 3;
	payload.message.itemId = weapon.projectileFormId;
	payload.message.unknownA = 0xFFFFFFFF;
	payload.message.unknownB = 0xFFFFFFFF;
	payload.message.unknownC = 0x00000000;
	payload.message.unknownD = 1.0f;
	payload.message.unknownE = 0x00000000;
	payload.message.unknownArrayPtrA = allocAddress + unknownArrayAOffset;
	payload.message.unknownArrayEndA = allocAddress + unknownArrayAOffset + sizeof(std::uint16_t);
	payload.message.unknownArrayPadA = allocAddress + unknownArrayAOffset + sizeof(std::uint16_t);
	payload.message.unknownF = 0xFF;
	payload.message.unknownArrayPtrB = allocAddress + unknownArrayBOffset;
	payload.message.unknownArrayEndB = allocAddress + unknownArrayBOffset + sizeof(std::uint8_t);
	payload.message.unknownArrayPadB = allocAddress + unknownArrayBOffset + sizeof(std::uint8_t);
	payload.message.unknownG = 0x00;
	
	payload.rotation[0] = pitch;
	payload.rotation[1] = 0.0f;
	payload.rotation[2] = yaw;
	payload.unknownArrayValueA = static_cast<std::uint16_t>(Utils::GetRangedInt(999, 9999));
	payload.unknownArrayValueB = 0x01;
	
	// Build full buffer with ExternalFunction header
	const auto pageData = std::make_unique<BYTE[]>(allocSize);
	memset(pageData.get(), 0x00, allocSize);
	
	ExternalFunction externalFunctionData = {
		.address = ErectusProcess::exe + OFFSET_MESSAGE_SENDER,
		.rcx = allocAddress + payloadOffset,  // Points to the message
		.rdx = 0,
		.r8 = 0,
		.r9 = 0
	};
	
	memcpy(pageData.get(), &externalFunctionData, sizeof externalFunctionData);
	memcpy(&pageData.get()[payloadOffset], &payload, sizeof payload);
	
	// Write data
	if (!ErectusProcess::Wpm(allocAddress, pageData.get(), static_cast<size_t>(allocSize)))
	{
		ErectusProcess::FreeEx(allocAddress);
		return false;
	}
	
	// Flip to RX for execution
	if (!ErectusProcess::ProtectEx(allocAddress, allocSize))
	{
		ErectusProcess::FreeEx(allocAddress);
		return false;
	}
	
	// Execute
	const auto paramAddress = allocAddress + sizeof(ExternalFunction::ASM);
	auto* const thread = CreateRemoteThread(ErectusProcess::handle, nullptr, 0,
		reinterpret_cast<LPTHREAD_START_ROUTINE>(allocAddress),
		reinterpret_cast<LPVOID>(paramAddress), 0, nullptr);
	
	if (!thread)
	{
		ErectusProcess::FreeEx(allocAddress);
		return false;
	}
	
	const auto threadResult = WaitForSingleObject(thread, 3000);
	CloseHandle(thread);
	
	ErectusProcess::FreeEx(allocAddress);
	return threadResult != WAIT_TIMEOUT;
}
