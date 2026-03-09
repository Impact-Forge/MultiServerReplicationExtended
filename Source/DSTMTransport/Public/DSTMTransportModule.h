// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

#if UE_WITH_REMOTE_OBJECT_HANDLE
#include "UObject/RemoteObjectTransfer.h"
#include "UObject/RemoteObjectTypes.h"
#endif

/**
 * Module for the DSTM Transport plugin.
 *
 * Responsibilities:
 *   1. Initialize FRemoteServerId for this server instance (from -DedicatedServerId=)
 *   2. Pre-bind the DSTM transport delegates BEFORE InitRemoteObjects() runs,
 *      routing serialized migration data through our beacon mesh instead of disk I/O.
 *
 * Timing:
 *   StartupModule() runs during module load, before any UWorld is created.
 *   InitRemoteObjects() runs during world initialization.
 *   → Our delegate bindings win over the default disk-based fallbacks
 *     (RemoteObject.cpp checks !IsBound() before applying defaults).
 *
 * See SEAMLESS.md, Approach 2, Steps 2 & 4.
 */
class FDSTMTransportModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	static FDSTMTransportModule& Get();

private:
	/** Parse -DedicatedServerId= and call FRemoteServerId::InitGlobalServerId(). */
	void InitializeServerIdentity();

	/** Bind RemoteObjectTransferDelegate and RequestRemoteObjectDelegate. */
	void BindTransportDelegates();

#if UE_WITH_REMOTE_OBJECT_HANDLE
	/**
	 * Static callback for RemoteObjectTransferDelegate.
	 * Called by the engine when an object is ready to be sent to a remote server.
	 * Finds the DSTMSubsystem and routes serialized data through the beacon mesh.
	 */
	static void OnRemoteObjectTransfer(const UE::RemoteObject::Transfer::FMigrateSendParams& Params);

	/**
	 * Static callback for RequestRemoteObjectDelegate.
	 * Called when a server needs to pull an object from another server.
	 * Routes the request through the beacon mesh.
	 */
	static void OnRequestRemoteObject(
		FRemoteWorkPriority Priority,
		FRemoteObjectId ObjectId,
		FRemoteServerId LastKnownServerId,
		FRemoteServerId DestServerId);
#endif

	/** Whether we successfully initialized the server identity. */
	bool bServerIdentityInitialized = false;
};
