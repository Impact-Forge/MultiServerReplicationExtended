// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class DSTMTransport : ModuleRules
{
	public DSTMTransport(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			// MultiServer beacon infrastructure (AMultiServerBeaconClient, UMultiServerNode)
			"MultiServerReplication",

			// AOnlineBeaconClient base class
			"OnlineSubsystemUtils"
		});
	}
}
