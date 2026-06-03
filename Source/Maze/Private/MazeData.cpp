#include "MazeData.h"

#if WITH_EDITOR
void AMazeData::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (PropertyChangedEvent.GetMemberPropertyName() == GET_MEMBER_NAME_CHECKED(AMazeData, Graphs) &&
		PropertyChangedEvent.ChangeType == EPropertyChangeType::ArrayAdd)
	{
		Graphs.Last() = NewObject<UPCGGraphInstance>(this);
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif