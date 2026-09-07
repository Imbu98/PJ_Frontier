###### ============================================================

###### FRONTIER

###### ============================================================

###### 

###### 프론티어는 3인칭 PvPvE 익스트랙션 액션 RPG입니다.

###### 

###### 플레이어는 로비에서 파티와 장비를 준비한 뒤 Dedicated Server 레이드에 입장합니다.

###### 

###### 레이드의 전투와 루팅은 서버가 권위적으로 처리하며, 탈출·사망 결과는 백엔드의 영구 인벤토리와 장비 상태에 반영됩니다.

###### \------------------------------------------------------------

###### 프로젝트 한눈에 보기

###### \------------------------------------------------------------

###### 

###### 장르

###### &#x20; 3인칭 PvPvE 익스트랙션 RPG

###### 

###### 개발 환경

###### &#x20; Unreal Engine 5.7, C++

###### 

###### 네트워크

###### &#x20; Dedicated Server 기반 서버 권위 구조

###### 

###### 주요 담당

###### &#x20; GAS 프레임워크

###### &#x20; 전투 시스템

###### &#x20; 아이템 데이터

###### &#x20; 캐릭터 애니메이션

###### &#x20; 몬스터 전투 AI

###### &#x20; 백엔드 연동

###### 

###### 

###### \------------------------------------------------------------

###### 담당 영역 요약

###### \------------------------------------------------------------

###### 

###### 1\. GAS 프레임워크

###### &#x20; Attribute, Ability, Effect, GameplayCue의 공통 실행 구조

###### &#x20; PlayerState 기반 ASC 수명 관리

###### 

###### &#x20; 주요 기술

###### &#x20; Gameplay Ability System, GameplayTag, Replication

###### 

###### 2\. 전투 시스템

###### &#x20; 기본 공격, 콤보, 근접 Trace, 스킬, 속성 상성

###### &#x20; 서버 권위 데미지 및 피격 연출

###### 

###### &#x20; 주요 기술

###### &#x20; GAS, RPC, AnimNotifyState, Niagara

###### 

###### 3\. 아이템 데이터

###### &#x20; 정적 템플릿과 UUID 기반 런타임 인스턴스 분리

###### &#x20; 장착, 강화, 인벤토리 데이터 흐름

###### 

###### &#x20; 주요 기술

###### &#x20; DataAsset, DataTable, Fast Array

###### 

###### 4\. 캐릭터 애니메이션

###### &#x20; 공통 Locomotion과 무기별 동작 분리

###### 

###### &#x20; 주요 기술

###### &#x20; Animation Layer Interface, Animation Blueprint

###### &#x20; Linked Anim Layer

###### 

###### 5\. 몬스터 전투 AI

###### &#x20; 감지, 추적, 공격, 이탈 및 보스 행동 선택

###### 

###### &#x20; 주요 기술

###### &#x20; StateTree, AI Perception, Navigation

###### 

###### 6\. 백엔드 연동

###### &#x20; 로비 데이터 조회

###### &#x20; 파티 및 매칭 상태 수신

###### &#x20; Dedicated Server 입장과 레이드 결과 연결

###### 

###### &#x20; 주요 기술

###### &#x20; HTTP/JSON, WebSocket, STOMP 1.2, Steam OSS

#### 

#### ============================================================

#### 1\. GAS 기반 개발 프레임워크

#### ============================================================

###### 

###### \[핵심 구현]

###### 

###### &#x20; · 플레이어의 ASC와 AttributeSet을 Pawn이 아닌

###### &#x20;   PlayerState가 소유하도록 구성했습니다.

###### 

###### &#x20; · Possess와 OnRep\_PlayerState() 이후

###### &#x20;   ASC의 Owner·Avatar Actor를 다시 연결하는

###### &#x20;   초기화 경계를 만들었습니다.

###### 

###### &#x20; · 체력, 스태미나, 공격력, 방어력,

###### &#x20;   원소 공격력·저항, 무기별 패시브 수치를

###### &#x20;   AttributeSet으로 통합했습니다.

###### 

###### &#x20; · 공격과 스킬은 GameplayAbility,

###### &#x20;   수치 변경은 GameplayEffect,

###### &#x20;   SFX·VFX는 GameplayCue가 담당하도록 분리했습니다.

