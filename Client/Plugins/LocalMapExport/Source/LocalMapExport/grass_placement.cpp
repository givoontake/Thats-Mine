#include "grass_placement.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "LandscapeComponent.h"
#include "LandscapeDataAccess.h"
#include "LandscapeGrassType.h"
#include "LandscapeProxy.h"
#include "LandscapeUtils.h"
#include "Math/Halton.h"
#include "Misc/Crc.h"

static bool IsPlacedVariety(const FGrassVariety& variety)
{
	return variety.GrassMesh && variety.GetDensity() > 0.0f && variety.GetEndCullDistance() > 0;
}

TSet<FString> CollectGrassMeshNames(UWorld* world)
{
	check(world);
	TSet<FString> names;
	for (TActorIterator<ALandscapeProxy> iterator(world); iterator; ++iterator)
	{
		for (ULandscapeComponent* component : iterator->LandscapeComponents)
		{
			if (!component)
			{
				continue;
			}
			for (const TPair<FName, TObjectPtr<ULandscapeGrassType>>& pair : UE::Landscape::Grass::FilterValidNamedGrassTypes(component->GetNamedGrassTypes()))
			{
				for (const FGrassVariety& variety : pair.Value->GrassVarieties)
				{
					if (IsPlacedVariety(variety))
					{
						names.Add(variety.GrassMesh->GetName());
					}
				}
			}
		}
	}
	return names;
}

