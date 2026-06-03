#pragma once

#include "PCGGraph.h"

#include "CoreMinimal.h"
#include "GameFramework/Info.h"
#include "MazeData.generated.h"

UCLASS(Blueprintable)
class AMazeData : public AInfo
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere)
	TArray<TObjectPtr<UPCGGraphInstance>> Graphs;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};