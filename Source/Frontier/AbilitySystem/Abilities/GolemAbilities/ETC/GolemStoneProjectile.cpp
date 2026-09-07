#include "GolemStoneProjectile.h"

#include "Character/FrontierChaosDeathActor.h"
#include "Combat/FrontierDamageStatics.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Frontier.h"
#include "Game/FrontierGameState.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "CollisionQueryParams.h"
#include "Engine/OverlapResult.h"

AGolemStoneProjectile::AGolemStoneProjectile()
{
	

	PrimaryActorTick.bCanEverTick = false;

	bReplicates = true;
	SetReplicateMovement(true);

	SphereCollision = CreateDefaultSubobject<USphereComponent>(TEXT("SphereCollision"));
	SetRootComponent(SphereCollision);

	SphereCollision->InitSphereRadius(18.f);
	SphereCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	SphereCollision->SetCollisionObjectType(ECC_WorldDynamic);
	SphereCollision->SetCollisionResponseToAllChannels(ECR_Ignore);
	SphereCollision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	SphereCollision->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Ignore);
	SphereCollision->SetNotifyRigidBodyCollision(true);
	SphereCollision->SetGenerateOverlapEvents(true);

	RockMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RockMesh"));
	RockMesh->SetupAttachment(SphereCollision);
	RockMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	ProjectileMovement->InitialSpeed = 1200.f;
	ProjectileMovement->MaxSpeed = 1200.f;
	ProjectileMovement->ProjectileGravityScale = 1.0f;
	ProjectileMovement->bRotationFollowsVelocity = true;
	ProjectileMovement->bShouldBounce = false;
	ProjectileMovement->bSweepCollision = true;
	ProjectileMovement->bAutoActivate = false;
}

void AGolemStoneProjectile::BeginPlay()
{
	

	Super::BeginPlay();

	if (HasAuthority())
	{
		// The projectile is enabled as a query component while held, so keep the
		// initial WorldStatic response disabled until the throw has started.
		SphereCollision->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Ignore);

		SphereCollision->OnComponentBeginOverlap.AddDynamic(
			this,
			&AGolemStoneProjectile::OnSphereBeginOverlap
		);
		SphereCollision->OnComponentHit.AddDynamic(
			this,
			&AGolemStoneProjectile::OnSphereHit
		);
		ProjectileMovement->OnProjectileStop.AddDynamic(
			this,
			&AGolemStoneProjectile::OnProjectileStop
		);

		SetLifeSpan(LifeSeconds);
	}
}

void AGolemStoneProjectile::Destroyed()
{
	GetWorldTimerManager().ClearTimer(WorldStaticCollisionTimerHandle);
	HideImpactWarning();
	Super::Destroyed();
}

void AGolemStoneProjectile::InitProjectile(
	AActor* InOwnerActor,
	AActor* InInstigatorActor,
	TSubclassOf<UGameplayEffect> InDamageEffectClass,
	float InDamage,
	FGameplayTag InDamageTypeTag
)
{
	FRONTIER_LOG(Log, TEXT("Initializing golem projectile. Owner=%s Instigator=%s Damage=%.2f DamageType=%s"),
		*GetNameSafe(InOwnerActor),
		*GetNameSafe(InInstigatorActor),
		InDamage,
		*InDamageTypeTag.ToString());

	OwnerActor = InOwnerActor;
	InstigatorActor = InInstigatorActor;
	DamageEffectClass = InDamageEffectClass;
	Damage = InDamage;
	DamageTypeTag = InDamageTypeTag;
	bImpactHandled = false;

	SetOwner(InOwnerActor);
}

void AGolemStoneProjectile::SetHeldProjectileState(bool bHeld)
{
	FRONTIER_LOG(Log, TEXT("Setting golem projectile held state. Projectile=%s Held=%d"),
		*GetNameSafe(this),
		bHeld ? 1 : 0);

	SphereCollision->SetCollisionEnabled(bHeld ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryOnly);

	if (bHeld)
	{
		ProjectileMovement->StopMovementImmediately();
		ProjectileMovement->Deactivate();
	}
}