###### 

###### &#x20; · 장착 아이템과 스킬트리 수치는

###### &#x20;   SetByCaller GameplayEffect로 Attribute에 반영했습니다.

###### 

###### \[설계 이유]

###### 

###### 일반 멤버 변수와 개별 RPC만으로 구현하면

###### 버프, 장착 효과, 사망 상태, 비용, 쿨다운마다

###### 별도의 규칙이 생깁니다.

###### 

###### GAS를 공통 실행 경계로 사용해

###### 수치 변경, 태그 기반 상태, Ability 수명,

###### 네트워크 적용 규칙을 일관된 방식으로 관리했습니다.

###### 

###### \[실행 흐름]

###### 

###### PlayerState

###### &#x20; ↓

###### ASC + AttributeSet

###### &#x20; ↓

###### Character를 Avatar로 연결

###### &#x20; ↓

###### GameplayAbility 활성화

###### &#x20; ↓

###### GameplayEffect로 수치 변경

###### &#x20; ↓

###### GameplayCue로 SFX·VFX 실행

###### 

###### \[주요 코드]

###### 

###### ASC 소유 및 기본 Ability

###### &#x20; Source/Frontier/Game/FrontierPlayerState.cpp

###### &#x20; - ASC·AttributeSet 생성

###### &#x20; - 시작 Ability 지급

###### &#x20; - Attribute 변경 Delegate

###### 

###### Owner·Avatar 초기화

###### &#x20; Source/Frontier/Character/FrontierBaseCharacter.cpp

###### &#x20; - InitializeAbilityActorInfo()

###### &#x20; - PlayerState 기반 ASC 탐색

###### 

###### Attribute 정의 및 복제

###### &#x20; Source/Frontier/AbilitySystem/FrontierAttributeSet.h

###### 

###### Attribute 후처리

###### &#x20; Source/Frontier/AbilitySystem/FrontierAttributeSet.cpp

###### &#x20; - 수치 Clamp

###### &#x20; - Damage Meta Attribute 처리

###### &#x20; - 피격 반응 및 사망 연결

###### 

###### 공통 ASC 기능

###### &#x20; Source/Frontier/AbilitySystem/

###### &#x20; FrontierAbilitySystemComponent.cpp

###### 

###### GameplayEffect

###### &#x20; Source/Frontier/AbilitySystem/Effects

###### 

###### GameplayTag

###### &#x20; Source/Frontier/Tags/FrontierGameplayTags.cpp

###### 

###### 

#### ============================================================

#### 2\. 전투 시스템

#### ============================================================

###### 

###### \[핵심 구현]

###### 

###### &#x20; · 무기별 기본 공격, 입력 유지 콤보,

###### &#x20;   AnimNotify 기반 공격 구간을 구현했습니다.

###### 

###### &#x20; · 로컬 플레이어가 무기 소켓 기반 Sphere Trace로

###### &#x20;   Hit 후보를 찾고 서버에 보고하도록 구성했습니다.

###### 

###### &#x20; · 서버는 활성 Ability, 대상 유효성,

###### &#x20;   중복 피격, 팀 규칙을 검증한 뒤에만

###### &#x20;   실제 데미지를 적용합니다.

###### 

###### &#x20; · 공격력, 무기 타입 공격력, 원소 공격력,

###### &#x20;   방어력, 원소 저항과 상성을

###### &#x20;   하나의 데미지 경로에서 계산합니다.

###### 

###### &#x20; · 즉발, 투사체, 지속 범위, 위치 지정형 스킬이

###### &#x20;   동일한 전투 경계를 사용하도록 구성했습니다.

###### 

###### &#x20; · 원소별 GameplayCue와 Niagara 숫자 팝업으로

###### &#x20;   피격 피드백을 분리했습니다.

###### 

###### \[네트워크 판정 흐름]

###### 

###### 로컬 입력

###### &#x20; ↓

###### 몽타주 재생

###### &#x20; ↓

###### AnimNotifyState

###### &#x20; ↓

###### 무기 소켓 Trace

###### &#x20; ↓

###### Hit 후보를 서버에 RPC로 전달

###### &#x20; ↓

###### 서버에서 활성 Ability, 대상, 중복 Hit, 팀 규칙 검증

