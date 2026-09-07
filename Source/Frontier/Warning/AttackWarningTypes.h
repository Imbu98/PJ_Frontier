#pragma once

#include "CoreMinimal.h"
#include "AttackWarningTypes.generated.h"

class UMaterialInterface;
class UNiagaraSystem;

UENUM(BlueprintType)
enum class EAttackWarningShapeType : uint8
{
	Circle,
	Cone,
	Box,
	Line
};

USTRUCT(BlueprintType)
struct FAttackWarningData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Frontier|Warning")
	FVector WarningLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Frontier|Warning")
	FRotator WarningRotation = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Frontier|Warning", meta=(ClampMin="0.0"))
	float WarningRadius = 100.0f;

	/** Base world radius represented by the assigned Niagara warning system at scale 1.0. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Frontier|Warning|Niagara", meta=(ClampMin="1.0"))
	float WarningNiagaraReferenceRadius = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Frontier|Warning", meta=(ClampMin="0.0"))
	float WarningDuration = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Frontier|Warning")
	EAttackWarningShapeType WarningShapeType = EAttackWarningShapeType::Circle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Frontier|Warning")
	TObjectPtr<UMaterialInterface> WarningDecalMaterial = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Frontier|Warning")
	TObjectPtr<UNiagaraSystem> WarningNiagaraSystem = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Frontier|Warning")
	bool bAttachToGround = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Frontier|Warning")
	bool bDestroyOnImpact = true;
};
