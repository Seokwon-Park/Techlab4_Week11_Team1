#pragma once

#include "UObject/Object.h"
#include "AnimNodeBase.h"
#include "AnimInstance.generated.h"

UCLASS()
class UAnimInstance : public UObject
{
	GENERATED_BODY()

public:
	virtual void NativeInitializeAnimation();               // 그래프를 만든다 (한 번)
	virtual void NativeUpdateAnimation(float DeltaSeconds); // 변수를 갱신한다 (매 프레임)

protected:
	FAnimNode_Base* RootNode = nullptr;
};

