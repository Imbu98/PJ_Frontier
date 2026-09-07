#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "FrontierWhirlwindDamageWindowInterface.generated.h"

UINTERFACE(MinimalAPI)
class UFrontierWhirlwindDamageWindowInterface : public UInterface
{
	GENERATED_BODY()
};

class FRONTIER_API IFrontierWhirlwindDamageWindowInterface
{
	GENERATED_BODY()

public:
	virtual void BeginWhirlwindDamageWindow(float WindowDuration) = 0;
	virtual void EndWhirlwindDamageWindow() = 0;
	virtual float ResolveWhirlwindVisualRadius(const UObject* WorldContextObject, int32 AbilityLevel) const = 0;
};
