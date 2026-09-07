#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Blueprint/UserWidget.h"
#include "Components/SphereComponent.h"
#include "Extraction/FrontierExtractionZoneActor.h"
#include "FrontierPlayerController.h"
#include "Interaction/FrontierInteractableActor.h"
#include "Interaction/FrontierInteractionPolicy.h"
#include "Interaction/FrontierLobbyStorageActor.h"
#include "Interaction/FrontierRaidDeployActor.h"
#include "Interaction/FrontierTutorialPosterActor.h"
#include "Loot/FrontierDroppedItemActor.h"
#include "Loot/FrontierLootContainerActor.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UI/FrontierPlayerStatusWidget.h"
#include "UI/FrontierDropLootEntryWidget.h"
#include "UI/FrontierDropLootWidget.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFrontierInteractionPolicyTest,
	"Frontier.Interaction.ChannelPolicy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFrontierInteractionPolicyTest::RunTest(const FString& Parameters)
{
	FFrontierInteractionChannelContext ValidContext;
	ValidContext.bHasAuthority = true;
	ValidContext.bTargetValid = true;
	ValidContext.bPlayerValid = true;
	ValidContext.bTargetCanInteract = true;

	TestTrue(
		TEXT("A valid server interaction can start without a progress-widget dependency"),
		FFrontierInteractionPolicy::CanBeginChannel(ValidContext));

	FFrontierInteractionChannelContext ExtractionTargetContext = ValidContext;
	ExtractionTargetContext.bTargetIsExtractionZone = true;
	TestFalse(
		TEXT("An extraction zone cannot become an F-interaction target"),
		FFrontierInteractionPolicy::CanBeginChannel(ExtractionTargetContext));

	FFrontierInteractionChannelContext InsideExtractionContext = ValidContext;
	InsideExtractionContext.bPlayerInsideExtractionZone = true;
	TestFalse(
		TEXT("A player inside an extraction zone cannot start another interaction"),
		FFrontierInteractionPolicy::CanBeginChannel(InsideExtractionContext));

	FFrontierInteractionChannelContext PendingContext = ValidContext;
	PendingContext.bHasPendingInteraction = true;
	TestFalse(
		TEXT("A second channel cannot replace an active interaction"),
		FFrontierInteractionPolicy::CanBeginChannel(PendingContext));

	FFrontierInteractionChannelContext DeadPlayerContext = ValidContext;
	DeadPlayerContext.bPlayerDead = true;
	TestFalse(
		TEXT("A dead player cannot start an interaction"),
		FFrontierInteractionPolicy::CanBeginChannel(DeadPlayerContext));

	TestFalse(
		TEXT("Movement at the tolerance boundary remains valid"),
		FFrontierInteractionPolicy::HasExceededMovementTolerance(
			FVector::ZeroVector,
			FVector(25.0f, 0.0f, 0.0f),
			25.0f));
	TestTrue(
		TEXT("Movement beyond the tolerance cancels the channel"),
		FFrontierInteractionPolicy::HasExceededMovementTolerance(
			FVector::ZeroVector,
			FVector(25.1f, 0.0f, 0.0f),
			25.0f));

	TestTrue(
		TEXT("A rejected client focus retries with the authoritative nearest target"),
		FFrontierInteractionPolicy::ShouldRetryWithAuthoritativeTarget(false, false, false));
	TestFalse(
		TEXT("Authoritative fallback stays disabled inside an extraction zone"),
		FFrontierInteractionPolicy::ShouldRetryWithAuthoritativeTarget(false, false, true));
	TestFalse(
		TEXT("Authoritative fallback does not replace an active channel"),
		FFrontierInteractionPolicy::ShouldRetryWithAuthoritativeTarget(false, true, false));

	const AFrontierPlayerController* ControllerDefaults = GetDefault<AFrontierPlayerController>();
	const FFloatProperty* MovementToleranceProperty = FindFProperty<FFloatProperty>(
		AFrontierPlayerController::StaticClass(),
		TEXT("InteractionMovementCancelDistance"));
	TestNotNull(TEXT("Interaction movement tolerance remains configurable"), MovementToleranceProperty);
	if (ControllerDefaults && MovementToleranceProperty)
	{
		TestEqual(
			TEXT("The native movement tolerance allows small network corrections"),
			MovementToleranceProperty->GetPropertyValue_InContainer(ControllerDefaults),
			25.0f);
	}

	const FObjectProperty* BoundCircleProgressProperty = FindFProperty<FObjectProperty>(
		UFrontierPlayerStatusWidget::StaticClass(),
		TEXT("WBP_CircleProgressBar"));
	TestNotNull(TEXT("Player status exposes the circle-progress BindWidget slot"), BoundCircleProgressProperty);
	if (BoundCircleProgressProperty)
	{
		TestTrue(
			TEXT("The circle-progress slot accepts a user-widget child"),
			BoundCircleProgressProperty->PropertyClass->IsChildOf(UUserWidget::StaticClass()));
	}

	const FObjectProperty* InteractionPromptContainerProperty = FindFProperty<FObjectProperty>(
		UFrontierPlayerStatusWidget::StaticClass(),
		TEXT("InteractionPromptContainer"));
	const FObjectProperty* InteractionKeyTextProperty = FindFProperty<FObjectProperty>(
		UFrontierPlayerStatusWidget::StaticClass(),
		TEXT("InteractionKeyText"));
	const FObjectProperty* InteractionTargetNameTextProperty = FindFProperty<FObjectProperty>(
		UFrontierPlayerStatusWidget::StaticClass(),
		TEXT("InteractionTargetNameText"));
	const FObjectProperty* InteractionProgressPercentTextProperty = FindFProperty<FObjectProperty>(
		UFrontierPlayerStatusWidget::StaticClass(),
		TEXT("InteractionProgressPercentText"));
	TestNotNull(TEXT("Player status exposes the centered interaction prompt root"), InteractionPromptContainerProperty);
	TestNotNull(TEXT("Player status exposes the interaction key text"), InteractionKeyTextProperty);
	TestNotNull(TEXT("Player status exposes the interaction target-name text"), InteractionTargetNameTextProperty);
	TestNotNull(TEXT("Player status exposes the interaction percent text"), InteractionProgressPercentTextProperty);

	const AFrontierDroppedItemActor* DroppedItemDefaults = GetDefault<AFrontierDroppedItemActor>();
	TestTrue(
		TEXT("Dropped items use the shared interactable actor base"),
		AFrontierDroppedItemActor::StaticClass()->IsChildOf(AFrontierInteractableActor::StaticClass()));
	TestTrue(
		TEXT("Loot containers use the shared interactable actor base"),
		AFrontierLootContainerActor::StaticClass()->IsChildOf(AFrontierInteractableActor::StaticClass()));
	TestTrue(
		TEXT("Lobby storage uses the shared interactable actor base"),
		AFrontierLobbyStorageActor::StaticClass()->IsChildOf(AFrontierInteractableActor::StaticClass()));
	TestTrue(
		TEXT("Raid deploy actors use the shared interactable actor base"),
		AFrontierRaidDeployActor::StaticClass()->IsChildOf(AFrontierInteractableActor::StaticClass()));
	TestTrue(
		TEXT("Tutorial posters use the shared interactable actor base"),
		AFrontierTutorialPosterActor::StaticClass()->IsChildOf(AFrontierInteractableActor::StaticClass()));
	TestTrue(
		TEXT("Extraction zones use the shared interactable actor base"),
		AFrontierExtractionZoneActor::StaticClass()->IsChildOf(AFrontierInteractableActor::StaticClass()));
	TestTrue(
		TEXT("Dropped items bypass the timed interaction channel"),
		DroppedItemDefaults && DroppedItemDefaults->IsInstantInteraction(nullptr));
	const USphereComponent* DroppedItemQuerySphere = DroppedItemDefaults
		? DroppedItemDefaults->FindComponentByClass<USphereComponent>()
		: nullptr;
	TestNotNull(TEXT("Dropped items expose a nearby-loot query sphere"), DroppedItemQuerySphere);
	if (DroppedItemQuerySphere)
	{
		TestEqual(
			TEXT("Dropped-item query sphere uses the LootOrb object channel"),
			DroppedItemQuerySphere->GetCollisionObjectType(),
			ECC_GameTraceChannel5);
		TestEqual(
			TEXT("Dropped-item query sphere remains available for local overlap queries"),
			DroppedItemQuerySphere->GetCollisionEnabled(),
			ECollisionEnabled::QueryOnly);
		TestFalse(
			TEXT("Dropped-item query sphere does not generate persistent overlap events"),
			DroppedItemQuerySphere->GetGenerateOverlapEvents());
	}

	TestTrue(
		TEXT("Nearby dropped-loot panel requires a Widget Blueprint subclass"),
		UFrontierDropLootWidget::StaticClass()->IsChildOf(UUserWidget::StaticClass())
			&& UFrontierDropLootWidget::StaticClass()->HasAnyClassFlags(CLASS_Abstract));
	TestTrue(
		TEXT("Nearby dropped-loot entries require a Widget Blueprint subclass"),
		UFrontierDropLootEntryWidget::StaticClass()->IsChildOf(UUserWidget::StaticClass())
			&& UFrontierDropLootEntryWidget::StaticClass()->HasAnyClassFlags(CLASS_Abstract));
	TestEqual(
		TEXT("Dropped items use the acquire action"),
		DroppedItemDefaults ? DroppedItemDefaults->GetInteractionActionText(nullptr).ToString() : FString(),
		FString(TEXT("획득")));
	TestEqual(
		TEXT("Loot containers use the search action for boxes, monster loot, and corpses"),
		GetDefault<AFrontierLootContainerActor>()->GetInteractionActionText(nullptr).ToString(),
		FString(TEXT("수색")));
	TestEqual(
		TEXT("Lobby storage uses the open action"),
		GetDefault<AFrontierLobbyStorageActor>()->GetInteractionActionText(nullptr).ToString(),
		FString(TEXT("열기")));
	TestEqual(
		TEXT("Tutorial posters use the open action"),
		GetDefault<AFrontierTutorialPosterActor>()->GetInteractionActionText(nullptr).ToString(),
		FString(TEXT("열기")));
	TestEqual(
		TEXT("Raid interactables use the open action"),
		GetDefault<AFrontierRaidDeployActor>()->GetInteractionActionText(nullptr).ToString(),
		FString(TEXT("열기")));

	TestNull(
		TEXT("The controller no longer owns a projected per-actor interaction prompt"),
		FindFProperty<FObjectProperty>(AFrontierPlayerController::StaticClass(), TEXT("InteractionPromptWidget")));

	UClass* CircleProgressWidgetClass = LoadClass<UUserWidget>(
		nullptr,
		TEXT("/Game/SY/WBP_CircleProgressBar.WBP_CircleProgressBar_C"));
	TestNotNull(TEXT("WBP_CircleProgressBar can be loaded"), CircleProgressWidgetClass);
	if (CircleProgressWidgetClass)
	{
		const UFunction* SetPercentFunction = CircleProgressWidgetClass->FindFunctionByName(TEXT("SetPercent"));
		TestNotNull(TEXT("WBP_CircleProgressBar implements SetPercent"), SetPercentFunction);

		int32 InputParameterCount = 0;
		bool bHasNumericPercentInput = false;
		if (SetPercentFunction)
		{
			for (TFieldIterator<FProperty> PropertyIterator(SetPercentFunction); PropertyIterator; ++PropertyIterator)
			{
				const FProperty* Property = *PropertyIterator;
				if (!Property->HasAnyPropertyFlags(CPF_Parm) || Property->HasAnyPropertyFlags(CPF_ReturnParm))
				{
					continue;
				}

				++InputParameterCount;
				bHasNumericPercentInput = CastField<FFloatProperty>(Property) != nullptr
					|| CastField<FDoubleProperty>(Property) != nullptr;
			}
		}
		TestEqual(TEXT("SetPercent has exactly one input"), InputParameterCount, 1);
		TestTrue(TEXT("SetPercent accepts a Blueprint real value"), bHasNumericPercentInput);

		const FObjectProperty* DynamicMaterialProperty = FindFProperty<FObjectProperty>(
			CircleProgressWidgetClass,
			TEXT("RoundProgressbarInst"));
		TestNotNull(TEXT("WBP_CircleProgressBar exposes its generated dynamic material"), DynamicMaterialProperty);
		if (DynamicMaterialProperty)
		{
			TestTrue(
				TEXT("The generated progress material is a dynamic material instance"),
				DynamicMaterialProperty->PropertyClass->IsChildOf(UMaterialInstanceDynamic::StaticClass()));
		}
	}

	const UFunction* FocusedInteractionRpc = AFrontierPlayerController::StaticClass()->FindFunctionByName(
		TEXT("ServerRequestFocusedInteraction"));
	TestTrue(
		TEXT("Focused interaction remains a reliable server RPC"),
		FocusedInteractionRpc
			&& FocusedInteractionRpc->HasAllFunctionFlags(FUNC_Net | FUNC_NetServer | FUNC_NetReliable));

	const UFunction* DroppedItemPickupRpc = AFrontierPlayerController::StaticClass()->FindFunctionByName(
		TEXT("ServerPickupDroppedItem"));
	TestTrue(
		TEXT("Nearby-loot pickup remains an explicit reliable server RPC"),
		DroppedItemPickupRpc
			&& DroppedItemPickupRpc->HasAllFunctionFlags(FUNC_Net | FUNC_NetServer | FUNC_NetReliable));

	const UFunction* DroppedItemSlotPickupRpc = AFrontierPlayerController::StaticClass()->FindFunctionByName(
		TEXT("ServerPickupDroppedItemToRaidSlot"));
	TestTrue(
		TEXT("Dropped-item slot pickup remains an explicit reliable server RPC"),
		DroppedItemSlotPickupRpc
			&& DroppedItemSlotPickupRpc->HasAllFunctionFlags(FUNC_Net | FUNC_NetServer | FUNC_NetReliable));

	return true;
}

#endif
