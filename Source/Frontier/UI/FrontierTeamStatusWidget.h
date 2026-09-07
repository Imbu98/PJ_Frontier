#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FrontierTeamStatusWidget.generated.h"

class AFrontierPlayerState;
class UFrontierTeamMemberWidget;
class UVerticalBox;

UCLASS()
class FRONTIER_API UFrontierTeamStatusWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	void RefreshTeamMembers();

protected:
	UPROPERTY(EditDefaultsOnly, Category="Frontier|Team")
	TSubclassOf<UFrontierTeamMemberWidget> TeamMemberWidgetClass;

private:
	void BuildWidgetTreeIfNeeded();
	bool NeedsRebuild(const TArray<AFrontierPlayerState*>& TeamMembers) const;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UVerticalBox> TeamMembersBox;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UFrontierTeamMemberWidget>> MemberWidgets;

	float RefreshAccumulator = 0.0f;
};