###### &#x20; ↓

###### 검증 성공 시 데미지 확정

###### &#x20; ↓

###### GameplayEffect 및 GameplayCue 실행

###### 

###### 현재 구조는 Server Rewind 방식이 아닙니다.

###### 입력 반응성을 위해 로컬에서 Hit 후보를 검출하지만,

###### 체력 변경과 데미지 확정은 서버만 수행합니다.

###### 

###### \[데미지 계산]

###### 

###### RawDamage =

###### &#x20; BaseDamage

###### &#x20; + AttackPower × AttackMultiplier

###### &#x20; + WeaponTypeAttackPower × AttackMultiplier

###### &#x20; + ElementAttackPower × ElementMultiplier

###### &#x20; + WeaponElementAttackPower × ElementMultiplier

###### 

###### FinalDamage =

###### &#x20; RawDamage

###### &#x20; × ElementAffinityMultiplier

###### &#x20; × Clamp(

###### &#x20;     DefenseMultiplier × ResistanceMultiplier,

###### &#x20;     0.2,

###### &#x20;     2.0

###### &#x20;   )

###### 

###### 최종값은 Data.Damage SetByCaller로

###### Damage GameplayEffect에 전달됩니다.

###### 

###### AttributeSet은 Damage Meta Attribute를 소비해

###### Health를 감소시킵니다.

###### 

###### \[첫 공격 지연 대응]

###### 

###### Soft Reference가 첫 공격 시점에 동기 로드되면

###### Dedicated Server에서 첫 몽타주나 판정이

###### 늦어질 수 있습니다.

###### 

###### 이를 방지하기 위해 장비 변경 시 다음 자산을

###### RequestAsyncLoad()로 미리 로드하고

###### 강한 참조로 유지했습니다.

###### 

###### &#x20; · 공격 Ability

###### &#x20; · 공격 몽타주

###### &#x20; · GameplayEffect

###### &#x20; · Animation Layer

###### 

###### 실제 공격 중에는

###### GetPreloadedAttackActionAssets()를 통해

###### 캐시만 조회합니다.

###### 

###### \[주요 코드]

###### 

###### 기본 공격 및 콤보

###### &#x20; Source/Frontier/AbilitySystem/Abilities/

###### &#x20; FrontierGameplayAbility\_PlayerAttack.cpp

###### 

###### 공격 Hit RPC

###### &#x20; Source/Frontier/Character/FrontierPlayerCharacter.cpp

###### &#x20; - ReportClientAttackHit()

###### &#x20; - ServerReportAttackHit\_Implementation()

###### 

###### 공격 구간

###### &#x20; Source/Frontier/Animation/Notifies/

###### &#x20; FrontierAnimNotifyState\_PlayerAttackTrace.cpp

###### 

###### 최종 데미지

###### &#x20; Source/Frontier/Combat/FrontierDamageStatics.cpp

###### 

###### 위치 지정형 스킬

###### &#x20; Source/Frontier/AbilitySystem/Abilities/

###### &#x20; FrontierGameplayAbility\_AreaSkill.cpp

###### 

###### 구현 스킬 예시

###### &#x20; Source/Frontier/AbilitySystem/Abilities

###### &#x20; - Burning Slam

###### &#x20; - Throw Axe

###### &#x20; - Ice Floor

###### &#x20; - Whirlwind

###### &#x20; - 보스 스킬

###### 

###### 데미지 숫자

###### &#x20; Source/Frontier/Combat/

###### &#x20; FrontierNumberPopComponent\_NiagaraText.cpp

###### 

###### 공격 자산 프리로드

###### &#x20; Source/Frontier/Components/FrontierEquipmentComponent.cpp

###### 

###### 프리로드 캐시

###### &#x20; Source/Frontier/Components/FrontierEquipmentComponent.h

###### &#x20; - FFrontierPreloadedAttackAssets

###### 

#### 

#### ============================================================

#### 3\. DataAsset·DataTable 기반 아이템 데이터

#### ============================================================

###### 

###### \[핵심 구현]

###### 

###### &#x20; · 무기, 방어구, 장신구, 소모품, 재료를

###### &#x20;   타입별 DataTable Row로 분리했습니다.

