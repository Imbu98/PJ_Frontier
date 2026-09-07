#include "UI/FrontierTeamStatusWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/VerticalBox.h"
#include "Frontier.h"
#include "Game/FrontierGameState.h"
#include "Game/FrontierPlayerState.h"
#include "UI/FrontierTeamMemberWidget.h"

void UFrontierTeamStatusWidget::NativeConstruct()
{
	
	Super::NativeConstruct();
	BuildWidgetTreeIfNeeded();
	RefreshTeamMembers();
}

void UFrontierTeamStatusWidget::NativeDestruct()
{
	
	MemberWidgets.Reset();
	Super::NativeDestruct();
}

void UFrontierTeamStatusWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	RefreshAccumulator += InDeltaTime;
	if (RefreshAccumulator >= 0.5f)
	{
		RefreshAccumulator = 0.0f;
		RefreshTeamMembers();
	}
}

void UFrontierTeamStatusWidget::RefreshTeamMembers()
{
	BuildWidgetTreeIfNeeded();

	AFrontierPlayerState* LocalPlayerState = GetOwningPlayer()
		? GetOwningPlayer()->GetPlayerState<AFrontierPlayerState>()
		: nullptr;
	AFrontierGameState* FrontierGameState = GetWorld()
		? GetWorld()->GetGameState<AFrontierGameState>()
		: nullptr;

	if (!FrontierGameState || !TeamMembersBox)
	{
		return;
	}

	TArray<AFrontierPlayerState*> TeamMembers;
	if (LocalPlayerState)
	{
		FrontierGameState->GetPlayersInTeam(LocalPlayerState->GetTeamId(), TeamMembers);
	}

	if (LocalPlayerState)
	{
		TeamMembers.Remove(LocalPlayerState);
	}

	if (!NeedsRebuild(TeamMembers))
	{
		for (UFrontierTeamMemberWidget* MemberWidget : MemberWidgets)
		{
			if (MemberWidget)
			{
				MemberWidget->RefreshFromPlayerState();
			}
		}
		return;
	}

	TeamMembersBox->ClearChildren();
	MemberWidgets.Reset();

	TSubclassOf<UFrontierTeamMemberWidget> WidgetClass = TeamMemberWidgetClass;
	if (!WidgetClass)
	{
		WidgetClass = UFrontierTeamMemberWidget::StaticClass();
	}

	for (AFrontierPlayerState* TeamMember : TeamMembers)
	{
		if (!TeamMember)
		{
			continue;
		}

		UFrontierTeamMemberWidget* MemberWidget = CreateWidget<UFrontierTeamMemberWidget>(GetOwningPlayer(), WidgetClass);
		if (!MemberWidget)
		{
			continue;
		}

		MemberWidget->SetObservedPlayerState(TeamMember);
		TeamMembersBox->AddChildToVerticalBox(MemberWidget);
		MemberWidgets.Add(MemberWidget);
	}
}

void UFrontierTeamStatusWidget::BuildWidgetTreeIfNeeded()
{
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	TeamMembersBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("TeamMembersBox"));
	WidgetTree->RootWidget = TeamMembersBox;
}

bool UFrontierTeamStatusWidget::NeedsRebuild(const TArray<AFrontierPlayerState*>& TeamMembers) const
{
	if (MemberWidgets.Num() != TeamMembers.Num())
	{
		return true;
	}

	for (int32 Index = 0; Index < TeamMembers.Num(); ++Index)
	{
		if (!MemberWidgets.IsValidIndex(Index) || !MemberWidgets[Index] || MemberWidgets[Index]->GetObservedPlayerState() != TeamMembers[Index])
		{
			return true;
		}
	}

	return false;
}
