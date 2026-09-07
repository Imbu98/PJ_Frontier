#include "UI/FrontierSkillTooltipWidget.h"

#include "Abilities/GameplayAbility.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "Skill/FrontierSkillDataSubsystem.h"

void UFrontierSkillTooltipWidget::SetSkillInfo(
	FFrontierSkillInfo InSkillInfo,
	const int32 InSkillLevel,
	const float InCooldownOverride)
{
	CurrentSkillInfo = MoveTemp(InSkillInfo);
	CurrentSkillLevel = FMath::Max(1, InSkillLevel);
	CooldownOverride = InCooldownOverride;
	RefreshSkillTooltip();
}

void UFrontierSkillTooltipWidget::ClearSkillInfo()
{
	CurrentSkillInfo = FFrontierSkillInfo();
	CurrentSkillLevel = 1;
	CooldownOverride = -1.0f;

	if (SkillIcon)
	{
		SkillIcon->SetBrushFromTexture(nullptr);
	}
	if (SkillDescriptionText)
	{
		SkillDescriptionText->SetText(FText::GetEmpty());
	}
	if (SkillNameText)
	{
		SkillNameText->SetText(FText::GetEmpty());
	}
	if (SkillLevelText)
	{
		SkillLevelText->SetText(FText::GetEmpty());
	}
	if (SkillCost)
	{
		SkillCost->SetText(FText::GetEmpty());
	}
	if (SkillCoolTime)
	{
		SkillCoolTime->SetText(FText::GetEmpty());
	}
}

void UFrontierSkillTooltipWidget::NativeConstruct()
{
	Super::NativeConstruct();
	RefreshSkillTooltip();
}

void UFrontierSkillTooltipWidget::RefreshSkillTooltip()
{
	if (SkillIcon)
	{
		SkillIcon->SetBrushFromTexture(CurrentSkillInfo.Icon);
	}
	if (SkillNameText)
	{
		SkillNameText->SetText(CurrentSkillInfo.DisplayName);
	}
	if (SkillLevelText)
	{
		SkillLevelText->SetText(FText::AsNumber(CurrentSkillLevel));
	}
	if (SkillDescriptionText)
	{
		SkillDescriptionText->SetText(BuildFormattedDescription());
	}
	if (SkillCost)
	{
		SkillCost->SetText(FText::AsNumber(CurrentSkillInfo.StaminaCost));
	}
	if (SkillCoolTime)
	{
		const float DisplayCooldown = CooldownOverride >= 0.0f
			? CooldownOverride
			: CurrentSkillInfo.Cooldown;
		SkillCoolTime->SetText(FText::AsNumber(DisplayCooldown));
	}
}

FText UFrontierSkillTooltipWidget::BuildFormattedDescription() const
{
	if (CurrentSkillInfo.Description.IsEmpty() || CurrentSkillInfo.DescriptionParameters.IsEmpty())
	{
		return CurrentSkillInfo.Description;
	}

	TMap<FGameplayTag, float> ResolvedParameters;
	for (const FFrontierSkillDescriptionParameter& Parameter : CurrentSkillInfo.DescriptionParameters)
	{
		if (Parameter.ParameterTag.IsValid())
		{
			ResolvedParameters.Add(Parameter.ParameterTag, Parameter.DefaultValue);
		}
	}

	if (CurrentSkillInfo.AbilityClass)
	{
		if (UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr)
		{
			if (const UFrontierSkillDataSubsystem* SkillDataSubsystem = GameInstance->GetSubsystem<UFrontierSkillDataSubsystem>())
			{
				SkillDataSubsystem->ApplySkillBalance(
					CurrentSkillInfo.AbilityClass,
					CurrentSkillLevel,
					ResolvedParameters);
			}
		}
	}

	FFormatOrderedArguments Arguments;
	Arguments.Reserve(CurrentSkillInfo.DescriptionParameters.Num());
	for (const FFrontierSkillDescriptionParameter& Parameter : CurrentSkillInfo.DescriptionParameters)
	{
		const float* Value = ResolvedParameters.Find(Parameter.ParameterTag);
		Arguments.Add(FormatDescriptionValue(Parameter, Value ? *Value : Parameter.DefaultValue));
	}

	return FText::Format(CurrentSkillInfo.Description, Arguments);
}

FText UFrontierSkillTooltipWidget::FormatDescriptionValue(
	const FFrontierSkillDescriptionParameter& Parameter,
	const float Value) const
{
	float DisplayValue = Value;
	if (Parameter.Format == EFrontierSkillDescriptionValueFormat::Percent)
	{
		DisplayValue *= 100.0f;
	}

	FNumberFormattingOptions NumberOptions;
	NumberOptions.MinimumIntegralDigits = 1;
	NumberOptions.MinimumFractionalDigits = Parameter.DecimalPlaces;
	NumberOptions.MaximumFractionalDigits = Parameter.DecimalPlaces;
	return FText::AsNumber(DisplayValue, &NumberOptions);
}