###### 

###### &#x20; · 공통 정적 정의와 UUID를 가진 런타임 아이템을

###### &#x20;   별도 구조로 관리했습니다.

###### 

###### &#x20; · 백엔드가 발급한 랜덤 옵션과 생성 스킬을

###### &#x20;   로컬에서 다시 생성하지 않고

###### &#x20;   런타임 인스턴스로 변환합니다.

###### 

###### &#x20; · Inventory, Storage, Equipment 응답은

###### &#x20;   공통 Item DTO를 재사용하고,

###### &#x20;   컨테이너별 슬롯 정보만 Wrapper로 분리했습니다.

###### 

###### &#x20; · 인벤토리 슬롯은 Fast Array로 복제하며,

###### &#x20;   소유자 전용 정보와 다른 플레이어에게 필요한

###### &#x20;   장비 외형을 분리했습니다.

###### 

###### &#x20; · 장착 변경 사항이 Mesh, Animation Layer,

###### &#x20;   Attribute GameplayEffect, 장비 스킬에

###### &#x20;   함께 반영되도록 구성했습니다.

###### 

###### \[데이터 구분]

###### 

###### 정적 템플릿

###### &#x20; · ItemTemplateId

###### &#x20; · 이름, 설명, 카테고리, 아이콘

###### &#x20; · 장착 슬롯, 최대 내구도

###### &#x20; · Equipment BaseValue

###### &#x20; · Soft Asset Reference

###### 

###### 런타임 인스턴스

###### &#x20; · ItemInstanceId

###### &#x20; · RaidItemId

###### &#x20; · OriginItemInstanceId

###### &#x20; · 수량, 내구도, 강화 단계

###### &#x20; · 최종 희귀도

###### &#x20; · RuntimeGeneratedStats

###### &#x20; · RuntimeGeneratedSkills

###### &#x20; · NormalizedScore

###### &#x20; · EquipmentScore

###### 

###### 정적 데이터가 변경되더라도

###### 이미 발급된 아이템의 UUID, 옵션, 스킬,

###### 강화 상태를 다시 생성하지 않는 것이 핵심입니다.

###### 

###### \[인벤토리 복제 정책]

###### 

###### &#x20; · 개인 Inventory와 Loadout의 상세 인스턴스는

###### &#x20;   소유자에게만 복제합니다.

###### 

###### &#x20; · 다른 플레이어에게 필요한 현재 무기와 방어구 외형은

###### &#x20;   공개 Cosmetic Snapshot으로 별도 복제합니다.

###### 

###### &#x20; · 백엔드 변경 API가 성공하기 전에는

###### &#x20;   로컬 슬롯을 성공 상태로 확정하지 않습니다.

###### 

###### \[주요 코드]

###### 

###### 템플릿 및 인스턴스 구조

###### &#x20; Source/Frontier/Inventory/FrontierInventoryTypes.h

###### 

###### 아이템 카탈로그

###### &#x20; Source/Frontier/Inventory/Items/

###### &#x20; FrontierItemCatalogSubsystem.cpp

###### 

###### 백엔드 Item 변환

###### &#x20; Source/Frontier/Inventory/

###### &#x20; FrontierBackendInventoryMapper.cpp

###### 

###### UUID, 옵션, 스킬 복원

###### &#x20; Source/Frontier/Inventory/Persistence/

###### &#x20; FrontierItemPersistenceTypes.cpp

###### 

###### Raid Item 변환

###### &#x20; Source/Frontier/Inventory/

###### &#x20; FrontierRaidRuntimeItemMapper.cpp

###### 

###### Fast Array 인벤토리

###### &#x20; Source/Frontier/Components/FrontierInventoryComponent.cpp

###### 

###### 장비 적용

###### &#x20; Source/Frontier/Components/FrontierEquipmentComponent.cpp

###### 

###### 장비 스킬 수명

###### &#x20; Source/Frontier/Components/

###### &#x20; FrontierEquipmentSkillComponent.cpp

###### 

###### 강화 및 장비 점수

###### &#x20; Source/Frontier/Progression/

###### &#x20; FrontierUpgradeBalanceSubsystem.cpp

###### 

###### 

#### ============================================================

#### 4\. ALI 기반 캐릭터 애니메이션

