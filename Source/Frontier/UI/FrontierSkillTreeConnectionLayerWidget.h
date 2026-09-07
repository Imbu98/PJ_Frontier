#pragma once

#include "CoreMinimal.h"
#include "Components/Widget.h"
#include "FrontierSkillTreeConnectionLayerWidget.generated.h"

class SFrontierSkillTreeConnectionLayer;
class UFrontierSkillTreeWidget;

/** Runtime-only paint layer inserted behind the authored skill-tree nodes. */
UCLASS()
class FRONTIER_API UFrontierSkillTreeConnectionLayerWidget : public UWidget
{
	GENERATED_BODY()

public:
	void InitializeConnectionLayer(UFrontierSkillTreeWidget* InOwnerSkillTreeWidget);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

private:
	TWeakObjectPtr<UFrontierSkillTreeWidget> OwnerSkillTreeWidget;
	TSharedPtr<SFrontierSkillTreeConnectionLayer> MyConnectionLayer;
};
