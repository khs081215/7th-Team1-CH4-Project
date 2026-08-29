// Fill out your copyright notice in the Description page of Project Settings.
#include "Characters/Player/GAS/GameplayAbility/GA_Base.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Characters/Player/EDPlayerCharacter.h"
#include "Characters/Player/GAS/EDPlayerAttributeSet.h"
#include "Core/EDGameDataSubsystem.h"
#include "Core/EDPlayerState.h"
#include "Core/EDSkillDataSubsystem.h"
#include "Data/EDPlayerAnimDataAsset.h"
#include "Data/GameplayTag/EDGameplayTags.h"

UGA_Base::UGA_Base()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

bool UGA_Base::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
                                  const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags,
                                  FGameplayTagContainer* OptionalRelevantTags) const
{
	return Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags);
}

void UGA_Base::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
                               const FGameplayAbilityActivationInfo ActivationInfo,
                               const FGameplayEventData* TriggerEventData)
{
	//Commit
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		//Commit Failed
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	//Get ASC
	UAbilitySystemComponent* PlayerASC = GetAbilitySystemComponentFromActorInfo();
	if (!PlayerASC)
	{
		return;
	}
	//Get AttributeSet
	const UEDPlayerAttributeSet* PlayerAttributeSet = Cast<UEDPlayerAttributeSet>(PlayerASC->GetAttributeSet(UEDPlayerAttributeSet::StaticClass()));
	if (!IsValid(PlayerAttributeSet))
	{
		return;
	}
	
	//Get AnimMontage

	UEDGameDataSubsystem* EDGameDataSubsystem = UEDGameDataSubsystem::Get(GetWorld());
	if (IsValid(EDGameDataSubsystem))
	{
		UEDPlayerAnimDataAsset* PlayerAnimData = EDGameDataSubsystem->GetData<UEDPlayerAnimDataAsset>(
			FPrimaryAssetId(
				*StaticEnum<EPlayerDataType>()->GetNameStringByValue((int64)EPlayerDataType::PlayerAnimData),
				*StaticEnum<EPlayerAnimNameType>()->GetNameStringByValue((int64)MontageName)
			));

		
		if (IsValid(PlayerAnimData))
		{
			UE_LOG(LogTemp,Warning,TEXT("PlayerAnimData Exist"));
			
			
			// 1. 경로 자체가 비어있는지 확인
			UE_LOG(LogTemp, Log, TEXT("Path: %s"), *PlayerAnimData->AnimMontage.ToString());
    
			// 2. 이미 로드된 상태인지(IsPending) 확인
			bool bIsLoaded = PlayerAnimData->AnimMontage.IsPending() == false;
			UE_LOG(LogTemp, Log, TEXT("Is Loaded: %s"), bIsLoaded ? TEXT("True") : TEXT("False"));

			PlayerAnimMontage = PlayerAnimData->AnimMontage.LoadSynchronous();
			if (IsValid(PlayerAnimMontage))
			{
				UE_LOG(LogTemp,Warning,TEXT("PlayerAnimMontage Valid"));
			}
		}
		else
		{
			{
				UE_LOG(LogTemp,Warning,TEXT("PlayerAnimData Not"));
			}
		}
		
	}



	
	//Play Montage Task(비동기)
	UAbilityTask_PlayMontageAndWait* PlayMontageTask =
		UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
			this,
			NAME_None,
			PlayerAnimMontage,
			PlayerAttributeSet->GetAttackSpeed()
		);
	if (!PlayMontageTask)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	//Bind Delegate
	PlayMontageTask->OnCompleted.AddDynamic(this, &UGA_Base::OnMontageCompleted);
	PlayMontageTask->OnCancelled.AddDynamic(this, &UGA_Base::OnMontageCancelled);
	PlayMontageTask->OnInterrupted.AddDynamic(this, &UGA_Base::OnMontageCancelled);
	// Task 활성화
	PlayMontageTask->ReadyForActivation();
	
	//Anim Notify, 혹은 Projectile 에서 히트 판정이 들어왔을 경우 바인딩
	UAbilityTask_WaitGameplayEvent* WaitEventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this, FEDGameplayTags::Get().Event_SkillHit, nullptr, false, false);

	WaitEventTask->EventReceived.AddDynamic(this, &UGA_Base::OnNotifyHitEvent);
	WaitEventTask->ReadyForActivation();

	//GE_CoolDown 적용
	if (CoolTimeEffectClass == nullptr || CoolTime == 0.f)
	{
		return;
	}
	FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(CoolTimeEffectClass, GetAbilityLevel());
	if (SpecHandle.IsValid())
	{
		SpecHandle.Data.Get()->SetSetByCallerMagnitude(FEDGameplayTags::Get().Data_CoolTime, CoolTime);

		SpecHandle.Data.Get()->DynamicGrantedTags.AddTag(CoolTimeTag);

		ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, SpecHandle);
	}
}