void AGolemStoneProjectile::LaunchToDirection(const FVector& Direction, float Speed)
{
	FRONTIER_LOG(Log, TEXT("Launching golem projectile. Projectile=%s Direction=%s Speed=%.2f"),
		*GetNameSafe(this),
		*Direction.ToString(),
		Speed);

	const FVector LaunchVelocity = Direction.GetSafeNormal() * Speed;

	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	SetHeldProjectileState(false);
	DelayWorldStaticCollisionAfterLaunch();
	ProjectileMovement->Velocity = LaunchVelocity;
	ProjectileMovement->InitialSpeed = Speed;
	ProjectileMovement->MaxSpeed = Speed;
	ProjectileMovement->Activate(true);
}

void AGolemStoneProjectile::LaunchToTarget(const FVector& TargetLocation, float Speed)
{
	const FVector StartLocation = GetActorLocation();
	const FVector ToTarget = TargetLocation - StartLocation;

	const FVector HorizontalDelta(ToTarget.X, ToTarget.Y, 0.f);
	const float HorizontalDistance = HorizontalDelta.Size();

	const float SafeSpeed = FMath::Max(Speed, 1.f);

	// 너무 가까운 경우 보정
	if (HorizontalDistance < 100.f)
	{
		LaunchToDirection(GetActorForwardVector(), Speed);
		return;
	}

	// 도착 시간 계산
	const float FlightTime = FMath::Clamp(
		HorizontalDistance / SafeSpeed,
		0.35f,
		1.5f
	);

	const float GravityZ = GetWorld()
		? GetWorld()->GetGravityZ() * ProjectileMovement->ProjectileGravityScale
		: -980.f;

	const FVector GravityAcceleration(0.f, 0.f, GravityZ);

	const FVector LaunchVelocity =
		(ToTarget - 0.5f * GravityAcceleration * FlightTime * FlightTime) / FlightTime;

	const float LaunchSpeed = LaunchVelocity.Size();

	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

	SetHeldProjectileState(false);
	DelayWorldStaticCollisionAfterLaunch();

	ProjectileMovement->Velocity = LaunchVelocity;
	ProjectileMovement->InitialSpeed = LaunchSpeed;
	ProjectileMovement->MaxSpeed = LaunchSpeed * 2.0f;

	ProjectileMovement->Activate(true);
}

void AGolemStoneProjectile::DelayWorldStaticCollisionAfterLaunch()
{
	if (!HasAuthority() || !SphereCollision || !GetWorld())
	{
		return;
	}

	SphereCollision->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Ignore);
	WorldStaticCollisionGraceStartLocation = SphereCollision->GetComponentLocation();
	GetWorldTimerManager().SetTimer(
		WorldStaticCollisionTimerHandle,
		this,
		&AGolemStoneProjectile::EnableWorldStaticCollision,
		0.2f,
		false);
}

