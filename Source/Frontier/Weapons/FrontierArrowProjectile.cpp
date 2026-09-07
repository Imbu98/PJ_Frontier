#include "Weapons/FrontierArrowProjectile.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Frontier.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

AFrontierArrowProjectile::AFrontierArrowProjectile()
{
	bReplicates = true;
	SetReplicateMovement(true);

	CollisionComponent = CreateDefaultSubobject<UBoxComponent>(TEXT("Collision"));
	SetRootComponent(CollisionComponent);
	CollisionComponent->InitBoxExtent(FVector(6.0,6.0,6.0));
	// The arrow is spawned at the bow socket. Keep collision disabled until it
	// has moved away from the bow so the initial overlap cannot destroy it.
	CollisionComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CollisionComponent->SetCollisionObjectType(ECC_WorldDynamic);
	CollisionComponent->SetCollisionResponseToAllChannels(ECR_Block);
	// Characters use dedicated Enemy and Player object channels rather than ECC_Pawn.
	// Overlap both so HandleOverlap applies damage; keep all world/object channels blocked.
	CollisionComponent->SetCollisionResponseToChannel(ECC_GameTraceChannel2, ECR_Overlap); // Enemy
	CollisionComponent->SetCollisionResponseToChannel(ECC_GameTraceChannel3, ECR_Overlap); // Player
	CollisionComponent->SetGenerateOverlapEvents(true);

	ArrowMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ArrowMesh"));
	ArrowMeshComponent->SetupAttachment(CollisionComponent);
	ArrowMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	ProjectileMovement->bRotationFollowsVelocity = true;
	ProjectileMovement->bShouldBounce = false;
	ProjectileMovement->bAutoActivate = false;
}

void AFrontierArrowProjectile::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AFrontierArrowProjectile, ReplicatedArrowMesh);
}

void AFrontierArrowProjectile::BeginPlay()
{
	Super::BeginPlay();
	CollisionComponent->OnComponentBeginOverlap.AddDynamic(this, &AFrontierArrowProjectile::HandleOverlap);
	CollisionComponent->OnComponentHit.AddDynamic(this, &AFrontierArrowProjectile::HandleHit);
}

void AFrontierArrowProjectile::Destroyed()
{
	FRONTIER_LOG(
		Log,
		TEXT("Arrow projectile destroyed. Arrow=%s Reason=%s Launched=%d Location=%s Velocity=%s"),
		*GetNameSafe(this),
		DestroyReason.IsEmpty() ? TEXT("LifeSpanOrExternal") : *DestroyReason,
		bHasLaunched ? 1 : 0,
		*GetActorLocation().ToCompactString(),
		ProjectileMovement ? *ProjectileMovement->Velocity.ToCompactString() : TEXT("None"));

	Super::Destroyed();
}

void AFrontierArrowProjectile::InitializeArrow(
	AActor* InDamageSource,
	UStaticMesh* InArrowMesh,
	const FFrontierDamageRequest& InDamageRequest,
	const float InGravityScale)
{
	DamageSource = InDamageSource;
	DamageRequest = InDamageRequest;
	ReplicatedArrowMesh = InArrowMesh;
	CollisionComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ProjectileMovement->StopMovementImmediately();
	ProjectileMovement->Deactivate();
	ProjectileMovement->ProjectileGravityScale = FMath::Max(0.0f, InGravityScale);
	OnRep_ArrowMesh();
	if (InDamageSource)
	{
		CollisionComponent->IgnoreActorWhenMoving(InDamageSource, true);
	}
}

void AFrontierArrowProjectile::Launch(const FVector& Direction, const float Speed, const float MaximumRange)
{
	const float SafeSpeed = FMath::Max(1.0f, Speed);
	bHasLaunched = true;
	ProjectileMovement->InitialSpeed = SafeSpeed;
	ProjectileMovement->MaxSpeed = SafeSpeed;
	ProjectileMovement->Velocity = Direction.GetSafeNormal() * SafeSpeed;
	ProjectileMovement->Activate(true);
	const float LifeSeconds = (FMath::Max(1.0f, MaximumRange) / SafeSpeed) + 0.5f;
	SetLifeSpan(LifeSeconds);
	FRONTIER_LOG(
		Log,
		TEXT("Arrow projectile launched. Arrow=%s Direction=%s Speed=%.2f LifeSeconds=%.2f MovementActive=%d"),
		*GetNameSafe(this),
		*Direction.GetSafeNormal().ToCompactString(),
		SafeSpeed,
		LifeSeconds,
		ProjectileMovement->IsActive() ? 1 : 0);

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick(
			FTimerDelegate::CreateUObject(this, &AFrontierArrowProjectile::ArmCollision));
	}
}

void AFrontierArrowProjectile::ArmCollision()
{
	if (IsValid(this) && CollisionComponent && bHasLaunched)
	{
		CollisionComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	}
}

void AFrontierArrowProjectile::HandleOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComponent,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	if (!HasAuthority() || !bHasLaunched || !OtherActor || OtherActor == DamageSource || OtherActor == this)
	{
		return;
	}

	FFrontierDamageRequest ImpactDamageRequest = DamageRequest;
	ImpactDamageRequest.HitLocation = bFromSweep
		? FVector(SweepResult.ImpactPoint)
		: (OtherComponent ? OtherComponent->GetComponentLocation() : OtherActor->GetActorLocation());
	const FFrontierDamageResult Result = UFrontierDamageStatics::ApplyDamage(DamageSource, OtherActor, ImpactDamageRequest);
	if (Result.bApplied || Result.bHit)
	{
		DestroyArrow(TEXT("DamageOverlap"));
	}
}

void AFrontierArrowProjectile::HandleHit(
	UPrimitiveComponent* HitComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComponent,
	FVector NormalImpulse,
	const FHitResult& Hit)
{
	if (HasAuthority() && bHasLaunched && OtherActor && OtherActor != DamageSource && OtherActor != this)
	{
		DestroyReason = FString::Printf(TEXT("BlockingHit:%s"), *GetNameSafe(OtherActor));
		Destroy();
	}
}

void AFrontierArrowProjectile::DestroyArrow(const TCHAR* Reason)
{
	DestroyReason = Reason ? Reason : TEXT("Unknown");
	Destroy();
}

void AFrontierArrowProjectile::OnRep_ArrowMesh()
{
	if (ArrowMeshComponent)
	{
		ArrowMeshComponent->SetStaticMesh(ReplicatedArrowMesh);
	}
}
