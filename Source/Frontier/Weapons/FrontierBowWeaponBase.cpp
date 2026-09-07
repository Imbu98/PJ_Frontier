#include "Weapons/FrontierBowWeaponBase.h"

#include "Animation/FrontierBowAnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Net/UnrealNetwork.h"

AFrontierBowWeaponBase::AFrontierBowWeaponBase()
{
}

void AFrontierBowWeaponBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AFrontierBowWeaponBase, BowChargeProgress);
}

void AFrontierBowWeaponBase::SetBowChargeProgress(const float NewChargeProgress)
{
	BowChargeProgress = FMath::Clamp(NewChargeProgress, 0.0f, 1.0f);
	ApplyBowChargeProgressToAnimInstance();
}

void AFrontierBowWeaponBase::OnRep_BowChargeProgress()
{
	ApplyBowChargeProgressToAnimInstance();
}

void AFrontierBowWeaponBase::ApplyBowChargeProgressToAnimInstance()
{
	USkeletalMeshComponent* BowMesh = GetWeaponMesh();
	UFrontierBowAnimInstance* BowAnimInstance = BowMesh
		? Cast<UFrontierBowAnimInstance>(BowMesh->GetAnimInstance())
		: nullptr;
	if (!BowAnimInstance)
	{
		return;
	}

	BowAnimInstance->SetChargeProgress(BowChargeProgress);
}