void AGolemStoneProjectile::EnableWorldStaticCollision()
{
	if (!HasAuthority() || !SphereCollision || IsActorBeingDestroyed())
	{
		return;
	}

	SphereCollision->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);

	// Changing a response from Ignore to Block does not generate a hit for
	// geometry the projectile already passed through during the grace period.
	// Detect that case explicitly so the stone cannot tunnel through the floor.
	if (!GetWorld() || bImpactHandled)
	{
		return;
	}

	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldStatic);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(GolemStoneWorldStaticEnable), false, this);
	QueryParams.AddIgnoredActor(this);
	QueryParams.AddIgnoredActor(OwnerActor);
	QueryParams.AddIgnoredActor(InstigatorActor);

	const FVector CurrentLocation = SphereCollision->GetComponentLocation();
	const FCollisionShape CollisionShape =
		FCollisionShape::MakeSphere(SphereCollision->GetScaledSphereRadius());

	// The projectile intentionally ignores WorldStatic for the first 0.2 seconds.
	// Sweep that grace-period path when the response is enabled so it cannot pass
	// through a floor or wall without producing a hit event.
	TArray<FHitResult> BlockingHits;
	const bool bFoundBlockingHit = GetWorld()->SweepMultiByObjectType(
		BlockingHits,
		WorldStaticCollisionGraceStartLocation,
		CurrentLocation,
		SphereCollision->GetComponentQuat(),
		ObjectQueryParams,
		CollisionShape,
		QueryParams);

	if (bFoundBlockingHit)
	{
		for (const FHitResult& Hit : BlockingHits)
		{
			AActor* OtherActor = Hit.GetActor();
			if (!OtherActor || OtherActor == this || OtherActor == OwnerActor || OtherActor == InstigatorActor)
			{
				continue;
			}

			const FVector ImpactLocation = Hit.ImpactPoint.IsNearlyZero()
				? CurrentLocation
				: FVector(Hit.ImpactPoint);
			const FVector ImpactNormal = Hit.ImpactNormal.IsNearlyZero()
				? FVector::UpVector
				: FVector(Hit.ImpactNormal);
			HandleBlockingImpact(OtherActor, ImpactLocation, ImpactNormal);
			return;
		}
	}

	TArray<FOverlapResult> Overlaps;
	const bool bHasWorldStaticOverlap = GetWorld()->OverlapMultiByObjectType(
		Overlaps,
		CurrentLocation,
		SphereCollision->GetComponentQuat(),
		ObjectQueryParams,
		CollisionShape,
		QueryParams);

	if (!bHasWorldStaticOverlap)
	{
		return;
	}

	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* OtherActor = Overlap.GetActor();
		if (!OtherActor
			|| OtherActor == this
			|| OtherActor == OwnerActor
			|| OtherActor == InstigatorActor)
		{
			continue;
		}

		const FVector ImpactNormal = ProjectileMovement && !ProjectileMovement->Velocity.IsNearlyZero()
			? -ProjectileMovement->Velocity.GetSafeNormal()
			: FVector::UpVector;
		HandleBlockingImpact(OtherActor, CurrentLocation, ImpactNormal);
		return;
	}
}

void AGolemStoneProjectile::SetImpactWarningId(const FGuid InWarningId)
{
	ImpactWarningId = InWarningId;
	bImpactWarningHidden = false;
}

void AGolemStoneProjectile::HideImpactWarning()
{
	if (bImpactWarningHidden || !HasAuthority() || !ImpactWarningId.IsValid())
	{
		return;
	}

	bImpactWarningHidden = true;
	if (AFrontierGameState* FrontierGameState = GetWorld() ? GetWorld()->GetGameState<AFrontierGameState>() : nullptr)
	{
		FrontierGameState->MulticastHideAttackWarning(ImpactWarningId);
	}
	ImpactWarningId.Invalidate();
}

void AGolemStoneProjectile::OnSphereBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult
)
{
	if (!HasAuthority())
	{
		return;
	}

	if (!OtherActor || OtherActor == this || OtherActor == OwnerActor || OtherActor == InstigatorActor)
	{
		return;
	}
	if (bImpactHandled)
	{
		return;
	}

	bImpactHandled = true;

	FFrontierDamageRequest DamageRequest;
	DamageRequest.BaseDamage = Damage;
	DamageRequest.DamageMultiplier = DamageMultiplier;
	DamageRequest.DamageTypeTag = DamageTypeTag;
	DamageRequest.DamageEffectClass = DamageEffectClass;

	const FFrontierDamageResult DamageResult = UFrontierDamageStatics::ApplyDamage(OwnerActor, OtherActor, DamageRequest);
	FRONTIER_LOG(Log, TEXT("Golem projectile overlap handled. Projectile=%s Target=%s Applied=%d Damage=%.2f"),
		*GetNameSafe(this),
		*GetNameSafe(OtherActor),
		DamageResult.bApplied ? 1 : 0,
		DamageResult.AppliedDamage);

	FVector ImpactLocation = GetActorLocation();
	if (!SweepResult.ImpactPoint.IsNearlyZero())
	{
		ImpactLocation = FVector(SweepResult.ImpactPoint);
	}

	FVector ImpactNormal = (GetActorLocation() - OtherActor->GetActorLocation()).GetSafeNormal();
	if (!SweepResult.ImpactNormal.IsNearlyZero())
	{
		ImpactNormal = FVector(SweepResult.ImpactNormal);
	}
	SpawnImpactBreakEffect(ImpactLocation, ImpactNormal);
	HideImpactWarning();
	Destroy();
}

