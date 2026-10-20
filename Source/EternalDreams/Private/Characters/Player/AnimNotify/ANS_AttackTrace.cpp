// Fill out your copyright notice in the Description page of Project Settings.


#include "Characters/Player/AnimNotify/ANS_AttackTrace.h"

#include "AbilitySystemComponent.h"
#include "GameplayEffectTypes.h"
#include "Characters/Player/EDPlayerCharacter.h"
#include "Characters/Player/Weapon/EDWeapon.h"
#include "Data/GameplayTag/EDGameplayTags.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/KismetSystemLibrary.h"

UANS_AttackTrace::UANS_AttackTrace()
{
}

void UANS_AttackTrace::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration,
                                   const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);
	if (!MeshComp||!MeshComp->GetOwner())
	{
		return;
	}
	if (MeshComp->GetOwner()->HasAuthority()==false)
	{
		return;
	}
	AEDPlayerCharacter* Player=Cast<AEDPlayerCharacter>(MeshComp->GetOwner());
	if (!IsValid(Player)||Player->GetWeaponMeshComp()==nullptr)
	{
		return;
	}
	
	
	//소켓 위치로 변수 초기화
	Player->CurrentAttackSocketLocation = Player->GetWeaponMeshComp()->GetSocketTransform(SocketName, RTS_World).
												  GetLocation();
	Player->PresentAttackSocketLocation = Player->GetWeaponMeshComp()->GetSocketTransform(SocketName, RTS_World).
												  GetLocation();
	
}

void UANS_AttackTrace::NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyTick(MeshComp, Animation, FrameDeltaTime, EventReference);
	if (!MeshComp||!MeshComp->GetOwner())
	{
		return;
	}
	//서버가 아니면 반환
	if (MeshComp->GetOwner()->HasAuthority()==false)
	{
		return;
	}
	AEDPlayerCharacter* Player=Cast<AEDPlayerCharacter>(MeshComp->GetOwner());
	if (!IsValid(Player)||Player->GetWeaponMeshComp()==nullptr||Player->GetAbilitySystemComponent()==nullptr)
	{
		return;
	}
	
	
	Player->CurrentAttackSocketLocation = Player->GetWeaponMeshComp()->GetSocketTransform(SocketName, RTS_World).
												  GetLocation();

	//Sphere Trace
	TArray<FHitResult> HitResults;
	TArray<AActor*> ActorsToIgnore;
	ActorsToIgnore.Add(Player);
	EDrawDebugTrace::Type DebugType = bShowDebug ? EDrawDebugTrace::ForDuration : EDrawDebugTrace::None;

	bool bHit = UKismetSystemLibrary::SphereTraceMulti(
		Player->GetWorld(),
		Player->PresentAttackSocketLocation,
		Player->CurrentAttackSocketLocation,
		TraceRadius,
		//Pawn만 Trace 처리
		UEngineTypes::ConvertToTraceType(ECC_Pawn),
		false, // bTraceComplex
		ActorsToIgnore,
		DebugType,
		HitResults,
		true, // bIgnoreSelf
		FLinearColor::Red, // TraceColor
		FLinearColor::Green, // TraceHitColor
		2.0f // DrawTime
	);
	//이전 소켓의 위치를 갱신
	Player->PresentAttackSocketLocation = Player->CurrentAttackSocketLocation;
	//피격되지 않았으면 Early Return
	if (!bHit)
	{
		return;
	}
	for (const FHitResult& HitResult : HitResults)
	{
		AActor* HittedActor=HitResult.GetActor();
		//1번 공격시 이미 피격당한 목록에 있다면 중복 타격 방지
		if (!HittedActor||Player->HittedCharacterArray.Contains(HittedActor))
		{
			continue;
		}
		
		//피격당한 목록에 추가
		Player->HittedCharacterArray.Add(HittedActor);
		FGameplayEventData HitGameplayEventData;
		
		HitGameplayEventData.Target=HittedActor;
		Player->GetAbilitySystemComponent()->HandleGameplayEvent(FEDGameplayTags::Get().Event_SkillHit,&HitGameplayEventData);
		
		
		//넉백 추가
		ACharacter* HittedCharacter=Cast<ACharacter>(HittedActor);
		if (!IsValid(HittedCharacter))
		{
			continue;
		}
		
		HittedCharacter->GetCharacterMovement()->Velocity=FVector::ZeroVector;
		FVector KnockBackDirection;
		FVector2D KnockBackDirection2D;
		KnockBackDirection=(HittedActor->GetActorLocation()-Player->GetActorLocation());
		KnockBackDirection2D=FVector2D(KnockBackDirection.X,KnockBackDirection.Y);
		//정규화는 XY만 필요할 경우 2D에서 해야 연산량 감소
		KnockBackDirection2D.Normalize();
		KnockBackDirection2D*=LaunchPowerXY;
		KnockBackDirection=FVector(KnockBackDirection2D.X,KnockBackDirection2D.Y,LaunchPowerZ);
		HittedCharacter->LaunchCharacter(KnockBackDirection,true,true);
	}
}

void UANS_AttackTrace::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);
	if (MeshComp->GetOwner()->HasAuthority()==false)
	{
		return;
	}
	AEDPlayerCharacter* Player=Cast<AEDPlayerCharacter>(MeshComp->GetOwner());
	if (IsValid(Player))
	{
		//피격당한 목록을 비워준다.
		Player->HittedCharacterArray.Empty();
	}
}
