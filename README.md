# EternalDreams

> Unreal Engine 5.5 기반 GAS 멀티플레이 팀전 배틀로얄 액션 게임

**[▶ 시연 영상]([Eternal Dreams Demo Video - YouTube](https://www.youtube.com/watch?v=TQSDvPFhwYk&feature=youtu.be))** 

---

## 프로젝트 개요

|             |                                                                                  |
| ----------- | -------------------------------------------------------------------------------- |
| **기간**      | 2026.04.01 ~ 2026.04.24 (1개월)                                                          |
| **팀 구성**    | 6명 (프로그래머 6)                                                                     |
| **본인 역할**   | 클라이언트 — 플레이어 GAS 전투 파이프라인 · 다무기 스킬/회피 어빌리티 · 메시 비동기 리플리케이션 · SkillComponent · UI |
| **엔진 / 언어** | Unreal Engine 5.5 · C++                                                          |
| **주요 기술**   | GAS, GameplayTags, EnhancedInput, CommonUI, UMG, Primary Data Asset, Niagara     |

> 팀 프로젝트입니다. 본 레포는 원본 저장소의 포크본이며, 아래 "본인 구현 범위"에 명시한 부분이 직접 작성한 코드입니다.

---

## 본인 구현 범위

**[전체 커밋 보기](https://github.com/khs081215/7th-Team1-CH4-Project/commits/develop/?author=khs081215)**

| 영역                                                                                                  | 주요 파일                                                                      |
| --------------------------------------------------------------------------------------------------- | -------------------------------------------------------------------------- |
| 플레이어 GAS 전투 파이프라인 (GA_Base · GA_BasicAttack · 회피 · 스킬 3종)                                           | `Source/EternalDreams/Public/Characters/Player/GAS/`                       |
| SkillComponent (4슬롯 태그 관리 · 서버 RPC · SRP 설계)                                                        | `Source/EternalDreams/Public/Characters/Player/Component/SkillComponent.h` |
| AnimNotify–GAS 히트 판정 브리지 (ANS_AttackTrace · ANS_AttackMove · AN_ProjectileAttack · AN_TeleportMove) | `Source/EternalDreams/Public/Characters/Player/AnimNotify/`                |
| 메시 비동기 리플리케이션 + 핸드셰이크 게이트                                                                           | `Source/EternalDreams/Private/Characters/Player/EDPlayerCharacter.cpp`     |
| 스킬 데미지 배수 테이블 · ExecCalc 연동                                                                         | `Source/EternalDreams/Public/Characters/Base/GAS/ExecCalc_Damage.h`        |
| UI — 스킬 쿨타임 · 팀/적 오버레이 매트 · 출혈 HUD · 로딩 화면 · 스킨 선택 · 스킬 사운드                                         | `Source/EternalDreams/Public/UI/HUD/`                                      |
| Native GameplayTags 싱글톤                                                                             | `Source/EternalDreams/Public/Data/GameplayTag/EDGameplayTags.h`            |
| 독립 CameraActor (줌인/줌아웃 · 엣지 스크롤 · 포커스 토글)                                                           | `Source/EternalDreams/Public/Characters/Player/OtherActor/`                |

---

## 핵심 구현

### 1. 멀티플레이 무기·캐릭터 메시 동기화 재설계

**문제**
무거운 메시 에셋을 직접 복제하거나 Multicast로 뿌리던 방식이어서, 클라이언트별 로드 완료 시점이 달라 수신 시점에 에셋이 미로드 상태이면 무기·캐릭터가 보이지 않는 동기화 실패가 간헐적으로 발생.

**해결

**
복제 단위를 무거운 에셋 참조에서 가벼운 `FPrimaryAssetId`로 바꾸고(`Multicast` → `OnRep`), 각 클라이언트가 게임 시작 시 프리로드해둔 데이터에서 ID로 조회·적용하도록 전환했다. 나아가 서버와 전원 클라이언트의 로드 완료를 `GameMode`가 집계한 뒤 일괄 적용하는 핸드셰이크 게이트를 추가해, 모든 피어가 준비된 시점에만 메시를 세팅하도록 타이밍을 통일했다.

**설계 판단**

Multicast 유지 + 재전송 재시도 방식도 검토했으나, 타이밍 관리가 복잡하여 지양하였다. `FPrimaryAssetId` 복제는 페이로드가 수 바이트에 불과하고, 프리로드 타이밍은 로비→인게임 전환 시점으로 이미 확보된 슬롯이어서 추가 비용이 없었다.

**결과**
클라이언트별 로드 타이밍 불일치로 인한 간헐적 메시 미표시 현상을 해소했다.

---

### 2. AnimNotify–GAS 히트 판정 브리지 설계

**문제**
무기 4종(검·해머·활·스태프)마다 일반공격 방식이 다르고, 스킬은 무기에 귀속되지 않고 파밍으로 획득되어 조합이 독립적인 구조였다. 근접 판정 로직을 각 어빌리티에 직접 넣으면 어빌리티 수가 늘수록 판정 코드가 중복되고, 타격 타이밍이 애니메이션에 묶여 수정이 번거로워진다.

**해결**
`AnimNotifyState`의 `NotifyTick`에서 `SphereTraceMulti`로 히트 판정을 수행하고, `HasAuthority` 가드 + 콤보 내 중복 타격 방지를 적용했다. 판정 결과는 `HandleGameplayEvent(Event.SkillHit)`로 발행하고, GA가 `WaitGameplayEvent`로 구독해 팀 ID 검증 후 데미지 GE를 적용하는 구조로 판정과 어빌리티 로직을 분리했다.

**설계 판단**
GameplayEvent를 경유하면 ANS는 "히트를 알린다"는 단일 책임만 가지고, GA는 이벤트 수신 후 자유롭게 후처리를 확장할 수 있다. 원거리(활·스태프)는 발사체 착탄 시 판정 경로가 분리되나, 데미지 계산은 동일한 `ExecCalc_Damage`를 공유했다.

**결과**
신규 근접 스킬 추가 시 몽타주에 ANS를 배치하는 것만으로 판정 코드를 재사용할 수 있다.

```cpp
// ANS_AttackTrace::NotifyTick — 서버 전용 히트 감지 후 GAS 이벤트 발행
if (!Owner->HasAuthority()) return;

TArray<FHitResult> HitResults;
UKismetSystemLibrary::SphereTraceMulti(..., HitResults);

for (const FHitResult& Hit : HitResults)
{
    if (Owner->HittedCharacterArray.Contains(Hit.GetActor())) continue;
    Owner->HittedCharacterArray.Add(Hit.GetActor());

    FGameplayEventData Payload;
    Payload.Target = Hit.GetActor();
    UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(
        Owner, FEDGameplayTags::Get().Event_SkillHit, Payload);
}
```

---

## 빌드 및 실행

```
엔진 버전 : Unreal Engine 5.5
필요 항목 : Visual Studio 2022 / Git LFS
```

1. 레포를 클론합니다.
2. `EternalDreams.uproject` 우클릭 → Generate Visual Studio project files
3. `EternalDreams.sln` 빌드 (`EternalDreamsEditor | Win64 | Development`) 후 에디터 실행

---

## 참고

- [원본 팀 저장소](https://github.com/NbcampUnreal/7th-Team1-CH4-Project)