void AGolemStoneProjectile::OnSphereHit(
	UPrimitiveComponent* HitComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	const FVector NormalImpulse,
	const FHitResult& Hit)
{
	if (!HasAuthority()
		|| bImpactHandled
		|| !OtherActor
		|| OtherActor == this
		|| OtherActor == OwnerActor
		|| OtherActor == InstigatorActor)
	{
		return;
	}

	const FVector ImpactLocation = Hit.ImpactPoint.IsNearlyZero()
		? GetActorLocation()
		: FVector(Hit.ImpactPoint);
	const FVector ImpactNormal = Hit.ImpactNormal.IsNearlyZero()
		? -GetActorForwardVector()
		: FVector(Hit.ImpactNormal);
	HandleBlockingImpact(OtherActor, ImpactLocation, ImpactNormal);
}

void AGolemStoneProjectile::OnProjectileStop(const FHitResult& ImpactResult)
{
	if (!HasAuthority() || bImpactHandled)
	{
		return;
	}

	AActor* OtherActor = ImpactResult.GetActor();
	if (!OtherActor)
	{
		return;
	}

	const FVector ImpactLocation = ImpactResult.ImpactPoint.IsNearlyZero()
		? GetActorLocation()
		: FVector(ImpactResult.ImpactPoint);
	const FVector ImpactNormal = ImpactResult.ImpactNormal.IsNearlyZero()
		? -GetActorForwardVector()
		: FVector(ImpactResult.ImpactNormal);
	HandleBlockingImpact(OtherActor, ImpactLocation, ImpactNormal);
}

void AGolemStoneProjectile::HandleBlockingImpact(
	AActor* OtherActor,
	const FVector& ImpactLocation,
	const FVector& ImpactNormal)
{
	if (!HasAuthority()
		|| bImpactHandled
		|| !OtherActor
		|| OtherActor == this
		|| OtherActor == OwnerActor
		|| OtherActor == InstigatorActor)
	{
		return;
	}

	bImpactHandled = true;
	SpawnImpactBreakEffect(ImpactLocation, ImpactNormal);
	HideImpactWarning();
	Destroy();
}

void AGolemStoneProjectile::SpawnImpactBreakEffect(
	const FVector& ImpactLocation,
	const FVector& ImpactNormal)
{
	if (!HasAuthority() || !ImpactBreakActorClass || !GetWorld())
	{
		return;
	}

	const FVector SafeNormal = ImpactNormal.GetSafeNormal();
	const FVector SpawnLocation = ImpactLocation + SafeNormal * 2.0f;
	const FRotator SpawnRotation = RockMesh
		? RockMesh->GetComponentRotation()
		: GetActorRotation();
	const FVector SpawnScale = RockMesh
		? RockMesh->GetComponentScale()
		: FVector::OneVector;

	MulticastSpawnImpactBreakEffect(
		FTransform(SpawnRotation, SpawnLocation, SpawnScale),
		ImpactLocation);
}

void AGolemStoneProjectile::MulticastSpawnImpactBreakEffect_Implementation(
	const FTransform& SpawnTransform,
	const FVector ImpactPoint)
{
	if (GetNetMode() == NM_DedicatedServer || !ImpactBreakActorClass || !GetWorld())
	{
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = OwnerActor;
	SpawnParams.Instigator = Cast<APawn>(InstigatorActor);
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	if (AFrontierChaosDeathActor* ImpactBreakActor = GetWorld()->SpawnActor<AFrontierChaosDeathActor>(
		ImpactBreakActorClass,
		SpawnTransform,
		SpawnParams))
	{
		ImpactBreakActor->SetDestructionOrigin(ImpactPoint, true);
	}
}
