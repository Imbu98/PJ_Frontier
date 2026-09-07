#include "Animation/Notifies/FrontierAnimNotifyState_EnemyAttackTrace.h"

#include "Character/FrontierEnemyCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Frontier.h"

namespace
{
	AFrontierEnemyCharacter* ResolveEnemyCharacter(USkeletalMeshComponent* MeshComp)
	{
		return MeshComp ? Cast<AFrontierEnemyCharacter>(MeshComp->GetOwner()) : nullptr;
	}
}

FString UFrontierAnimNotifyState_EnemyAttackTrace::GetNotifyName_Implementation() const
{
	return TEXT("Frontier Enemy Attack Trace");
}

void UFrontierAnimNotifyState_EnemyAttackTrace::NotifyBegin(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const float TotalDuration,
	const FAnimNotifyEventReference& EventReference)
{
	FRONTIER_LOG(VeryVerbose, TEXT("Enemy attack trace notify began. Owner=%s Duration=%.2f"), *GetNameSafe(MeshComp ? MeshComp->GetOwner() : nullptr), TotalDuration);
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	if (AFrontierEnemyCharacter* EnemyCharacter = ResolveEnemyCharacter(MeshComp))
	{
		EnemyCharacter->BeginCurrentAttackTrace();
	}
}

void UFrontierAnimNotifyState_EnemyAttackTrace::NotifyTick(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const float FrameDeltaTime,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyTick(MeshComp, Animation, FrameDeltaTime, EventReference);

	if (AFrontierEnemyCharacter* EnemyCharacter = ResolveEnemyCharacter(MeshComp))
	{
		EnemyCharacter->TickCurrentAttackTrace();
	}
}

void UFrontierAnimNotifyState_EnemyAttackTrace::NotifyEnd(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	FRONTIER_LOG(VeryVerbose, TEXT("Enemy attack trace notify ended. Owner=%s"), *GetNameSafe(MeshComp ? MeshComp->GetOwner() : nullptr));
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	if (AFrontierEnemyCharacter* EnemyCharacter = ResolveEnemyCharacter(MeshComp))
	{
		EnemyCharacter->EndCurrentAttackTrace();
	}
}
