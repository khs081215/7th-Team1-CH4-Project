// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "ANS_AttackTrace.generated.h"

class UGameplayEffect;
class UAbilitySystemComponent;
class IAbilitySystemInterface;
class AEDPlayerCharacter;
class AEDWeapon;
/**
 * 
 */
UCLASS()
class ETERNALDREAMS_API UANS_AttackTrace : public UAnimNotifyState
{
	GENERATED_BODY()
	UANS_AttackTrace();

	
public:
	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	
protected:
	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Player|Attack")
	float TraceRadius=50.0f;
	
	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Player|Attack")
	bool bShowDebug=false;
	
	//XY방향으로 날리는 힘
	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Player|Attack")
	float LaunchPowerXY=0.f;
	//Z방향으로 날리는 힘
	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Player|Attack")
	float LaunchPowerZ=0.f;

	
private:

	//소켓이름
	FName SocketName=FName("Socket");

	
	
	
	
};
