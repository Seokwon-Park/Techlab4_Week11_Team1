#include "EnginePCH.h"
#include "Editor/Viewports/EditorViewportClient.h"
#include "Components/PrimitiveComponent.h"
#include "Editor/Gizmo/Gizmo.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Input/InputSystem.h"
#include "Camera/ViewportCameraMovement.h"
#include "Math/EngineMath.h"
#include "Render/RenderCommand.h"
#include "Editor/AssetEditor/FPreviewScene.h"
#include "Math/BoxSphereBounds.h"
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

FEditorViewportClient::FEditorViewportClient() {}

FEditorViewportClient::FEditorViewportClient(FPreviewScene& InPreviewScene)
    : PreviewScene(&InPreviewScene) {}

UWorld* FEditorViewportClient::GetWorld() const {
  return PreviewScene ? PreviewScene->GetWorld() : World;
}

void FEditorViewportClient::SetWorld(UWorld* InWorld) {
  // AssetEditor Viewport인 경우, SetWorld를 무시
  // PreviewScene이 World를 직접 관리해야함
  if (PreviewScene) {
    return;
  }

  if (World != InWorld) {
    RenderData.ResetScene();
    World = InWorld;
  }
}

void FEditorViewportClient::FocusOnBounds(const FBoxSphereBounds& Bounds) {
  const float Radius = FMath::Max(FMath::Max(Bounds.SphereRadius, Bounds.BoxExtent.Size()), 0.1f);
  const float Aspect = FMath::Max(AspectRatio, 0.01f);
  const float HalfFOV = FMath::DegreesToRadians(FMath::Clamp(ViewFOV, 1.0f, 179.0f)) * 0.5f;
  const float LimitingHalfFOV = atanf(tanf(HalfFOV) * FMath::Min(Aspect, 1.0f));
  const float Distance = FMath::Max(Radius * 1.1f / sinf(LimitingHalfFOV), Radius + NearClip);

  OrthoWidth = Radius * 2.2f * FMath::Max(Aspect, 1.0f);
  ViewLocation = Bounds.Origin - GetViewForward() * Distance;
  FarClip = FMath::Max(FarClip, Distance + Radius * 2.0f);
}

void FEditorViewportClient::TickInput(float DeltaTime, bool bCaptured,
                                      bool bHovered, int32 WheelDelta,
                                      float MoveSpeed, float MouseSensitivity) {

  FViewportCameraInput Input;
  Input.bCaptured = bCaptured;
  Input.bAllowWheel = bCaptured || bHovered;
  Input.MouseX = static_cast<float>(FInputSystem::GetMouseDeltaX());
  Input.MouseY = static_cast<float>(FInputSystem::GetMouseDeltaY());
  Input.Wheel = static_cast<float>(WheelDelta);
  Input.bForward = FInputSystem::IsKeyDown(EKeyCode::W);
  Input.bBackward = FInputSystem::IsKeyDown(EKeyCode::S);
  Input.bRight = FInputSystem::IsKeyDown(EKeyCode::D);
  Input.bLeft = FInputSystem::IsKeyDown(EKeyCode::A);
  Input.bUp = FInputSystem::IsKeyDown(EKeyCode::E);
  Input.bDown = FInputSystem::IsKeyDown(EKeyCode::Q);
  FViewportCameraState Camera{ViewLocation, ViewRotation, OrthoWidth,
      Width > 0 ? static_cast<float>(Width) : 800.0f, bIsPerspective};
  ApplyViewportCameraMovement(Camera, Input, DeltaTime, MoveSpeed, MouseSensitivity);
  ViewLocation = Camera.Location;
  ViewRotation = Camera.Rotation;
  OrthoWidth = Camera.OrthoWidth;
}

