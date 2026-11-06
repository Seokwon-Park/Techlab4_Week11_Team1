#pragma once

#include "MovementComponent.h"
#include <Math/Vector.h>
#include "PawnMovementComponent.generated.h"

class APawn;

UCLASS()
class UPawnMovementComponent : public UMovementComponent
{
	GENERATED_BODY()

public:
	virtual void InitializeComponent() override;
	APawn* GetPawnOwner() const { return PawnOwner; }

protected:
	APawn* PawnOwner = nullptr;
};
