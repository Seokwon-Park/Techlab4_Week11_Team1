#include "EnginePCH.h"
#include "Editor/Viewports/LevelEditorViewport.h"
#include "Editor/HitoriEd/Editor.h"
#include "Render/Renderer.h"
#include "Render/SceneRenderer.h"
#include "Components/ShapeComponent.h"
#include "Components/DecalComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/RectLightComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/Actor/DecalActor.h"
#include "GameFramework/Actor/PointLightActor.h"
#include "GameFramework/Actor/SpotLightActor.h"
#include "GameFramework/Actor/RectLightActor.h"
#include "Engine/World.h"

FLevelEditorViewportClient* GCurrentLevelEditingViewportClient = nullptr;

// 사라지는 뷰포트를 전역이 계속 가리키지 않게 한다. UE도 소멸자에서 같은 일을 한다.
FLevelEditorViewportClient::~FLevelEditorViewportClient()
{
	if (GCurrentLevelEditingViewportClient == this)
	{
		GCurrentLevelEditingViewportClient = nullptr;
	}
}

// 이번 프레임의 표시 설정을 채운 뒤 기반 클래스의 Draw로 씬을 그린다.
// 에디터 설정 창의 값과 이 뷰포트의 뷰 모드를 EngineShowFlags에 옮긴다.
// UE의 FLevelEditorViewportClient::Draw(FViewport*, FCanvas*)도 준비를 한 뒤 FEditorViewportClient::Draw를 부른다.
void FLevelEditorViewportClient::Draw(FRenderer* Renderer)
{
	if (GEditor && GEditor->SettingsPanel)
	{
		const FEditorSettings& S = GEditor->SettingsPanel->GetSettings();

		// 모든 뷰포트에 똑같이 적용되는 에디터 설정
		EngineShowFlags.bEnableFXAA = S.bEnableFXAA;
		EngineShowFlags.bCacheShadows = S.bCacheShadows;
		EngineShowFlags.PcfMode = S.PcfMode;
		EngineShowFlags.RenderingPath = S.RenderingPath;
		EngineShowFlags.LightingModel = S.LightingModel;
		EngineShowFlags.LightCullingMode = S.LightCullingMode;
		EngineShowFlags.DirectionalShadowMapSize = S.GetDirectionalShadowMapSize();
		EngineShowFlags.PointShadowMapSize = S.GetPointShadowMapSize();
		EngineShowFlags.SpotShadowMapSize = S.GetSpotShadowMapSize();
		EngineShowFlags.bGammaCorrection = S.bGammaCorrection;
		EngineShowFlags.DisplayGamma = S.DisplayGamma;
		EngineShowFlags.bShowVignette = S.bShowVignette;
		EngineShowFlags.bShowCameraOverlay = S.bShowCameraOverlay;
		EngineShowFlags.bShowSkeletalMesh = S.bShowSkeletalMesh;
		EngineShowFlags.bDrawPrimitives = S.bDrawPrimitives;
		EngineShowFlags.bDrawDecals = S.bDrawDecals;
		EngineShowFlags.bDecalCulling = S.bDecalCulling;
		EngineShowFlags.bDecalNarrowCulling = S.bDecalNarrowCulling;
		EngineShowFlags.bDecalBVHCulling = S.bDecalBVHCulling;
		EngineShowFlags.bDecalOBBTree = S.bDecalOBBTree;

		// 이 뷰포트의 뷰 모드에서 나오는 설정
		EngineShowFlags.bEnableFog = ViewportMode == EViewportMode::Solid;
		EngineShowFlags.bShowSceneDepth = IsSceneDepth();
		EngineShowFlags.bShowCascade = IsCascade();
		EngineShowFlags.bShowShadowMask = IsShadowMask();
		EngineShowFlags.bShowWorldNormal = IsWorldNormal();
		EngineShowFlags.bShowLightCulling = IsLightCulling();
		EngineShowFlags.SceneDepthRange = MaxRange;
	}

	FEditorViewportClient::Draw(Renderer);
}