// 기즈모 조작 및 액터 피킹 갱신
void FEditorViewportClient::UpdateGizmoAndPicking(
    FGizmo *InGizmo, const FVector2 &LocalMousePosition,
    const std::function<void(AActor *)> &OnSelectActor) {
  if (!bActive || !InGizmo) {
    return;
  }

  const FVector2 ViewLocalMouse(LocalMousePosition.X - Rect.X,
                                LocalMousePosition.Y - Rect.Y);
  FRay Ray = DeprojectScreenToWorld(ViewLocalMouse);
  const FMatrix ViewProjection = GetViewProjectionMatrix();
  const bool bMouseDown = FInputSystem::IsMouseDown(EMouseButton::Left);

  InGizmo->Update(Ray, ViewLocalMouse, ViewProjection,
                  static_cast<int>(Rect.Width), static_cast<int>(Rect.Height),
                  bMouseDown, GetViewLocation(), !IsPerspective());

  if (FInputSystem::IsMousePressed(EMouseButton::Left) && !InGizmo->IsUsing() &&
      InGizmo->GetHoveredAxis() < 0) {
    FHitResult Hit;
    UWorld* PickingWorld = GetWorld();
    if (PickingWorld && PickingWorld->LineTraceSingle(Ray, Hit)) {
      if (Hit.HitComponent && Hit.HitComponent->GetOwner()) {
        if (OnSelectActor) {
          OnSelectActor(Hit.HitComponent->GetOwner());
        }
      }
    } else {
      if (OnSelectActor) {
        OnSelectActor(nullptr);
      }
    }
  }
}

void FEditorViewportClient::MouseMove(FViewport *Viewport, int32 X, int32 Y) {}

bool FEditorViewportClient::InputKey(FViewport *Viewport, int32 Key,
                                     bool bDown) {
  return false;
}

bool FEditorViewportClient::InputAxis(FViewport* Viewport, int32 ControllerId, EGameInputAxis AxisKey, float Delta, float DeltaTime) {
  return false;
}

// 뷰 행렬 계산
FMatrix FEditorViewportClient::GetViewMatrix() const {
  FQuat WorldQuat = ViewRotation.Quaternion();
  FMatrix RotationMatrix = WorldQuat.ToFMatrix();

  FMatrix InverseTranslation = FMatrix::MakeTranslation(ViewLocation * -1.0f);
  FMatrix InverseRotation = RotationMatrix.GetTransposed();

  return InverseTranslation * InverseRotation;
}

// 원근 투영 행렬
FMatrix FEditorViewportClient::GetPerspectiveMatrix() const {
  const float HalfFOV = FMath::DegreesToRadians(ViewFOV) * 0.5f;

  const float YScale = 1.0f / tan(HalfFOV);
  const float XScale = YScale / AspectRatio;

  const float ZScale = FarClip / (FarClip - NearClip);
  const float ZOffset = -NearClip * FarClip / (FarClip - NearClip);

  return FMatrix(0.0f, 0.0f, ZScale, 1.0f, XScale, 0.0f, 0.0f, 0.0f, 0.0f,
                 YScale, 0.0f, 0.0f, 0.0f, 0.0f, ZOffset, 0.0f);
}

// 직교 투영 행렬
FMatrix FEditorViewportClient::GetOrthogonalMatrix() const {
  const float Width = OrthoWidth;
  const float Height = Width / AspectRatio;

  const float XScale = 2.0f / Width;
  const float YScale = 2.0f / Height;

  const float ZScale = 1.0f / (FarClip - NearClip);
  const float ZOffset = -NearClip / (FarClip - NearClip);

  return FMatrix(0.0f, 0.0f, ZScale, 0.0f, XScale, 0.0f, 0.0f, 0.0f, 0.0f,
                 YScale, 0.0f, 0.0f, 0.0f, 0.0f, ZOffset, 1.0f);
}

// 투영 행렬
FMatrix FEditorViewportClient::GetProjectionMatrix() const {
  if (bIsPerspective) {
    return GetPerspectiveMatrix();
  }
  return GetOrthogonalMatrix();
}

// 뷰 프로젝션 행렬
FMatrix FEditorViewportClient::GetViewProjectionMatrix() const {
  return GetViewMatrix() * GetProjectionMatrix();
}

// 전방 벡터 계산
FVector FEditorViewportClient::GetViewForward() const {
  FQuat WorldQuat = ViewRotation.Quaternion();
  return WorldQuat.GetForwardVector();
}