void UGA_Base::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
                          const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility,
                          bool bWasCancelled)
{
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGA_Base::OnMontageCompleted()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_Base::OnMontageCancelled()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UGA_Base::OnNotifyHitEvent(FGameplayEventData HitGameplayEventData)
{
	//맞은 적의 ASI, ASC를 가져온다.
	AActor* HittedActor = const_cast<AActor*>(HitGameplayEventData.Target.Get());
	if (!IsValid(HittedActor))
	{
		return;
	}
	
	AEDPlayerCharacter* HittedPlayer=Cast<AEDPlayerCharacter>(HittedActor);
	AEDPlayerCharacter* AttackedPlayer=Cast<AEDPlayerCharacter>(GetAvatarActorFromActorInfo());
	if (HittedPlayer&&AttackedPlayer&&HittedPlayer->GetController()&&AttackedPlayer->GetController())
	{
		AEDPlayerState* HittedPlayerState=HittedPlayer->GetController()->GetPlayerState<AEDPlayerState>();
		AEDPlayerState* AttackedPlayerState=AttackedPlayer->GetController()->GetPlayerState<AEDPlayerState>();
		if (HittedPlayerState&&AttackedPlayerState)
		{
			//같은 팀인 경우 GE 적용하지 않음
			if (HittedPlayerState->TeamId==AttackedPlayerState->TeamId)
			{
				return;
			}
		}
		
	}
	

	
	


	IAbilitySystemInterface* TargetASI = Cast<IAbilitySystemInterface>(HittedActor);
	if (TargetASI == nullptr)
	{
		return;
	}
	UAbilitySystemComponent* TargetASC = TargetASI->GetAbilitySystemComponent();
	if (TargetASC == nullptr)
	{
		return;
	}

	UAbilitySystemComponent* PlayerASC = GetAbilitySystemComponentFromActorInfo();
	if (!PlayerASC)
	{
		return;
	}

	FGameplayEffectContextHandle Context = PlayerASC->MakeEffectContext();
	Context.AddSourceObject(GetAvatarActorFromActorInfo()); // 소스 오브젝트는 현재 캐릭터(Avatar)

	FGameplayEffectSpecHandle SpecHandle = PlayerASC->MakeOutgoingSpec(DamageEffectClass, 1.0f, Context);

	const UEDPlayerAttributeSet* PlayerAttributeSet = Cast<UEDPlayerAttributeSet>(PlayerASC->GetAttributeSet(UEDPlayerAttributeSet::StaticClass()));
	if (SpecHandle.IsValid() && IsValid(PlayerAttributeSet))
	{
		//AssetTag 로 검색
		const FGameplayTagContainer& AssetTags=GetAssetTags();
		FGameplayTag AssetTag=AssetTags.GetByIndex(0);
		
		const UEDSkillDataSubsystem* EDSkillDataSubsystem=UEDSkillDataSubsystem::Get(GetWorld());
		
		if (AssetTag==FGameplayTag::EmptyTag||!IsValid(EDSkillDataSubsystem))
		{
			return;
		}
		//데미지 배율
		const FSkillMulStatus* SkillMulStaus =EDSkillDataSubsystem->GetSkillData(AssetTag);
		
		if (SkillMulStaus==nullptr)
		{
			return;
		}
		
		//공격자의 총합 데미지
		float SkillFinalDamage =
			PlayerAttributeSet->GetStrength() * SkillMulStaus->DamageStrengthMultiplier +
			PlayerAttributeSet->GetDexterity() * SkillMulStaus->DamageDexterityMultiplier +
			PlayerAttributeSet->GetIntelligence() * SkillMulStaus->DamageIntelligenceMultiplier
		;

		SpecHandle.Data->SetSetByCallerMagnitude(FEDGameplayTags::Get().Data_Damage, SkillFinalDamage);
		PlayerASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);
	}
	
	//디버프 GE
	for (auto DebuffEffectClass: DebuffEffectClassMap)
	{
		FGameplayEffectSpecHandle DebuffSpecHandle = PlayerASC->MakeOutgoingSpec(DebuffEffectClass.Key, 1.0f, Context);
		if (DebuffSpecHandle.IsValid())
		{
			DebuffSpecHandle.Data->SetSetByCallerMagnitude(FEDGameplayTags::Get().Data_DebuffTime, DebuffEffectClass.Value);
			PlayerASC->ApplyGameplayEffectSpecToTarget(*DebuffSpecHandle.Data.Get(), TargetASC);
		}
		
	}
	
	
}
