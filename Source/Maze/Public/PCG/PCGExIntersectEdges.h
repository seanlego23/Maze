// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "PCGExOctree.h"
#include "Core/PCGExClustersProcessor.h"

#include "PCGExIntersectEdges.generated.h"

#define PCGEX_TARGET_CLUSTER_BATCH_PROCESSING(_STATE) if (!Context->ProcessTargetClusters(_STATE)) { return false; }

#define PCGEX_ELEMENT_BATCH_TARGET_EDGE_DECL virtual TSharedPtr<PCGExClusterMT::IBatch> CreateTargetEdgeBatchInstance(const TSharedRef<PCGExData::FPointIO>& InVtx, TArrayView<TSharedRef<PCGExData::FPointIO>> InEdges) const override;
#define PCGEX_ELEMENT_BATCH_TARGET_EDGE_IMPL(_CLASS) TSharedPtr<PCGExClusterMT::IBatch> FPCGEx##_CLASS##Context::CreateTargetEdgeBatchInstance(const TSharedRef<PCGExData::FPointIO>& InVtx, TArrayView<TSharedRef<PCGExData::FPointIO>> InEdges) const{ \
return MakeShared<PCGExClusterMT::TBatch<PCGEx##_CLASS::FTargetProcessor>>(const_cast<FPCGEx##_CLASS##Context*>(this), InVtx, InEdges); }
#define PCGEX_ELEMENT_BATCH_TARGET_EDGE_IMPL_ADV(_CLASS) TSharedPtr<PCGExClusterMT::IBatch> FPCGEx##_CLASS##Context::CreateTargetEdgeBatchInstance(const TSharedRef<PCGExData::FPointIO>& InVtx, TArrayView<TSharedRef<PCGExData::FPointIO>> InEdges) const{ \
return MakeShared<PCGEx##_CLASS::FTargetBatch>(const_cast<FPCGEx##_CLASS##Context*>(this), InVtx, InEdges); }

namespace PCGExIntersectEdges
{
class FTargetProcessor;
}

namespace PCGExIntersectEdgesLabels
{

const FName TargetVtxLabel = TEXT("Vtx");
const FName TargetEdgesLabel = TEXT("TargetEdges");

}

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category = "PCGEx|Clusters")
class UPCGExIntersectEdgesSettings : public UPCGExClustersProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(IntersectEdges, "Cluster : Intersect Edges", "Finds Edge/Edge intersections between source and target edges.");
	virtual FLinearColor GetNodeTitleColor() const override
	{
		return PCGEX_NODE_COLOR_NAME(ClusterOp);
	}
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UPCGExClustersProcessorSettings interface
public:
	virtual PCGExData::EIOInit GetMainOutputInitMode() const override;
	virtual PCGExData::EIOInit GetEdgeOutputInitMode() const override;
	//~End UPCGExClustersProcessorSettings interface

	/** If disabled, edges will only be checked against target datasets. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bEnableSelfIntersection = true;
};

struct FPCGExIntersectEdgesContext final : FPCGExClustersProcessorContext
{
	friend class UPCGExIntersectEdgesSettings;
	friend class FPCGExIntersectEdgesElement;
	friend class PCGExIntersectEdges::FTargetProcessor;

	TSharedPtr<PCGExData::FPointIOCollection> TargetPoints;
	TSharedPtr<PCGExData::FPointIOCollection> TargetEdges;
	TSharedPtr<PCGExClusters::FDataLibrary> TargetClusterDataLibrary;
	TSharedPtr<PCGExData::FPointIOTaggedEntries> TaggedTargetEdges;

	TSharedPtr<PCGExData::FPointIO> CurrentTargetIO;

protected:

	virtual TSharedPtr<PCGExClusterMT::IBatch> CreateTargetEdgeBatchInstance(const TSharedRef<PCGExData::FPointIO>& InVtx, TArrayView<TSharedRef<PCGExData::FPointIO>> InEdges) const;

public:

	virtual bool AdvanceTargetPointsIO(const bool bCleanupKeys = true);

	bool ProcessTargetClusters(const PCGExCommon::ContextState NextStateId);

protected:

	TArray<TSharedPtr<PCGExClusterMT::IBatch>> TargetBatches;
	TArray<TSharedRef<PCGExData::FFacade>> TargetEdgesDataFacades;

	TSharedPtr<PCGExOctree::FItemOctree> TargetEdgeOctree;
	FBox TargetEdgeOctreeBounds;

	int32 CurrentTargetPointIOIndex = -1;

	bool bSkipTargetClusterBatchCompletionStep = false;
	bool bDoTargetClusterBatchWritingStep = false;

	bool StartProcessingTargetClusters(FBatchProcessingValidateEntries&& ValidateEntries, FBatchProcessingInitEdgeBatch&& InitBatch);

	virtual void TargetClusterProcessing_InitialProcessingDone();
	virtual void TargetClusterProcessing_WorkComplete();
	virtual void TargetClusterProcessing_WritingDone();
	virtual void TargetClusterProcessing_GraphCompilationDone();
};

class FPCGExIntersectEdgesElement final : public FPCGExClustersProcessorElement
{
protected:
	PCGEX_ELEMENT_CREATE_CONTEXT(IntersectEdges)

	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const override;
};

namespace PCGExIntersectEdges
{

namespace States
{

PCGEX_CTX_STATE(MTState_TargetClusterProcessing)
PCGEX_CTX_STATE(MTState_TargetClusterCompletingWork)
PCGEX_CTX_STATE(MTState_TargetClusterWriting)

PCGEX_CTX_STATE(State_TargetGraphDone)

}

class FTargetProcessor final : public PCGExClusterMT::TProcessor<FPCGExIntersectEdgesContext, UPCGExIntersectEdgesSettings>
{
	int32 VtxIOIndex = 0;
	int32 EdgesIOIndex = 0;
	TArray<PCGExGraphs::FEdge> IndexedEdges;

public:
	bool bInvalidEdges = true;

	explicit FTargetProcessor(const TSharedRef<PCGExData::FFacade>& InVtxDataFacade, const TSharedRef<PCGExData::FFacade>& InEdgeDataFacade)
		: TProcessor(InVtxDataFacade, InEdgeDataFacade)
	{
	}

	virtual ~FTargetProcessor() override;

	virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager>& InTaskManager) override;

	virtual void CompleteWork() override;
	virtual void Write() override;

};
}
