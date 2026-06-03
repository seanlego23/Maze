#pragma once

#include "CoreMinimal.h"
#include "Core/PCGExProbeFactoryProvider.h"
#include "Core/PCGExProbeOperation.h"

#include "PCGExProbeClosest2D.generated.h"

namespace PCGExProbing
{
	struct FCandidate;
}

USTRUCT(BlueprintType)
struct FPCGExProbeConfigClosest2D : public FPCGExProbeConfigBase
{
	GENERATED_BODY()

	FPCGExProbeConfigClosest2D()
		: FPCGExProbeConfigBase()
	{}

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExInputValueType MaxConnectionsInput = EPCGExInputValueType::Constant;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName = "Max Connections (Attr)", EditCondition = "MaxConnectionsInput != EPCGExInputValueType::Constant", EditConditionHides))
	FPCGAttributePropertyInputSelector MaxConnectionsAttribute;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName = "Max Connections", ClampMin = 0, EditCondition = "MaxConnectionsInput == EPCGExInputValueType::Constant", EditConditionHides, ClampMin = 0))
	int32 MaxConnectionsConstant = 1;

	PCGEX_SETTING_VALUE_DECL(MaxConnections, int32)

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FVector PlaneNormal = FVector::UpVector;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, ClampMin = "-90.0", ClampMax = "90.0"))
	float LowerBoundAngle = -1.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, ClampMin = "-90.0", ClampMax = "90.0"))
	float UpperBoundAngle = 1.0f;

	/** Attempts to prevent connections that are roughly in the same direction */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bPreventCoincidence = true;

	/** Attempts to prevent connections that are roughly in the same direction */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition = "bPreventCoincidence", ClampMin = 0.00001))
	double CoincidencePreventionTolerance = 0.001;
};

class FPCGExProbeClosest2D : public FPCGExProbeOperation
{
public:
	virtual bool Prepare(FPCGExContext* InContext) override;
	virtual void ProcessCandidates(const int32 Index, TArray<PCGExProbing::FCandidate>& Candidates, TSet<uint64>* Coincidence, const FVector& ST, TSet<uint64>* OutEdges, PCGExMT::FScopedContainer* Container) override;

	FPCGExProbeConfigClosest2D Config;

	TSharedPtr<PCGExDetails::TSettingValue<int32>> MaxConnections;

	FVector PlaneNormal;
	float LowerBoundSinAngle;
	float UpperBoundSinAngle;

protected:
	FVector CWCoincidenceTolerance = FVector::OneVector;
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category = "PCGEx|Data")
class UPCGExProbeFactoryClosest2D : public UPCGExProbeFactoryData
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FPCGExProbeConfigClosest2D Config;

	virtual TSharedPtr<FPCGExProbeOperation> CreateOperation(FPCGExContext* InContext) const override;
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category = "PCGEx|Graph|Params")
class UPCGExProbeClosest2DProviderSettings : public UPCGExProbeFactoryProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(ProbeClosest2D, "Probe : Closests 2D", "Probe in a given Closest 2D plane.", FName(GetDisplayName()))
#endif
	//~End UPCGSettings

	virtual UPCGExFactoryData* CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const override;

		/** Filter Config.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExProbeConfigClosest2D Config;


#if WITH_EDITOR
	virtual FString GetDisplayName() const override;
#endif
};