FGrassLocations ComputeGrassLocations(UWorld* world, const TSet<FString>& mesh_names)
{
	check(world);
	FGrassLocations result;
	const int32 max_instances_per_component = FMath::Max<int32>(1024, IConsoleManager::Get().FindConsoleVariable(TEXT("grass.MaxInstancesPerComponent"))->GetInt());
	const float density_scale = IConsoleManager::Get().FindConsoleVariable(TEXT("grass.densityScale"))->GetFloat();
	for (TActorIterator<ALandscapeProxy> iterator(world); iterator; ++iterator)
	{
		ALandscapeProxy* proxy = *iterator;
		const FVector draw_scale = proxy->GetRootComponent()->GetRelativeScale3D();
		const FIntPoint landscape_section_offset = proxy->GetSectionBase();
		const FMatrix landscape_to_world = proxy->GetRootComponent()->GetComponentTransform().ToMatrixNoScale();
		for (ULandscapeComponent* component : proxy->LandscapeComponents)
		{
			if (!component)
			{
				continue;
			}
			const FLandscapeComponentGrassData& grass_data = *component->GrassData;
			const int32 stride = component->ComponentSizeQuads + 1;
			if (grass_data.NumElements != stride * stride || grass_data.HeightWeightData.IsEmpty())
			{
				// 가중치가 전부 0이라 비워 둔 조각(원소 수 0)은 풀이 없는 정상 조각이다.
				result.invalid_component_count += grass_data.NumElements == 0 ? 0 : 1;
				continue;
			}
			const uint16* heights = reinterpret_cast<const uint16*>(grass_data.HeightWeightData.GetData());
			const FIntPoint section_base = component->GetSectionBase();

			// 아래는 엔진 FAsyncGrassBuilder::Build의 위치 계산을 그대로 옮긴 것이다. 난수 소비 순서도 같아야 위치가 같다.
			for (const TPair<FName, TObjectPtr<ULandscapeGrassType>>& pair : UE::Landscape::Grass::FilterValidNamedGrassTypes(component->GetNamedGrassTypes()))
			{
				const int32* weight_offset = grass_data.WeightOffsets.Find(pair.Key);
				const uint8* weights = weight_offset ? &grass_data.HeightWeightData[*weight_offset] : nullptr;
				const ULandscapeGrassType* grass_type = pair.Value;
				int32 variety_index = -1;
				uint32 halton_base_index = 1;
				for (const FGrassVariety& variety : grass_type->GrassVarieties)
				{
					++variety_index;
					if (!IsPlacedVariety(variety))
					{
						continue;
					}
					const float density = variety.GetDensity() * (grass_type->bEnableDensityScaling ? density_scale : 1.0f);
					const FVector full_extent(draw_scale.X * component->ComponentSizeQuads, draw_scale.Y * component->ComponentSizeQuads, 0.0f);
					const int32 full_sqrt_max_instances = FMath::CeilToInt32(FMath::Sqrt(FMath::Abs(full_extent.X * full_extent.Y * density / 1000.0f / 1000.0f)));
					int32 sqrt_subsections = 1;
					if (full_sqrt_max_instances > 0)
					{
						sqrt_subsections = FMath::Clamp<int32>(FMath::CeilToInt(float(full_sqrt_max_instances) / FMath::Sqrt((float)max_instances_per_component)), 1, 16);
					}
					const int32 max_instances_sub = FMath::Square(full_sqrt_max_instances / sqrt_subsections);
					const bool use_halton = !variety.bUseGrid;
					const FString mesh_name = variety.GrassMesh->GetName();
					// 고르지 않은 종류는 계산하지 않되, 다음 종류의 할턴 시작 번호가 같도록 하위 구역 수만큼 번호를 넘긴다.
					if (!mesh_names.Contains(mesh_name))
					{
						halton_base_index += use_halton ? sqrt_subsections * sqrt_subsections * max_instances_sub : 0;
						continue;
					}
					TArray<FVector>& locations = result.locations.FindOrAdd(mesh_name);

					for (int32 sub_x = 0; sub_x < sqrt_subsections; ++sub_x)
					{
						for (int32 sub_y = 0; sub_y < sqrt_subsections; ++sub_y)
						{
							uint32 halton_index = 0;
							if (use_halton)
							{
								halton_base_index += max_instances_sub;
								halton_index = halton_base_index - max_instances_sub;
							}
							if (full_sqrt_max_instances == 0 || !weights)
							{
								continue;
							}
							FVector origin(draw_scale.X * section_base.X, draw_scale.Y * section_base.Y, 0.0f);
							FVector extent = full_extent;
							int32 sqrt_max_instances = full_sqrt_max_instances;
							if (sqrt_subsections != 1)
							{
								sqrt_max_instances /= sqrt_subsections;
								extent.X /= sqrt_subsections;
								extent.Y /= sqrt_subsections;
								origin.X += extent.X * sub_x;
								origin.Y += extent.Y * sub_y;
							}

							int32 fol_seed = FCrc::StrCrc32(StringCast<ANSICHAR>(*FString::Printf(TEXT("%s_%s_%d_%d_%d_%d_%d"),
								*pair.Key.ToString().ToLower(), *grass_type->GetName().ToLower(), section_base.X, section_base.Y, sub_x, sub_y, variety_index)).Get());
							if (fol_seed == 0)
							{
								fol_seed++;
							}
							FRandomStream random_stream(fol_seed);

							bool random_scale = false;
							int32 scale_fraction_count = 0;
							switch (variety.Scaling)
							{
							case EGrassScaling::Uniform:
								random_scale = variety.ScaleX.Size() > 0;
								scale_fraction_count = 1;
								break;
							case EGrassScaling::Free:
								random_scale = variety.ScaleX.Size() > 0 || variety.ScaleY.Size() > 0 || variety.ScaleZ.Size() > 0;
								scale_fraction_count = 3;
								break;
							case EGrassScaling::LockXY:
								random_scale = variety.ScaleX.Size() > 0 || variety.ScaleZ.Size() > 0;
								scale_fraction_count = 2;
								break;
							}

							// 엔진 SampleLandscapeAtLocationLocal과 같은 보간으로 가중치와 높이를 구한다.
							auto sample = [&](const FVector& location, FVector& out_location, FVector* out_normal) -> float
							{
								const float test_x = static_cast<float>(location.X / draw_scale.X - section_base.X);
								const float test_y = static_cast<float>(location.Y / draw_scale.Y - section_base.Y);
								const int32 x1 = FMath::Clamp<int32>(FMath::FloorToInt(test_x), 0, stride - 1);
								const int32 y1 = FMath::Clamp<int32>(FMath::FloorToInt(test_y), 0, stride - 1);
								const int32 x2 = FMath::Clamp<int32>(FMath::CeilToInt(test_x), 0, stride - 1);
								const int32 y2 = FMath::Clamp<int32>(FMath::CeilToInt(test_y), 0, stride - 1);
								const float lerp_x = FMath::Fractional(test_x);
								const float lerp_y = FMath::Fractional(test_y);
								auto weight = [&](int32 x, int32 y) { return ((float)weights[x + stride * y]) / 255.f; };
								auto height = [&](int32 x, int32 y) { return LandscapeDataAccess::GetLocalHeight(heights[x + stride * y]); };
								const float out_weight = FMath::Lerp(FMath::Lerp(weight(x1, y1), weight(x2, y1), lerp_x), FMath::Lerp(weight(x1, y2), weight(x2, y2), lerp_x), lerp_y);
								out_location.X = location.X - draw_scale.X * float(landscape_section_offset.X);
								out_location.Y = location.Y - draw_scale.Y * float(landscape_section_offset.Y);
								if (lerp_x < lerp_y)
								{
									const double a = (1.0 - lerp_y);
									const double b = lerp_x;
									const double c = 1.0 - a - b;
									out_location.Z = draw_scale.Z * (height(x1, y1) * a + height(x2, y2) * b + height(x1, y2) * c);
								}
								else
								{
									const double a = (1.0 - lerp_x);
									const double b = lerp_x - lerp_y;
									const double c = 1.0 - a - b;
									out_location.Z = draw_scale.Z * (height(x1, y1) * a + height(x2, y1) * b + height(x2, y2) * c);
								}
								if (out_normal)
								{
									// 엔진은 범위를 자르지 않은 칸 좌표로 꼭짓점을 만들고 높이만 자른 칸에서 읽는다.
									const int32 raw_x1 = FMath::FloorToInt(test_x);
									const int32 raw_y1 = FMath::FloorToInt(test_y);
									const int32 raw_x2 = FMath::CeilToInt(test_x);
									const int32 raw_y2 = FMath::CeilToInt(test_y);
									const FVector p11 = FVector((float)raw_x1, (float)raw_y1, height(x1, y1)) * draw_scale;
									const FVector p22 = FVector((float)raw_x2, (float)raw_y2, height(x2, y2)) * draw_scale;
									if (lerp_x > lerp_y)
									{
										const FVector p21 = FVector((float)raw_x2, (float)raw_y1, height(x2, y1)) * draw_scale;
										*out_normal = (p11 != p21 && p22 != p21) ? FVector(((p22 - p21) ^ (p11 - p21)).GetSafeNormal()) : FVector::UpVector;
									}
									else
									{
										const FVector p12 = FVector((float)raw_x1, (float)raw_y2, height(x1, y2)) * draw_scale;
										*out_normal = (p11 != p12 && p22 != p12) ? FVector(((p11 - p12) ^ (p22 - p12)).GetSafeNormal()) : FVector::UpVector;
									}
								}
								return out_weight;
							};
							auto keep = [&](float weight) { return weight > variety.AllowedDensityRange.Min && weight <= variety.AllowedDensityRange.Max && weight >= random_stream.GetFraction(); };

							// 엔진 GetDefaultScale·GetRandomScale과 같은 크기 계산
							auto default_scale = [&]() -> FVector
							{
								FVector result(variety.ScaleX.Min > 0.0f && FMath::IsNearlyZero(variety.ScaleX.Size()) ? variety.ScaleX.Min : 1.0f,
									variety.ScaleY.Min > 0.0f && FMath::IsNearlyZero(variety.ScaleY.Size()) ? variety.ScaleY.Min : 1.0f,
									variety.ScaleZ.Min > 0.0f && FMath::IsNearlyZero(variety.ScaleZ.Size()) ? variety.ScaleZ.Min : 1.0f);
								if (variety.Scaling == EGrassScaling::Uniform)
								{
									result.Y = result.X;
									result.Z = result.X;
								}
								else if (variety.Scaling == EGrassScaling::LockXY)
								{
									result.Y = result.X;
								}
								return result;
							};
							auto random_scale_of = [&](float weight) -> FVector
							{
								FVector result(1.0f);
								float attenuation = 1.0f;
								if (variety.bWeightAttenuatesMaxScale)
								{
									attenuation = variety.MaxScaleWeightAttenuation < 1.0f ? FMath::Clamp((weight - variety.MaxScaleWeightAttenuation) / (1.0f - variety.MaxScaleWeightAttenuation), 0.0f, 1.0f) : 0.0f;
								}
								switch (variety.Scaling)
								{
								case EGrassScaling::Uniform:
									result.X = variety.ScaleX.Interpolate(random_stream.GetFraction() * attenuation);
									result.Y = result.X;
									result.Z = result.X;
									break;
								case EGrassScaling::Free:
									result.X = variety.ScaleX.Interpolate(random_stream.GetFraction() * attenuation);
									result.Y = variety.ScaleY.Interpolate(random_stream.GetFraction() * attenuation);
									result.Z = variety.ScaleZ.Interpolate(random_stream.GetFraction() * attenuation);
									break;
								case EGrassScaling::LockXY:
									result.X = variety.ScaleX.Interpolate(random_stream.GetFraction() * attenuation);
									result.Y = result.X;
									result.Z = variety.ScaleZ.Interpolate(random_stream.GetFraction() * attenuation);
									break;
								}
								return result;
							};
							TArray<FTransform>& transforms = result.transforms.FindOrAdd(mesh_name);

							const float div = 1.0f / float(sqrt_max_instances);
							if (halton_index)
							{
								const int32 max_num = sqrt_max_instances * sqrt_max_instances;
								for (int32 instance_index = 0; instance_index < max_num; ++instance_index)
								{
									const float halton_x = Halton(static_cast<int32>(instance_index + halton_index), 2);
									const float halton_y = Halton(static_cast<int32>(instance_index + halton_index), 3);
									const FVector location(origin.X + halton_x * extent.X, origin.Y + halton_y * extent.Y, 0.0f);
									FVector location_with_height;
									FVector normal;
									const float weight = sample(location, location_with_height, variety.AlignToSurface ? &normal : nullptr);
									if (!keep(weight))
									{
										continue;
									}
									// 엔진 FAsyncGrassBuilder::Build처럼 크기·회전 난수를 쓰고 표면 방향에 맞춘다.
									const FVector scale = random_scale ? random_scale_of(weight) : default_scale();
									const float rotation = variety.RandomRotation ? random_stream.GetFraction() * 360.0f : 0.0f;
									FMatrix transform = FScaleRotationTranslationMatrix(scale, FRotator(0.0f, rotation, 0.0f), FVector::ZeroVector);
									if (variety.AlignToSurface && !normal.IsNearlyZero())
									{
										const FVector new_z = normal * FMath::Sign(normal.Z);
										const FVector new_x = (FVector(0, -1, 0) ^ new_z).GetSafeNormal();
										const FVector new_y = new_z ^ new_x;
										transform = transform * FMatrix(new_x, new_y, new_z, FVector::ZeroVector);
									}
									transform = transform.ConcatTranslation(location_with_height) * landscape_to_world;
									transforms.Add(FTransform(transform));
									locations.Add(landscape_to_world.TransformPosition(location_with_height));
								}
							}
							else
							{
								// 격자 방식은 첫 번째 단계에서 위치가 정해지고, 크기·회전 난수는 그 뒤에 쓰므로 위치에 영향이 없다.
								const float max_jitter_1d = FMath::Clamp<float>(variety.PlacementJitter, 0.0f, .99f) * div * .5f;
								const FVector max_jitter = FVector(max_jitter_1d, max_jitter_1d, 0.0f) * extent;
								origin += extent * (div * 0.5f);
								for (int32 x_start = 0; x_start < sqrt_max_instances; ++x_start)
								{
									for (int32 y_start = 0; y_start < sqrt_max_instances; ++y_start)
									{
										FVector location(origin.X + float(x_start) * div * extent.X, origin.Y + float(y_start) * div * extent.Y, 0.0f);
										const float first_random = random_stream.GetFraction();
										const float second_random = random_stream.GetFraction();
										location += FVector(first_random * 2.0f - 1.0f, second_random * 2.0f - 1.0f, 0.0f) * max_jitter;
										FVector location_with_height;
										if (keep(sample(location, location_with_height, nullptr)))
										{
											locations.Add(landscape_to_world.TransformPosition(location_with_height));
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}
	return result;
}
