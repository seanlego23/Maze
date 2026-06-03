#include "PCG/PCGExProbeClosest2D.h"

#include "PCGExH.h"
#include "Containers/PCGExManagedObjects.h"
#include "Details/PCGExSettingsDetails.h"
#include "Core/PCGExProbingCandidates.h"

PCGEX_SETTING_VALUE_IMPL(FPCGExProbeConfigClosest2D, MaxConnections, int32, MaxConnectionsInput, MaxConnectionsAttribute, MaxConnectionsConstant)
PCGEX_CREATE_PROBE_FACTORY(Closest2D, {}, {})

bool FPCGExProbeClosest2D::Prepare(FPCGExContext* InContext)
{
	if (!FPCGExProbeOperation::Prepare(InContext))
	{
		return false;
	}

	MaxConnections = Config.GetValueSettingMaxConnections();
	if (!MaxConnections->Init(PrimaryDataFacade))
	{
		return false;
	}

	PlaneNormal = Config.PlaneNormal.GetSafeNormal();
	LowerBoundSinAngle = FMath::Sin(FMath::DegreesToRadians(Config.LowerBoundAngle));
	UpperBoundSinAngle = FMath::Sin(FMath::DegreesToRadians(Config.UpperBoundAngle));

	CWCoincidenceTolerance = FVector(PCGEx::SafeScalarTolerance(Config.CoincidencePreventionTolerance));

	return true;
}

void FPCGExProbeClosest2D::ProcessCandidates(const int32 Index, TArray<PCGExProbing::FCandidate>& Candidates, TSet<uint64>* Coincidence, const FVector& ST, TSet<uint64>* OutEdges, PCGExMT::FScopedContainer* Container)
{
	bool bIsAlreadyConnected;
	const int32 MaxIterations = FMath::Min(MaxConnections->Read(Index), Candidates.Num());
	const double R = GetSearchRadius(Index);

	if (MaxIterations <= 0)
	{
		return;
	}

	TSet<uint64> LocalCoincidence;
	int32 Additions = 0;

	for (PCGExProbing::FCandidate& C : Candidates)
	{
		if (C.Distance > R)
		{
			return;
		} // Candidates are sorted, stop there.

		const float Dot = C.Direction.Dot(PlaneNormal);
		if ((Dot < LowerBoundSinAngle && !FMath::IsNearlyEqual(Dot, LowerBoundSinAngle)) || 
			(Dot > UpperBoundSinAngle && !FMath::IsNearlyEqual(Dot, UpperBoundSinAngle)))
		{
			continue;
		}

		if (Coincidence)
		{
			Coincidence->Add(C.GH, &bIsAlreadyConnected);
			if (bIsAlreadyConnected)
			{
				continue;
			}
		}

		if (Config.bPreventCoincidence)
		{
			LocalCoincidence.Add(PCGEx::SH3(C.Direction, CWCoincidenceTolerance), &bIsAlreadyConnected);
			if (bIsAlreadyConnected)
			{
				continue;
			}
		}

		OutEdges->Add(PCGEx::H64U(Index, C.PointIndex));

		Additions++;
		if (Additions >= MaxIterations)
		{
			return;
		}
	}
}

#if WITH_EDITOR
FString UPCGExProbeClosest2DProviderSettings::GetDisplayName() const
{
	return TEXT("");
	/*
	return GetDefaultNodeName().ToString()
		+ TEXT(" @ ")
		+ FString::Printf(TEXT("%.3f"), (static_cast<int32>(1000 * Config.WeightFactor) / 1000.0));
		*/
}
#endif