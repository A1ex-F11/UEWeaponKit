/**
 * 自动化测试用的临时 Game World。
 * 不创建音频、物理、导航和 AI。析构时销毁世界和引擎上下文。
 */
#pragma once

#include "CoreMinimal.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

/**
 * 在栈上创建一对 BeginPlay 过的测试世界。
 * 运行时和背包测试用它生成武器、Pawn 和组件，不依赖关卡资源。
 */
struct FWeaponKitTestWorld
{
	/** 创建出的世界。失败时为空。 */
	UWorld* World = nullptr;

	/** 创建 Game 世界、登记引擎上下文，并对 Actor 执行 InitializeActorsForPlay 和 BeginPlay。 */
	FWeaponKitTestWorld()
	{
		UWorld::InitializationValues Values;
		Values.AllowAudioPlayback(false)
			.RequiresHitProxies(false)
			.CreatePhysicsScene(false)
			.CreateNavigation(false)
			.CreateAISystem(false)
			.ShouldSimulatePhysics(false);
		World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, false, ERHIFeatureLevel::Num, &Values);
		if (World && GEngine)
		{
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
		}
		if (World)
		{
			World->InitializeActorsForPlay(FURL());
			World->BeginPlay();
		}
	}

	/** 销毁世界并移除引擎上下文。测试用例结束时随栈对象自动调用。 */
	~FWeaponKitTestWorld()
	{
		if (World)
		{
			World->DestroyWorld(false);
			if (GEngine)
			{
				GEngine->DestroyWorldContext(World);
			}
			World = nullptr;
		}
	}

	/** 返回临时世界，创建失败时为空。 */
	UWorld* Get() const { return World; }
};