// 씬 뷰 정보 구성
FSceneView FEditorViewportClient::CalcSceneView(const FRect &InViewRect) const {
    FSceneView OutView;
    OutView.ViewIndex = ViewIndex;
    OutView.ViewLocation = ViewLocation;
    OutView.ViewRotation = ViewRotation;
    OutView.ViewForward = GetViewForward();
    OutView.ViewMatrix = GetViewMatrix();
    OutView.ProjectionMatrix = GetProjectionMatrix();
    OutView.ViewProjectionMatrix = GetViewProjectionMatrix();
    OutView.Frustum = ExtractFrustumPlanes(OutView.ViewProjectionMatrix);
    OutView.FrustumCorners = GenerateFrustumCorners(OutView.ViewProjectionMatrix.Inverse());
    OutView.ViewRect = InViewRect;
    OutView.FieldOfView = ViewFOV;
    OutView.AspectRatio = AspectRatio;
    OutView.NearClip = NearClip;
    OutView.FarClip = FarClip;
    OutView.OrthoWidth = OrthoWidth;
    OutView.bIsPerspective = bIsPerspective;
    OutView.bIsWireframe = ViewportMode == EViewportMode::Wireframe;

    if (GCameraOverlayDebug.bOverride)
    {
        const FCameraOverlayDebug& D = GCameraOverlayDebug;
        OutView.OverlayColor = FLinearColor{ D.FadeColor[0], D.FadeColor[1], D.FadeColor[2], D.FadeAmount };
        OutView.LetterboxAmount = D.LetterboxAmount;
        OutView.LetterboxAspect = D.LetterboxAspect;
        OutView.VignetteIntensity = D.VignetteIntensity;
    }

    return OutView;
}

// 씬 뷰 생성
FSceneView FEditorViewportClient::CalcSceneView() const {
  return CalcSceneView(
      FRect(0.0f, 0.0f, static_cast<float>(Width), static_cast<float>(Height)));
}

