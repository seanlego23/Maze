// Fill out your copyright notice in the Description page of Project Settings.

#include "PCG/PCGExIntersectEdges.h"

#include "Clusters/PCGExCluster.h"
#include "Clusters/PCGExClusterDataLibrary.h"
#include "Clusters/PCGExClustersHelpers.h"
#include "Data/PCGExData.h"
#include "Data/PCGExPointIO.h"

#define LOCTEXT_NAMESPACE "PCGExGraphSettings"

#pragma region UPCGSettings interface

TArray<FPCGPinProperties> UPCGExIntersectEdgesSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();

	if (bEnableSelfIntersection)
	{
		PCGEX_PIN_POINTS(PCGExIntersectEdgesLabels::TargetVtxLabel, "The target point data to be processed.", Normal)
		PCGEX_PIN_POINTS(PCGExIntersectEdgesLabels::TargetEdgesLabel, "Target Edges associated with the main input points that the main Edges test against.", Normal)
	}
	else
	{
		PCGEX_PIN_POINTS(PCGExIntersectEdgesLabels::TargetVtxLabel, "The target point data to be processed.", Required)
		PCGEX_PIN_POINTS(PCGExIntersectEdgesLabels::TargetEdgesLabel, "Target Edges associated with the main input points that the main Edges test against.", Required)
	}

	return PinProperties;
}

PCGExData::EIOInit UPCGExIntersectEdgesSettings::GetMainOutputInitMode() const { return PCGExData::EIOInit::Forward; }
PCGExData::EIOInit UPCGExIntersectEdgesSettings::GetEdgeOutputInitMode() const { return PCGExData::EIOInit::NoInit; }

#pragma endregion

#pragma region FPCGContext interface

PCGEX_ELEMENT_BATCH_TARGET_EDGE_IMPL(IntersectEdges)

bool FPCGExIntersectEdgesContext::AdvanceTargetPointsIO(const bool bCleanupKeys)
{
	if (bCleanupKeys && CurrentTargetIO)
	{
		CurrentTargetIO->ClearCachedKeys();
	}

	if (TargetPoints->Pairs.IsValidIndex(++CurrentTargetPointIOIndex))
	{
		CurrentTargetIO = TargetPoints->Pairs[CurrentTargetPointIOIndex];
	}
	else
	{
		CurrentTargetIO = nullptr;
		return false;
	}

	TaggedTargetEdges = TargetClusterDataLibrary->GetAssociatedEdges(CurrentTargetIO);

	if (TaggedTargetEdges && !TaggedTargetEdges->Entries.IsEmpty())
	{
		PCGExDataId OutId;
		PCGExClusters::Helpers::SetClusterVtx(CurrentTargetIO, OutId); // Update key
		PCGExClusters::Helpers::MarkClusterEdges(TaggedTargetEdges->Entries, OutId);
	}
	else
	{
		TaggedTargetEdges = nullptr;
	}

	if (!TaggedTargetEdges && !bQuietMissingClusterPairElement)
	{
		PCGE_LOG_C(Warning, GraphAndLog, this, FTEXT("Some input target vtx have no associated target edges."));
	}

	return true;
}

bool FPCGExIntersectEdgesContext::StartProcessingTargetClusters(FBatchProcessingValidateEntries&& ValidateEntries, FBatchProcessingInitEdgeBatch&& InitBatch)
{
	TargetBatches.Empty();

	TargetBatches.Reserve(TargetPoints->Pairs.Num());

	TargetEdgesDataFacades.Reserve(TargetEdges->Pairs.Num());
	for (const TSharedPtr<PCGExData::FPointIO>& EdgeIO : TargetEdges->Pairs)
	{
		TSharedPtr<PCGExData::FFacade> EdgeFacade = MakeShared<PCGExData::FFacade>(EdgeIO.ToSharedRef());
		TargetEdgesDataFacades.Add(EdgeFacade.ToSharedRef());
	}

	while (AdvanceTargetPointsIO(false))
	{
		if (!TaggedTargetEdges)
		{
			PCGE_LOG_C(Warning, GraphAndLog, this, FTEXT("Some input target points have no bound target edges."));
			continue;
		}

		if (!ValidateEntries(TaggedTargetEdges))
		{
			continue;
		}

		const TSharedPtr<PCGExClusterMT::IBatch> NewBatch = CreateTargetEdgeBatchInstance(CurrentTargetIO.ToSharedRef(), TaggedTargetEdges->Entries);
		InitBatch(NewBatch);

		NewBatch->EdgesDataFacades = &TargetEdgesDataFacades;
		TargetBatches.Add(NewBatch);
	}

	if (TargetBatches.IsEmpty())
	{
		return false;
	}

	bBatchProcessingEnabled = true;

	SetState(PCGExIntersectEdges::States::MTState_TargetClusterProcessing);
	PCGEX_ASYNC_SCHEDULING_SCOPE(GetTaskManager(), true)
	for (const TSharedPtr<PCGExClusterMT::IBatch>& Batch : TargetBatches)
	{
		PCGExClusterMT::ScheduleBatch(GetTaskManager(), Batch, bScopedIndexLookupBuild);
	}

	return false;
}

void FPCGExIntersectEdgesContext::TargetClusterProcessing_InitialProcessingDone()
{
}

void FPCGExIntersectEdgesContext::TargetClusterProcessing_WorkComplete()
{
	TargetEdgeOctree = MakeShared<PCGExOctree::FItemOctree>(TargetEdgeOctreeBounds.GetCenter(), (TargetEdgeOctreeBounds.GetExtent() + FVector(10)).Length());
}

