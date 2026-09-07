#include "Combat/FrontierNumberPopComponent.h"

UFrontierNumberPopComponent::UFrontierNumberPopComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UFrontierNumberPopComponent::AddNumberPop(const FFrontierNumberPopRequest& NewRequest)
{
}