// 레벨 에디터 뷰포트 하나를 그린다. 씬을 그리고 그 위에 선, 그리드, 외곽선, 기즈모를 올린다.
void FEditorViewportClient::Draw(FRenderer* Renderer, const FSceneRenderOptions& Options)
{
	UWorld* TargetWorld = GetWorld();
	if (!GEditor || !GEditor->SettingsPanel || !TargetWorld || !Renderer) return;

	// 카메라를 뷰·투영 행렬로 바꾼다.
	const FSceneView SceneView = CalcSceneView(Rect);

	// 보이는 물체와 라이트를 모은다.
	FSceneRenderer SceneRenderer(TargetWorld, SceneView, RenderData);
	SceneRenderer.InitViews(Renderer, Options);

	// 에디터 도구는 엔진(GEditor)이 한 벌만 갖고 있고 네 뷰포트가 같이 쓴다.
	const FEditorSettings& Settings = GEditor->SettingsPanel->GetSettings();
	FLineBatcher* LineBatcher = GEditor->LineBatcher.get();
	FGizmo* Gizmo = GEditor->Gizmo.get();
	FOutline* Outline = GEditor->Outline.get();
	FGridRenderer* GridRenderer = GEditor->GridRenderer.get();
	FGizmoRenderer* GizmoRenderer = GEditor->GizmoRenderer.get();
	FOutlineRenderer* OutlineRenderer = GEditor->OutlineRenderer.get();
	FSceneDepthRenderer* SceneDepthRenderer = GEditor->SceneDepthRenderer.get();
	FCascadeFloorRenderer* CascadeFloorRenderer = GEditor->CascadeFloorRenderer.get();

	// 패스를 순서대로 실행해 ViewTargets에 그린다. 에디터 요소는 씬 위에 그려야 하므로
	// 각각 그 시점에 불릴 함수로 등록한다. 등록한 함수는 Render 안에서 불린다.
	SceneRenderer.Render(Renderer, ViewTargets, Options,
		[&](FScenePassCallbacks& Callbacks)
		{
			// 뷰 모드가 Scene Depth, Cascade일 때 깊이를 색으로 바꿔 보여 준다.
			FTexture2D* ColorTarget = ViewTargets.SceneColor.get();
			FTexture2D* DepthTarget = ViewTargets.Depth.get();
			if (SceneDepthRenderer)
				Callbacks.DepthVisualization = [SceneDepthRenderer, &SceneView, DepthTarget, ColorTarget, Range = Options.SceneDepthRange]
				{
					SceneDepthRenderer->OnRender(SceneView, DepthTarget, ColorTarget, Range);
				};
			if (Options.bShowCascade && CascadeFloorRenderer)
				Callbacks.CascadeVisualization = [CascadeFloorRenderer, &SceneView, &SceneRenderer, DepthTarget, ColorTarget]
				{
					CascadeFloorRenderer->OnRender(SceneView, SceneRenderer.GetRenderData(), DepthTarget, ColorTarget);
				};

			const FViewportSettings ViewportSetting{ 0, 0, ViewTargets.SceneColor->GetWidth(), ViewTargets.SceneColor->GetHeight(), 0.0f, 1.0f };
			const bool bIsActiveViewport = GEditor->ViewportsPanel && ViewIndex == GEditor->ViewportsPanel->GetActiveViewIndex();

			const bool bDrawSelectedLight = bIsActiveViewport && Gizmo->GetTarget() &&
				(Cast<ASpotLightActor>(Gizmo->GetTarget()->GetOwner()) ||
					Cast<APointLightActor>(Gizmo->GetTarget()->GetOwner()) ||
					Cast<ARectLightActor>(Gizmo->GetTarget()->GetOwner()) ||
					Cast<ADecalActor>(Gizmo->GetTarget()->GetOwner()));

			const bool bDrawCascadeBounds = Settings.bDrawCascadeBounds;
			{
				Callbacks.Overlays.EditorLines = [this, &SceneView, &Settings, LineBatcher, Gizmo, bDrawSelectedLight, bDrawCascadeBounds]
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

						DrawCollisionShapes(GEditor->EditorWorld, LineBatcher, SelectedActor);
						DrawSpringArms(GEditor->EditorWorld, LineBatcher, SelectedActor);

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
		});
}

// 뷰포트 크기 변경 및 타깃 생성
void FEditorViewportClient::Resize(const uint32 InWidth,
                                   const uint32 InHeight) {
  if (InWidth == 0 || InHeight == 0) {
    return;
  }

  if (Width == InWidth && Height == InHeight &&
      ViewTargets.IsValidFor(InWidth, InHeight)) {
    return;
  }

  const uint32 OldWidth = Width;
  const uint32 OldHeight = Height;

  Width = InWidth;
  Height = InHeight;

  ViewTargets.Resize(Width, Height);

  if (Height > 0) {
    SetAspectRatio(static_cast<float>(Width) / static_cast<float>(Height));
  }

  if (!bIsPerspective && OldWidth > 1 && Width > 1) {
    // 화면 크기 변경 시 직교 배율 유지
    OrthoWidth *= static_cast<float>(Width) / static_cast<float>(OldWidth);
  }
}

// 뷰포트 시점 설정
void FEditorViewportClient::SetViewportType(const ELevelViewportType InType) {
  ViewportType = InType;
  switch (InType) {
  case ELevelViewportType::Perspective:
    bIsPerspective = true;
    ViewLocation = FVector(-5.0f, -10.0f, 10.0f);
    ViewRotation = FRotator(30.0f, 45.0f, 0.0f);
    break;
  case ELevelViewportType::Top:
    bIsPerspective = false;
    ViewLocation = FVector(0.0f, 0.0f, 20.0f);
    ViewRotation = FRotator(90.0f, 0.0f, 0.0f);
    break;
  case ELevelViewportType::Front:
    bIsPerspective = false;
    ViewLocation = FVector(20.0f, 0.0f, 0.0f);
    ViewRotation = FRotator(0.0f, 180.0f, 0.0f);
    break;
  case ELevelViewportType::Right:
    bIsPerspective = false;
    ViewLocation = FVector(0.0f, 20.0f, 0.0f);
    ViewRotation = FRotator(0.0f, -90.0f, 0.0f);
    break;
  }
}

// 화면 좌표 역투영
FRay FEditorViewportClient::DeprojectScreenToWorld(
    const FVector2 &ScreenPos) const {
  if (Width == 0 || Height == 0) {
    return FRay();
  }
  const float ScreenWidth = static_cast<float>(Width);
  const float ScreenHeight = static_cast<float>(Height);
  const float NdcX = (ScreenPos.X / ScreenWidth) * 2.0f - 1.0f;
  const float NdcY = 1.0f - (ScreenPos.Y / ScreenHeight) * 2.0f;
  const float Aspect =
      AspectRatio > 0.0f ? AspectRatio : (ScreenWidth / ScreenHeight);
  const FQuat Rotation = ViewRotation.Quaternion();
  const FVector Forward = Rotation.GetForwardVector();
  const FVector Right = Rotation.GetRightVector();
  const FVector Up = Rotation.GetUpVector();

  if (bIsPerspective) {
    const float Tangent = tanf(FMath::DegreesToRadians(ViewFOV) * 0.5f);
    const FVector Direction =
        (Forward + Right * (NdcX * Tangent * Aspect) + Up * (NdcY * Tangent))
            .Normalized();
    return FRay(ViewLocation, Direction);
  } else {
    const float OrthoHeight = OrthoWidth / Aspect;
    const FVector Origin = ViewLocation + Forward * NearClip +
                           Right * (NdcX * OrthoWidth * 0.5f) +
                           Up * (NdcY * OrthoHeight * 0.5f);
    return FRay(Origin, Forward.Normalized());
  }
}
