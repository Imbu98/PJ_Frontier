#pragma once

#include "CoreMinimal.h"
#include "Inventory/FrontierInventoryTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "FrontierItemCatalogSubsystem.generated.h"

class UDataTable;
class UFrontierItemDataAsset;

UCLASS()
class FRONTIER_API UFrontierItemCatalogSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	const FFrontierResolvedItemTemplateData* ResolveItemTemplateData(FName ItemTemplateId) const;

	static FFrontierResolvedItemTemplateData BuildTemplateDataFromItemData(const UFrontierItemDataAsset& ItemData);

private:
	void RebuildCatalog();
	void LoadWeaponTemplateRows();
	void LoadArmorTemplateRows();
	void LoadConsumableTemplateRows();
	void LoadMiscellaneousTemplateRows();
	void LoadAccessoryTemplateRows();
	bool AddResolvedTemplate(FName ItemTemplateId, FFrontierResolvedItemTemplateData&& TemplateData, const UDataTable* SourceTable);

	UPROPERTY(Transient)
	TArray<TObjectPtr<UDataTable>> LoadedTemplateDataTables;

	UPROPERTY(Transient)
	TMap<FName, FFrontierResolvedItemTemplateData> ItemTemplateCatalog;
};