void FPCGExIntersectEdgesContext::TargetClusterProcessing_WritingDone()
{
}

void FPCGExIntersectEdgesContext::TargetClusterProcessing_GraphCompilationDone()
{
}

bool FPCGExIntersectEdgesContext::ProcessTargetClusters(const PCGExCommon::ContextState NextStateId)
{
	PCGEX_ON_ASYNC_STATE_READY_INTERNAL(PCGExIntersectEdges::States::MTState_TargetClusterProcessing)
	{
		TargetClusterProcessing_InitialProcessingDone();
		SetState(PCGExIntersectEdges::States::MTState_TargetClusterCompletingWork);
		if (!bSkipTargetClusterBatchCompletionStep)
		{
			PCGEX_ASYNC_SCHEDULING_SCOPE(TaskManager, true)
			for (const TSharedPtr<PCGExClusterMT::IBatch>& Batch : TargetBatches)
			{
				Batch->CompleteWork();
			}
			return false;
		}
	}

	PCGEX_ON_ASYNC_STATE_READY_INTERNAL(PCGExIntersectEdges::States::MTState_TargetClusterCompletingWork)
	{
		if (!bSkipTargetClusterBatchCompletionStep)
		{
			TargetClusterProcessing_WorkComplete();
		}

		if (bDoTargetClusterBatchWritingStep)
		{
			SetState(PCGExIntersectEdges::States::MTState_TargetClusterWriting);
			PCGEX_ASYNC_SCHEDULING_SCOPE(TaskManager, true)
			for (const TSharedPtr<PCGExClusterMT::IBatch>& Batch : TargetBatches)
			{
				Batch->Write();
			}
			return false;
		}
		bBatchProcessingEnabled = false;
		if (NextStateId == PCGExCommon::States::State_Done)
		{
			Done();
		}
		SetState(NextStateId);
	}

	PCGEX_ON_ASYNC_STATE_READY_INTERNAL(PCGExIntersectEdges::States::MTState_TargetClusterWriting)
	{
		TargetClusterProcessing_WritingDone();

		bBatchProcessingEnabled = false;
		if (NextStateId == PCGExCommon::States::State_Done)
		{
			Done();
		}
		SetState(NextStateId);
	}

	return !IsWaitingForTasks();
}

#pragma endregion

#pragma region FPCGElement interface

PCGEX_INITIALIZE_ELEMENT(IntersectEdges)

bool FPCGExIntersectEdgesElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExClustersProcessorElement::Boot(InContext))
	{
		return false;
	}

	PCGEX_CONTEXT_AND_SETTINGS(IntersectEdges)

	Context->TargetClusterDataLibrary = MakeShared<PCGExClusters::FDataLibrary>(true);

	Context->TargetPoints = MakeShared<PCGExData::FPointIOCollection>(Context);
	TArray<FPCGTaggedData> Sources = Context->InputData.GetInputsByPin(PCGExIntersectEdgesLabels::TargetVtxLabel);
	Context->TargetPoints->Initialize(Sources);

	Context->TargetEdges = MakeShared<PCGExData::FPointIOCollection>(Context);
	TArray<FPCGTaggedData> EdgeSources = Context->InputData.GetInputsByPin(PCGExIntersectEdgesLabels::TargetEdgesLabel);
	Context->TargetEdges->Initialize(EdgeSources);

	if (Settings->bEnableSelfIntersection &&
		Context->TargetPoints->IsEmpty() &&
		Context->TargetEdges->IsEmpty())
	{
		return true;
	}
	else if (!Context->TargetClusterDataLibrary->Build(Context->TargetPoints, Context->TargetEdges))
	{
		Context->TargetClusterDataLibrary->PrintLogs(Context);
		PCGEX_LOG_MISSING_INPUT(Context, FTEXT("Could not find any valid vtx/edge pairs with target edges."))
		return false;
	}

	Context->bDoTargetClusterBatchWritingStep = true;

	return true;
}

bool FPCGExIntersectEdgesElement::AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExIntersectEdgesElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(IntersectEdges)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		if (!Context->StartProcessingTargetClusters([](const TSharedPtr<PCGExData::FPointIOTaggedEntries>& Entries) { return true; }, [&](const TSharedPtr<PCGExClusterMT::IBatch>& NewBatch)
		{
			NewBatch->bRequiresWriteStep = true;
			NewBatch->bForceSingleThreadedCompletion = true;
			NewBatch->bForceSingleThreadedWrite = true;
		}))
		{
			return Context->CancelExecution(TEXT("Could not build any target clusters."));
		}
	}

	PCGEX_TARGET_CLUSTER_BATCH_PROCESSING(PCGExIntersectEdges::States::State_TargetGraphDone)

	return false;
}

#pragma endregion

namespace PCGExIntersectEdges
{

FTargetProcessor::~FTargetProcessor()
{
}

bool FTargetProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InTaskManager)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(PCGExIntersectEdges::Process);

	if (!IProcessor::Process(InTaskManager))
	{
		return false;
	}

	Cluster->RebuildEdgeOctree();

	return true;
}

void FTargetProcessor::CompleteWork()
{
	Context->TargetEdgeOctreeBounds += Cluster->GetEdgeOctree()->GetRootBounds().GetBox();
}

void FTargetProcessor::Write()
{
	Cluster->GetEdgeOctree()->FindAllElements([&](const PCGExOctree::FItem& Item)
	{
		Context->TargetEdgeOctree->AddElement(Item);
	});
}

}

#undef LOCTEXT_NAMESPACE
