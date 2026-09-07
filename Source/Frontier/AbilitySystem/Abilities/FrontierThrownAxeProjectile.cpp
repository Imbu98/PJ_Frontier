#include "AbilitySystem/Abilities/FrontierThrownAxeProjectile.h"

#include "Combat/FrontierDamageStatics.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Frontier.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Net/UnrealNetwork.h"

AFrontierThrownAxeProjectile::AFrontierThrownAxeProjectile()
{
	PrimaryActorTick.bCanEverTick = true;

	bReplicates = true;
	SetReplicateMovement(true);

	SphereCollision = CreateDefaultSubobject<USphereComponent>(TEXT("SphereCollision"));
	SetRootComponent(SphereCollision);
	SphereCollision->InitSphereRadius(24.0f);
	SphereCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	SphereCollision->SetCollisionObjectType(ECC_WorldDynamic);
	SphereCollision->SetCollisionResponseToAllChannels(ECR_Ignore);
	SphereCollision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	SphereCollision->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Overlap);
	SphereCollision->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
	SphereCollision->SetGenerateOverlapEvents(true);

	AxeMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("AxeMesh"));
	AxeMesh->SetupAttachment(SphereCollision);
	AxeMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	AxeMesh->SetGenerateOverlapEvents(false);
	AxeMesh->SetRelativeRotation(MeshRelativeRotation);

	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	ProjectileMovement->InitialSpeed = 1800.0f;
	ProjectileMovement->MaxSpeed = 1800.0f;
	ProjectileMovement->ProjectileGravityScale = 0.0f;
	ProjectileMovement->bRotationFollowsVelocity = true;
	ProjectileMovement->bShouldBounce = false;
	ProjectileMovement->bAutoActivate = false;
}

void AFrontierThrownAxeProjectile::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bSpinMeshOnPitch || !AxeMesh)
	{
		return;
	}

	const FRotator SpinDelta(MeshPitchSpinSpeed * DeltaSeconds, 0.0f, 0.0f);
	AxeMesh->AddLocalRotation(SpinDelta);
}

void AFrontierThrownAxeProjectile::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AFrontierThrownAxeProjectile, ReplicatedWeaponMesh);
}

void AFrontierThrownAxeProjectile::BeginPlay()
{
	Super::BeginPlay();

	ApplyReplicatedWeaponMesh();

	if (HasAuthority())
	{
		SphereCollision->OnComponentBeginOverlap.AddDynamic(this, &AFrontierThrownAxeProjectile::OnSphereBeginOverlap);
		SetLifeSpan(LifeSeconds);
	}
}

void AFrontierThrownAxeProjectile::InitProjectile(
	AActor* InSourceActor,
	USkeletalMesh* InWeaponMesh,
	const TArray<UMaterialInterface*>& InWeaponMaterials,
	const float InBaseDamage,
	const float InDamageMultiplier,
	const FGameplayTag InDamageTypeTag,
	const EFrontierElementalType InElementalType,
	TSubclassOf<UGameplayEffect> InDamageEffectClass)
{
	FRONTIER_LOG(Log, TEXT("Initializing thrown axe projectile. Source=%s Mesh=%s BaseDamage=%.2f Multiplier=%.2f DamageType=%s Element=%d"),
		*GetNameSafe(InSourceActor),
		*GetNameSafe(InWeaponMesh),
		InBaseDamage,
		InDamageMultiplier,
		*InDamageTypeTag.ToString(),
		static_cast<int32>(InElementalType));

	SourceActor = InSourceActor;
	SetOwner(InSourceActor);
	ReplicatedWeaponMesh = InWeaponMesh;
	BaseDamage = InBaseDamage;
	DamageMultiplier = InDamageMultiplier;
	DamageTypeTag = InDamageTypeTag;
	ElementalType = InElementalType;
	DamageEffectClass = InDamageEffectClass;

	ApplyReplicatedWeaponMesh();

	if (AxeMesh)
	{
		for (int32 MaterialIndex = 0; MaterialIndex < InWeaponMaterials.Num(); ++MaterialIndex)
		{
			AxeMesh->SetMaterial(MaterialIndex, InWeaponMaterials[MaterialIndex]);
		}
	}
}

void AFrontierThrownAxeProjectile::LaunchInDirection(const FVector& Direction, const float Speed)
{
	if (!ProjectileMovement)
	{
		return;
	}

	const FVector LaunchDirection = Direction.GetSafeNormal();
	FRONTIER_LOG(Log, TEXT("Launching thrown axe projectile. Projectile=%s Direction=%s Speed=%.2f"),
		*GetNameSafe(this),
		*LaunchDirection.ToCompactString(),
		Speed);

	ProjectileMovement->InitialSpeed = Speed;
	ProjectileMovement->MaxSpeed = Speed;
	ProjectileMovement->Velocity = LaunchDirection * Speed;
	ProjectileMovement->Activate(true);
}

void AFrontierThrownAxeProjectile::OnRep_ReplicatedWeaponMesh()
{
	ApplyReplicatedWeaponMesh();
}

void AFrontierThrownAxeProjectile::ApplyReplicatedWeaponMesh()
{
	if (AxeMesh && ReplicatedWeaponMesh)
	{
		AxeMesh->SetSkeletalMesh(ReplicatedWeaponMesh);
	}
}

void AFrontierThrownAxeProjectile::OnSphereBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	if (!HasAuthority())
	{
		return;
	}

	if (!OtherActor || OtherActor == this || OtherActor == SourceActor)
	{
		return;
	}

	if (DamagedActors.Contains(OtherActor))
	{
		return;
	}

	DamagedActors.Add(OtherActor);

	FFrontierDamageRequest DamageRequest;
	DamageRequest.BaseDamage = BaseDamage;
	DamageRequest.DamageMultiplier = DamageMultiplier;
	DamageRequest.DamageTypeTag = DamageTypeTag;
	DamageRequest.ElementalType = ElementalType;
	DamageRequest.DamageEffectClass = DamageEffectClass;
	DamageRequest.HitLocation = bFromSweep
		? FVector(SweepResult.ImpactPoint)
		: (OtherComp ? OtherComp->GetComponentLocation() : OtherActor->GetActorLocation());

	const FFrontierDamageResult DamageResult = UFrontierDamageStatics::ApplyDamage(SourceActor, OtherActor, DamageRequest);
	FRONTIER_LOG(Log, TEXT("Thrown axe projectile overlap handled. Projectile=%s Target=%s Applied=%d Damage=%.2f"),
		*GetNameSafe(this),
		*GetNameSafe(OtherActor),
		DamageResult.bApplied ? 1 : 0,
		DamageResult.AppliedDamage);
}