#### ============================================================

###### 

###### \[핵심 구현]

###### 

###### &#x20; · 공통 Locomotion과 캐릭터별

###### &#x20;   Animation Blueprint를 분리했습니다.

###### 

###### &#x20; · Animation Layer Interface를 사용해

###### &#x20;   무기별 상체 공격 동작을 런타임에 교체합니다.

###### 

###### &#x20; · 무기 변경 시 Anim Layer와 공격 Ability가

###### &#x20;   동일한 WeaponData를 기준으로 갱신됩니다.

###### 

###### &#x20; · AnimNotify와 AnimNotifyState를

###### &#x20;   공격 Trace, 콤보 입력, 투사체 생성,

###### &#x20;   회피 이동 구간과 연결했습니다.

###### 

###### &#x20; · 로비 SceneCapture 프리뷰와 인게임 캐릭터가

###### &#x20;   Mesh와 AnimationClass를 공유하도록

###### &#x20;   Appearance 데이터를 분리했습니다.

###### 

###### \[애니메이션 구조]

###### 

###### 공통 Locomotion Animation Blueprint

###### &#x20; ↓

###### ALI\_Frontier

###### &#x20; ├─ 캐릭터 기본 Layer

###### &#x20; └─ 무기별 AnimLayerClass

###### &#x20;      ↓

###### &#x20;    AnimNotify

###### &#x20;      ↓

###### &#x20;    GameplayAbility 판정 구간

###### 

###### \[주요 코드 및 에디터 자산]

###### 

###### 무기 Animation Layer 교체

###### &#x20; Source/Frontier/Components/FrontierEquipmentComponent.cpp

###### 

###### 캐릭터 Appearance 적용

###### &#x20; Source/Frontier/Character/FrontierPlayerCharacter.cpp

###### 

###### 캐릭터별 Appearance 정의

###### &#x20; Source/Frontier/Character/

###### &#x20; FrontierCharacterAppearanceDataAsset.h

###### 

###### 공통 ALI

###### &#x20; Content/HT/Character/Player/Anims/ALI/

###### &#x20; ALI\_Frontier.uasset

###### 

###### 기본 Locomotion Layer

###### &#x20; Content/HT/Character/Player/Anims/ALI/

###### &#x20; ABP\_BaseLayer.uasset

###### 

###### 캐릭터 Animation Blueprint

###### &#x20; Content/HT/Character/Player/Anims/ALI/

###### &#x20; ABP\_DarkKnight.uasset

###### 

###### Animation Notify

###### &#x20; Source/Frontier/Animation/Notifies

###### 

###### 참고:

###### uasset 내부 그래프는 텍스트 Diff로 확인할 수 없습니다.

###### Unreal Editor에서 State Machine, Linked Anim Layer,

###### Notify 배치를 확인해야 합니다.

###### 

###### 

#### ============================================================

#### 5\. StateTree 기반 몬스터 전투 AI

#### ============================================================

###### 

###### \[핵심 구현]

###### 

###### &#x20; · 일반 몬스터의 Patrol, Chase, Attack, Abandon 흐름을

###### &#x20;   StateTree Task와 Condition으로 구성했습니다.

###### 

###### &#x20; · 이동 속도와 공격 간격을

###### &#x20;   StateTree의 InstanceData로 노출했습니다.

###### 

###### &#x20; · AI Perception의 Sight와 Damage Sense를 함께 사용해

###### &#x20;   시야 밖의 공격자도 인지합니다.

###### 

###### &#x20; · StateTree는 의사결정을 담당하고,

###### &#x20;   EnemyCharacter API는 이동, 공격 Ability,

###### &#x20;   Montage 실행을 담당합니다.

###### 

###### &#x20; · 보스는 거리, 행동 가능 상태, 쿨다운,

###### &#x20;   가중치를 평가해 다음 행동을 선택합니다.

###### 

###### \[실행 흐름]

###### 

###### AI Perception

###### &#x20; ↓

###### CombatTarget 갱신

###### &#x20; ↓

###### StateTree Condition 평가

###### &#x20; ↓

###### Task 선택

###### &#x20; ↓

###### 이동 또는 GameplayAbility 실행

###### 

###### \[주요 코드 및 에디터 자산]

