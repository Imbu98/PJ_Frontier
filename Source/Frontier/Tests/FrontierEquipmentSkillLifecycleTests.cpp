#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "AbilitySystem/Abilities/FrontierGameplayAbility_PlayerAttack.h"
#include "AbilitySystem/FrontierAbilitySystemComponent.h"
#include "Components/FrontierEquipmentSkillComponent.h"
#include "Components/FrontierLoadoutComponent.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "Game/FrontierPlayerState.h"
#include "Inventory/Items/FrontierItemCatalogSubsystem.h"
#include "Inventory/Items/FrontierAccessoryItemDataAsset.h"
#include "Inventory/Items/FrontierWeaponItemDataAsset.h"
#include "Skill/FrontierSkillDataAsset.h"
#include "Skill/FrontierWeaponSkillGenerationDataAsset.h"
#include "Tags/FrontierGameplayTags.h"
#include "Weapons/FrontierWeaponDataAsset.h"

namespace FrontierEquipmentSkillLifecycleTest
{
	class FScopedTestWorld
	{
	public:
		FScopedTestWorld()
		{
			if (!GEngine)
			{
				return;
			}

			const FName WorldName = MakeUniqueObjectName(
				nullptr,
				UWorld::StaticClass(),
				TEXT("FrontierEquipmentSkillTestWorld"),
				EUniqueObjectNameOptions::GloballyUnique);
			FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
			World = UWorld::CreateWorld(EWorldType::Game, false, WorldName, GetTransientPackage());
			if (World)
			{
				World->AddToRoot();
				WorldContext.SetCurrentWorld(World);
				World->InitializeActorsForPlay(FURL());
			}
		}

		~FScopedTestWorld()
		{
			if (!World || !GEngine)
			{
				return;
			}

			if (World->AreActorsInitialized())
			{
				for (AActor* Actor : FActorRange(World))
				{
					if (Actor)
					{
						Actor->RouteEndPlay(EEndPlayReason::LevelTransition);
					}
				}
			}
			GEngine->ShutdownWorldNetDriver(World);
			World->DestroyWorld(true);
			GEngine->DestroyWorldContext(World);
			World->RemoveFromRoot();
		}

		UWorld* Get() const
		{
			return World;
		}

	private:
		UWorld* World = nullptr;
	};

	FGameplayAbilitySpec* FindSpecBySource(
		UFrontierAbilitySystemComponent* AbilitySystemComponent,
		const TSubclassOf<UGameplayAbility> AbilityClass,
		const UObject* SourceObject)
	{
		if (!AbilitySystemComponent || !AbilityClass)
		{
			return nullptr;
		}

		for (FGameplayAbilitySpec& Spec : AbilitySystemComponent->GetActivatableAbilities())
		{
			if (Spec.Ability && Spec.Ability->GetClass() == AbilityClass && Spec.SourceObject.Get() == SourceObject)
			{
				return &Spec;
			}
		}
		return nullptr;
	}