// 레벨 에디터 뷰포트의 씬 위에 선, 그리드, 외곽선, 기즈모를 올린다. 씬은 기반 클래스의 Draw가 그린다.
// 여기서는 그릴 시점에 불릴 함수를 등록만 하고, 등록한 함수는 FSceneRenderer::Render 안에서 불린다.
void FLevelEditorViewportClient::Draw(const FSceneView* View, FScenePassCallbacks& Callbacks)
{
	if (!GEditor || !GEditor->SettingsPanel) return;
	const FSceneView& SceneView = *View;

	// 에디터 도구는 엔진(GEditor)이 한 벌만 갖고 있고 네 뷰포트가 같이 쓴다.
	const FEditorSettings& Settings = GEditor->SettingsPanel->GetSettings();
	UWorld* EditorWorld = GEditor->GetEditorWorldContext().World();
	FLineBatcher* LineBatcher = GEditor->LineBatcher.get();
	FGizmo* Gizmo = GEditor->Gizmo.get();
	FOutline* Outline = GEditor->Outline.get();
	FGridRenderer* GridRenderer = GEditor->GridRenderer.get();
	FGizmoRenderer* GizmoRenderer = GEditor->GizmoRenderer.get();
	FOutlineRenderer* OutlineRenderer = GEditor->OutlineRenderer.get();
	FSceneDepthRenderer* SceneDepthRenderer = GEditor->SceneDepthRenderer.get();
	FCascadeFloorRenderer* CascadeFloorRenderer = GEditor->CascadeFloorRenderer.get();

	// 뷰 모드가 Scene Depth, Cascade일 때 깊이를 색으로 바꿔 보여 준다.
	FTexture2D* ColorTarget = ViewTargets.SceneColor.get();
	FTexture2D* DepthTarget = ViewTargets.Depth.get();
	if (SceneDepthRenderer)
		Callbacks.DepthVisualization = [SceneDepthRenderer, &SceneView, DepthTarget, ColorTarget, Range = MaxRange]
		{
			SceneDepthRenderer->OnRender(SceneView, DepthTarget, ColorTarget, Range);
		};
	if (IsCascade() && CascadeFloorRenderer)
		Callbacks.CascadeVisualization = [this, CascadeFloorRenderer, &SceneView, DepthTarget, ColorTarget]
		{
			CascadeFloorRenderer->OnRender(SceneView, RenderData, DepthTarget, ColorTarget);
		};

	const FViewportSettings ViewportSetting{ 0, 0, ViewTargets.SceneColor->GetWidth(), ViewTargets.SceneColor->GetHeight(), 0.0f, 1.0f };
	const bool bIsActiveViewport = (GCurrentLevelEditingViewportClient == this);

	const bool bDrawSelectedLight = bIsActiveViewport && Gizmo->GetTarget() &&
		(Cast<ASpotLightActor>(Gizmo->GetTarget()->GetOwner()) ||
			Cast<APointLightActor>(Gizmo->GetTarget()->GetOwner()) ||
			Cast<ARectLightActor>(Gizmo->GetTarget()->GetOwner()) ||
			Cast<ADecalActor>(Gizmo->GetTarget()->GetOwner()));

	const bool bDrawCascadeBounds = Settings.bDrawCascadeBounds;
	{
		Callbacks.Overlays.EditorLines = [this, &SceneView, &Settings, LineBatcher, Gizmo, EditorWorld, bDrawSelectedLight, bDrawCascadeBounds]
			{
				LineBatcher->BeginFrame();
				if (bDrawCascadeBounds)
				{
					FSceneRenderData& Data = RenderData;
					const FVector4 Colors[4] = {
						FVector4(1.f, 0.2f, 0.2f, 1.f),
						FVector4(1.f, 0.8f, 0.2f, 1.f),
						FVector4(0.2f, 1.f, 0.4f, 1.f),
						FVector4(0.2f, 0.6f, 1.f, 1.f)
					};
					const FBox ClipBox{ FVector(-1.f, -1.f, 0.f), FVector(1.f, 1.f, 1.f) };
					// 광원의 첫 그림자 뷰(VisibleLightShadowViews)부터 cascade 순서로 들어 있다. 그림자가 없는 방향광은 건너뛴다.
					const uint32 DirectionalOffset = Data.GetDirectionalLightOffset();
					for (uint32 LightIndex = 0; LightIndex < Data.GetDirectionalLightCount(); ++LightIndex)
					{
						const uint32 VisibleIndex = DirectionalOffset + LightIndex;
						if (VisibleIndex >= static_cast<uint32>(Data.VisibleLightShadowViews.Num())) continue;
						const uint32 FirstCascade = Data.VisibleLightShadowViews[VisibleIndex];
						if (FirstCascade == FSceneRenderData::InvalidShadowView) continue;
						for (uint32 CascadeIndex = 0; CascadeIndex < Data.CascadeShadowCount; ++CascadeIndex)
						{
							const uint32 ShadowIndex = FirstCascade + CascadeIndex;
							if (ShadowIndex >= static_cast<uint32>(Data.LightShadows.Num())) break;
							LineBatcher->AddOrientedBox(ClipBox, Data.LightShadows[ShadowIndex].ViewProjection.Inverse(), Colors[CascadeIndex]);
						}
					}
				}
				if (Settings.bDrawBatchLine && Settings.bDrawBoundingBox)
				{
					LineBatcher->BuildVertexBuffer();
				}
				
				if (bDrawSelectedLight)
				{
					if (ASpotLightActor* Light = Cast<ASpotLightActor>(Gizmo->GetTarget()->GetOwner()))
						Light->GetSpotLightComponent()->DrawDebug(LineBatcher);
					else if (APointLightActor* Light = Cast<APointLightActor>(Gizmo->GetTarget()->GetOwner()))
						Light->GetPointLightComponent()->DrawDebug(LineBatcher);
					else if (ARectLightActor* Light = Cast<ARectLightActor>(Gizmo->GetTarget()->GetOwner()))
						Light->GetRectLightComponent()->DrawDebug(LineBatcher);
					else if (ADecalActor* Decal = Cast<ADecalActor>(Gizmo->GetTarget()->GetOwner()))
						Decal->GetDecalComponent()->DrawDebug(LineBatcher);
				}

				const AActor* SelectedActor =
					Gizmo->GetTarget() ? Gizmo->GetTarget()->GetOwner() : nullptr;

				DrawCollisionShapes(EditorWorld, LineBatcher, SelectedActor);
				DrawSpringArms(EditorWorld, LineBatcher, SelectedActor);

				LineBatcher->OnRender(SceneView.ViewProjectionMatrix);
			};
	}
	if (Settings.bDrawBatchLine)
	{
		Callbacks.Overlays.EditorGrid = [this, &SceneView, &Settings, GridRenderer, ViewportSetting]
			{
				EGridPlane GridPlane = EGridPlane::XY;
				switch (ViewportType)
				{
				case ELevelViewportType::Front: GridPlane = EGridPlane::YZ; break;
				case ELevelViewportType::Right: GridPlane = EGridPlane::XZ; break;
				default: break;
				}
				if (Settings.bDrawPSGrid && SceneView.bIsPerspective)
					GridRenderer->OnRenderPSGrid(SceneView.ViewProjectionMatrix, SceneView.ViewLocation,
						Settings, ViewportSetting);
				else
					GridRenderer->OnRenderBatchGrid(SceneView.ViewProjectionMatrix, SceneView.ViewLocation,
						SceneView.ViewForward, GridPlane,
						static_cast<float>(Settings.GridSpacing),
						SceneView.bIsPerspective, ViewportSetting);
			};
	}
	if (bIsActiveViewport && Outline->GetTarget())
		Callbacks.Overlays.EditorOutline = [&SceneView, Outline, OutlineRenderer, ViewportSetting]
		{
			OutlineRenderer->OnRender(*Outline, SceneView.ViewProjectionMatrix, ViewportSetting);
		};
	if (bIsActiveViewport && Gizmo->GetTarget())
		Callbacks.Overlays.EditorGizmo = [&SceneView, Gizmo, GizmoRenderer]
		{
			GizmoRenderer->OnRender(*Gizmo, SceneView.ViewProjectionMatrix,
				SceneView.ViewLocation, !SceneView.bIsPerspective);
		};
	Callbacks.Overlays.bClearEditorDepth = true;
	if (Settings.bShowUUID)
		Callbacks.Overlays.EditorUUID = [&SceneView] { GEditor->RenderActorUUIDs(SceneView); };
}