###### 

###### Perception 및 StateTree 실행

###### &#x20; Source/Frontier/AI/FrontierEnemyAIController.cpp

###### 

###### 일반 적 Task

###### &#x20; Source/Frontier/AI/StateTree/

###### &#x20; FrontierEnemyStateTreeTasks.cpp

###### 

###### 일반 적 Condition

###### &#x20; Source/Frontier/AI/StateTree/

###### &#x20; FrontierEnemyStateTreeConditions.cpp

###### 

###### Enemy 실행 API

###### &#x20; Source/Frontier/Character/FrontierEnemyCharacter.cpp

###### 

###### 보스 의사결정

###### &#x20; Source/Frontier/AI/StateTree/FrontierBossStateTree.cpp

###### 

###### 일반 적 StateTree

###### &#x20; Content/HT/Character/Enemy/Normal/StateTree/

###### &#x20; BP\_AIStateTree.uasset

###### 

###### 보스 StateTree

###### &#x20; Content/HT/Character/Enemy/Boss/Golem/AI/

###### &#x20; BP\_BossStateTree.uasset

###### 

###### 

#### ============================================================

#### 6\. 백엔드 API와 런타임 상태 연동

#### ============================================================

###### 

###### \[핵심 구현]

###### 

###### &#x20; · Steam 인증 결과로 백엔드 Access Token을 발급받아

###### &#x20;   로비 데이터를 초기화합니다.

###### 

###### &#x20; · HTTP Wire DTO와 게임 런타임 모델을 분리하고,

###### &#x20;   JSON 파싱 오류가 부분 적용으로 이어지지 않도록

###### &#x20;   Mapper 경계를 구성했습니다.

###### 

###### &#x20; · Inventory, Storage, Equipment의

###### &#x20;   Authoritative Snapshot을 PlayerState에 적용하면서

###### &#x20;   기존 Fast Array UI 갱신 구조를 유지했습니다.

###### 

###### &#x20; · 파티 및 매치메이킹 명령은 HTTP로 요청하고,

###### &#x20;   상태 변경은 WebSocket 기반

###### &#x20;   STOMP 개인 Queue로 수신합니다.

###### 

###### &#x20; · MATCHMAKING\_STATUS와 RAID\_SERVER\_READY 이벤트를

###### &#x20;   구조체로 변환해 매칭 UI와

###### &#x20;   레이드 입장 흐름에 전달합니다.

###### 

###### &#x20; · Dedicated Server가 초기 Loot Batch 준비와

###### &#x20;   Ready API를 완료한 후에만

###### &#x20;   Entry 및 ClientTravel을 진행합니다.

###### 

###### &#x20; · Join Token 승인, Raid Item ID 변환,

###### &#x20;   탈출·사망 결과 제출,

###### &#x20;   런타임 상태 정리를 구성했습니다.

###### 

###### 참고:

###### 백엔드 애플리케이션과 DB 구현은

###### 이 Unreal 저장소의 범위가 아닙니다.

###### 

###### 이 저장소에서는 게임 클라이언트와

###### Dedicated Server의 API 계약, 통신,

###### DTO 변환, 런타임 반영 과정을 확인할 수 있습니다.

###### 

###### \[서버별 책임]

###### 

###### 게임 클라이언트

###### &#x20; · 입력 및 UI

###### &#x20; · Steam 친구 초대

###### &#x20; · HTTP 명령 요청

###### &#x20; · STOMP 상태 표시

###### &#x20; · ClientTravel

###### 

###### 로비 백엔드

###### &#x20; · 인증

###### &#x20; · 파티 및 매칭

###### &#x20; · 영구 인벤토리 및 장비

###### &#x20; · 서버 할당

###### &#x20; · 레이드 결과 정산

###### 

###### Dedicated Server

###### &#x20; · 실시간 전투 및 이동

###### &#x20; · 런타임 Loot 권한

###### &#x20; · Join 검증

###### &#x20; · 탈출·사망 결과 생성

###### 

###### \[전체 흐름]

###### 

###### 1\. Steam 로그인 및 백엔드 인증

###### 2\. Inventory, Storage, Equipment 조회

###### 3\. PlayerState Runtime Component에 데이터 적용

###### 4\. Steam Party 및 Backend Party 연결

