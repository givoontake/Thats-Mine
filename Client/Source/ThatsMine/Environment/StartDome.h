#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "StartDome.generated.h"

class UStaticMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;

UCLASS()
class THATSMINE_API AStartDome : public AActor
{
	GENERATED_BODY()

public:
	AStartDome();
	virtual void OnConstruction(const FTransform& transform) override;
	virtual void PostRegisterAllComponents() override;
	virtual void Tick(float delta_seconds) override;
	virtual bool ShouldTickIfViewportsOnly() const override;

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Start Dome")
	void ShowDome();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Start Dome")
	void StartDissolve();

	// 연출 없이 다 사라진 상태로 만든다.
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Start Dome")
	void HideDome();

protected:
	virtual void BeginPlay() override;

private:
	void UpdateMaterial();

	UPROPERTY(VisibleAnywhere, Category = "Start Dome")
	TObjectPtr<UStaticMeshComponent> dome_mesh_;

	UPROPERTY(EditAnywhere, Category = "Start Dome")
	TObjectPtr<UMaterialInterface> dome_material_;

	UPROPERTY(EditAnywhere, Category = "Start Dome", meta = (ClampMin = "0.1", Units = "s"))
	float dissolve_duration_ = 2.0f;

	UPROPERTY(EditAnywhere, Category = "Start Dome", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float dissolve_progress_ = 0.0f;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> dynamic_material_;

	bool is_dissolving_ = false;
};