	int32 CountSpecsByClass(
		const UFrontierAbilitySystemComponent* AbilitySystemComponent,
		const TSubclassOf<UGameplayAbility> AbilityClass)
	{
		int32 Count = 0;
		if (!AbilitySystemComponent || !AbilityClass)
		{
			return Count;
		}

		for (const FGameplayAbilitySpec& Spec : AbilitySystemComponent->GetActivatableAbilities())
		{
			Count += Spec.Ability && Spec.Ability->GetClass() == AbilityClass ? 1 : 0;
		}
		return Count;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFrontierEquipmentSkillLifecycleTest,
	"Frontier.Skill.Equipment.AbilitySpecLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFrontierEquipmentSkillLifecycleTest::RunTest(const FString& Parameters)
{
	using namespace FrontierEquipmentSkillLifecycleTest;

	FFrontierGameplayTags::InitializeNativeGameplayTags();
	FScopedTestWorld TestWorld;
	UWorld* World = TestWorld.Get();
	TestNotNull(TEXT("Transient authority world is available"), World);
	if (!World)
	{
		return false;
	}

	AFrontierPlayerState* PlayerState = World->SpawnActor<AFrontierPlayerState>();
	TestNotNull(TEXT("Frontier PlayerState is available"), PlayerState);
	if (!PlayerState)
	{
		return false;
	}

	UFrontierAbilitySystemComponent* AbilitySystemComponent = PlayerState->GetFrontierAbilitySystemComponent();
	UFrontierLoadoutComponent* LoadoutComponent = PlayerState->GetLoadoutComponent();
	UFrontierEquipmentSkillComponent* SkillComponent = PlayerState->GetEquipmentSkillComponent();
	TestNotNull(TEXT("ASC is available"), AbilitySystemComponent);
	TestNotNull(TEXT("Loadout component is available"), LoadoutComponent);
	TestNotNull(TEXT("Equipment skill component is available"), SkillComponent);
	if (!AbilitySystemComponent || !LoadoutComponent || !SkillComponent)
	{
		return false;
	}

	const TSubclassOf<UGameplayAbility> SharedAbilityClass = UFrontierGameplayAbility_PlayerAttack::StaticClass();
	const FGameplayAbilitySpecHandle ForeignHandle = AbilitySystemComponent->GiveAbility(
		FGameplayAbilitySpec(SharedAbilityClass, 9, INDEX_NONE, PlayerState));
	TestTrue(TEXT("Foreign system ability was granted"), ForeignHandle.IsValid());

	const FGameplayTag SkillTag = FGameplayTag::RequestGameplayTag(TEXT("Skill.Axe.Normal.ThrowAxe"));
	const FGameplayTag WeaponTypeTag = FFrontierGameplayTags::Get().WeaponTypeAxe;
	UFrontierSkillDataAsset* SkillData = NewObject<UFrontierSkillDataAsset>();
	FFrontierSkillInfo& SkillInfo = SkillData->Skills.AddDefaulted_GetRef();
	SkillInfo.SkillTag = SkillTag;
	SkillInfo.AbilityClass = SharedAbilityClass;
	SkillInfo.MaxLevel = 10;

	UFrontierWeaponSkillGenerationDataAsset* SkillGenerationData = NewObject<UFrontierWeaponSkillGenerationDataAsset>();
	FFrontierWeaponTypeSkillDataAsset& SkillDataMapping = SkillGenerationData->WeaponTypeSkillDataAssets.AddDefaulted_GetRef();
	SkillDataMapping.RequiredWeaponTypeTag = WeaponTypeTag;
	SkillDataMapping.WeaponTypeSkillDataAsset = SkillData;

	UFrontierWeaponDataAsset* WeaponData = NewObject<UFrontierWeaponDataAsset>();
	WeaponData->WeaponTypeTag = WeaponTypeTag;

	UFrontierWeaponItemDataAsset* WeaponItemData = NewObject<UFrontierWeaponItemDataAsset>();
	WeaponItemData->ItemId = TEXT("Test.EquipmentAbilityOwnership");
	WeaponItemData->WeaponData = WeaponData;

	FFrontierItemInstance WeaponItem;
	WeaponItem.ItemTemplateData = UFrontierItemCatalogSubsystem::BuildTemplateDataFromItemData(*WeaponItemData);
	WeaponItem.Quantity = 1;
	WeaponItem.EnsureRuntimeIdentity();
	FFrontierRuntimeSkillData& GeneratedSkill = WeaponItem.RuntimeGeneratedSkills.AddDefaulted_GetRef();
	GeneratedSkill.SkillTag = SkillTag;
	GeneratedSkill.SkillLevel = 3;
	GeneratedSkill.SlotIndex = 0;

	FFrontierLoadoutSlot MainWeaponSlot;
	MainWeaponSlot.SlotType = EFrontierEquipmentSlot::MainWeapon;
	MainWeaponSlot.bOccupied = true;
	MainWeaponSlot.ItemInstance = WeaponItem;
	LoadoutComponent->SetLoadoutSlotsFromSnapshot({ MainWeaponSlot });
	SkillComponent->RefreshFromLoadout();

	FGameplayAbilitySpec* ForeignSpec = AbilitySystemComponent->FindAbilitySpecFromHandle(ForeignHandle);
	FGameplayAbilitySpec* EquipmentSpec = FindSpecBySource(AbilitySystemComponent, SharedAbilityClass, SkillComponent);
	TestNotNull(TEXT("Foreign ability remains after equipment sync"), ForeignSpec);
	TestNotNull(TEXT("Equipment receives its own AbilitySpec"), EquipmentSpec);
	TestEqual(TEXT("Foreign ability level is not modified"), ForeignSpec ? ForeignSpec->Level : INDEX_NONE, 9);
	TestEqual(TEXT("Equipment AbilitySpec uses backend SkillLevel"), EquipmentSpec ? EquipmentSpec->Level : INDEX_NONE, 3);
	TestEqual(TEXT("Both ownership sources can use the same AbilityClass"), CountSpecsByClass(AbilitySystemComponent, SharedAbilityClass), 2);
	if (!EquipmentSpec)
	{
		return false;
	}

	UFrontierAccessoryItemDataAsset* AccessoryItemData = NewObject<UFrontierAccessoryItemDataAsset>();
	AccessoryItemData->ItemId = TEXT("Test.EquipmentSkillAccessory");
	AccessoryItemData->EquipSlot = EFrontierEquipmentSlot::Ring;

	FFrontierItemInstance AccessoryItem;
	AccessoryItem.ItemTemplateData = UFrontierItemCatalogSubsystem::BuildTemplateDataFromItemData(*AccessoryItemData);
	AccessoryItem.Quantity = 1;
	AccessoryItem.EnsureRuntimeIdentity();
	FFrontierRuntimeSkillData& AccessorySkill = AccessoryItem.RuntimeGeneratedSkills.AddDefaulted_GetRef();
	AccessorySkill.SkillTag = SkillTag;
	AccessorySkill.SkillLevel = 2;
	AccessorySkill.SlotIndex = 0;

	FFrontierLoadoutSlot RingSlot;
	RingSlot.SlotType = EFrontierEquipmentSlot::Ring;
	RingSlot.bOccupied = true;
	RingSlot.ItemInstance = AccessoryItem;
	LoadoutComponent->SetLoadoutSlotsFromSnapshot({ MainWeaponSlot, RingSlot });
	SkillComponent->RefreshFromLoadout();

	FFrontierGeneratedWeaponSkill ActiveSkill;
	TestTrue(TEXT("Weapon skill remains in the active skill slots"), SkillComponent->GetGeneratedSkillAtSlot(0, ActiveSkill));
	TestEqual(TEXT("Active slot contains the weapon skill"), ActiveSkill.SkillTag, SkillTag);
	TestFalse(TEXT("Accessory skill is not added as a separate active slot"), SkillComponent->GetGeneratedSkillAtSlot(1, ActiveSkill));
	TestEqual(TEXT("Weapon and accessory skill levels are summed"), SkillComponent->GetSkillLevel(SkillTag), 5);
	EquipmentSpec = FindSpecBySource(AbilitySystemComponent, SharedAbilityClass, SkillComponent);
	TestEqual(TEXT("Equipment AbilitySpec uses the summed skill level"), EquipmentSpec ? EquipmentSpec->Level : INDEX_NONE, 5);

	const FGameplayAbilitySpecHandle FirstEquipmentHandle = EquipmentSpec->Handle;
	LoadoutComponent->SetLoadoutSlotsFromSnapshot({});
	SkillComponent->RefreshFromLoadout();
	TestNull(TEXT("Inactive equipment spec is removed immediately"), AbilitySystemComponent->FindAbilitySpecFromHandle(FirstEquipmentHandle));
	TestNotNull(TEXT("Foreign ability survives equipment removal"), AbilitySystemComponent->FindAbilitySpecFromHandle(ForeignHandle));

	LoadoutComponent->SetLoadoutSlotsFromSnapshot({ MainWeaponSlot });
	SkillComponent->RefreshFromLoadout();
	EquipmentSpec = FindSpecBySource(AbilitySystemComponent, SharedAbilityClass, SkillComponent);
	TestNotNull(TEXT("Equipment spec is granted again after re-equip"), EquipmentSpec);
	if (!EquipmentSpec)
	{
		return false;
	}

	const FGameplayAbilitySpecHandle ActiveEquipmentHandle = EquipmentSpec->Handle;
	EquipmentSpec->ActiveCount = 1;
	LoadoutComponent->SetLoadoutSlotsFromSnapshot({});
	SkillComponent->RefreshFromLoadout();
	FGameplayAbilitySpec* DeferredSpec = AbilitySystemComponent->FindAbilitySpecFromHandle(ActiveEquipmentHandle);
	TestNotNull(TEXT("Active equipment spec is retained until activation ends"), DeferredSpec);
	TestTrue(TEXT("Active equipment spec is marked for removal after activation"), DeferredSpec && DeferredSpec->RemoveAfterActivation);
	TestNotNull(TEXT("Foreign ability still survives deferred equipment removal"), AbilitySystemComponent->FindAbilitySpecFromHandle(ForeignHandle));

	if (DeferredSpec)
	{
		DeferredSpec->ActiveCount = 0;
		AbilitySystemComponent->ClearAbility(ActiveEquipmentHandle);
	}
	return true;
}

#endif