###### 5\. HTTP Matchmaking Ticket 요청

###### 6\. STOMP MATCHMAKING\_STATUS 수신

###### 7\. Dedicated Server 생성 및 Loot Pool 초기화

###### 8\. Ready API 완료

###### 9\. STOMP RAID\_SERVER\_READY 수신

###### 10\. Raid Entry 및 Join Token 발급

###### 11\. ClientTravel

###### 12\. Dedicated Server Join Authorization

###### 13\. 전투 및 루팅

###### 14\. 탈출 또는 사망 결과 제출

###### 15\. Backend Authoritative Snapshot 반영

###### 

###### \[안정성을 위해 고려한 사항]

###### 

###### &#x20; · Access Token과 Refresh Token을

###### &#x20;   Replication하거나 로그에 출력하지 않습니다.

###### 

###### &#x20; · 변경 API의 성공 응답 전에는

###### &#x20;   로컬 Inventory를 성공 상태로 확정하지 않습니다.

###### 

###### &#x20; · 여러 Aggregate를 반환하는 응답은

###### &#x20;   모든 변환에 성공한 경우에만 적용합니다.

###### 

###### &#x20; · itemInstanceId, raidItemId,

###### &#x20;   originItemInstanceId, lootSourceId의

###### &#x20;   역할을 분리합니다.

###### 

###### &#x20; · WebSocket 이벤트 유실 가능성에 대비해

###### &#x20;   HTTP 상태 조회 경로를 함께 유지합니다.

###### 

###### &#x20; · Dedicated Server 내부 API 자격 증명과

###### &#x20;   Player JWT의 역할을 분리합니다.

###### 

###### \[주요 코드]

###### 

###### HTTP 전송 및 JSON 파싱

###### &#x20; Source/FrontierOnline/Private/

###### &#x20; FrontierOnlineHttpClient.cpp

###### 

###### Wire DTO

###### &#x20; Source/FrontierOnline/Public/Inventory/

###### &#x20; FrontierOnlineInventoryTypes.h

###### 

###### 플레이어별 Protocol 상태

###### &#x20; Source/Frontier/Components/

###### &#x20; FrontierBackendProtocolComponent.cpp

###### 

###### WebSocket 및 STOMP

###### &#x20; Source/Frontier/Online/

###### &#x20; FrontierBackendWebSocketSubsystem.cpp

###### 

###### Steam 파티

###### &#x20; Source/Frontier/Game/FrontierSteamPartySubsystem.cpp

###### 

###### Raid Loot Pool

###### &#x20; Source/Frontier/Progression/

###### &#x20; FrontierRaidLootPoolSubsystem.cpp

###### 

###### Raid 입장 및 결과

###### &#x20; Source/Frontier/Progression/

###### &#x20; FrontierRaidSessionSubsystem.cpp

###### 

###### 내부 API 설정

###### &#x20; Source/Frontier/Progression/

###### &#x20; FrontierInternalApiConfig.cpp

###### 

###### 

#### ============================================================

#### 사용 기술

#### ============================================================

###### 

###### 

###### 게임플레이

###### &#x20; Gameplay Ability System

###### &#x20; GameplayTags

###### &#x20; GameplayTasks

###### 

###### AI

###### &#x20; StateTree

###### &#x20; GameplayStateTree

###### &#x20; AI Perception

###### &#x20; Navigation System

###### 

###### 애니메이션

###### &#x20; Animation Layer Interface

###### &#x20; AnimNotify

###### &#x20; AnimNotifyState

###### 

###### UI 및 연출

###### &#x20; UMG

###### &#x20; Niagara

###### &#x20; SceneCapture2D

###### 

###### 네트워크

###### &#x20; Dedicated Server

###### &#x20; RPC

###### &#x20; RepNotify

###### &#x20; Fast Array Serializer

###### &#x20; OwnerOnly Replication

###### 

###### 데이터

###### &#x20; DataAsset

###### &#x20; DataTable

###### 

###### 온라인 및 백엔드

###### &#x20; OnlineSubsystemSteam

###### &#x20; Steamworks Lobby/Friends API

###### &#x20; HTTP/JSON

###### &#x20; WebSocket

###### &#x20; STOMP 1.2

