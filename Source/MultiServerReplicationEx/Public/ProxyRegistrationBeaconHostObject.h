// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OnlineBeaconHostObject.h"
#include "OnlineBeaconHost.h"
#include "ProxyRegistrationBeaconHostObject.generated.h"

class AProxyRegistrationBeaconClient;

/**
 * Beacon host subclass that enables the NMT_Login handshake flow.
 *
 * By default AOnlineBeaconHost skips NMT_Login when bAuthRequired is false,
 * which means Connection->RequestURL is never populated. We need it
 * so the game server can pass its listen address via GetLoginOptions().
 */
UCLASS(transient, notplaceable)
class AProxyRegistrationBeaconHost : public AOnlineBeaconHost
{
	GENERATED_BODY()
public:
	AProxyRegistrationBeaconHost();

	virtual bool StartVerifyAuthentication(
		const FUniqueNetId& PlayerId,
		const FString& LoginOptions,
		const FString& AuthenticationToken,
		const FOnAuthenticationVerificationCompleteDelegate& OnComplete) override;
};

/** Tracks a game server registered with the proxy, including its DSTM beacon address. */
struct FRegisteredDSTMPeer
{
	AOnlineBeaconClient* BeaconClient = nullptr;
	FString DSTMAddress; // host:port — derived from GameServerAddress host + DSTMListenPort
};

/**
 * Beacon host object that runs on the proxy server.
 *
 * When a game server's AProxyRegistrationBeaconClient connects and sends its
 * address + DSTM info, this object:
 *   1. Collects existing peers' DSTM addresses
 *   2. Registers the game server with the proxy's UProxyNetDriver
 *   3. Stores the new server's DSTM address
 *   4. Sends the peer list to the new server via ClientReceiveDSTMPeers()
 *
 * This lets the proxy orchestrate the DSTM mesh topology so game servers
 * don't need -DSTMPeers= on their command line.
 */
UCLASS(transient, notplaceable)
class AProxyRegistrationBeaconHostObject : public AOnlineBeaconHostObject
{
	GENERATED_BODY()

public:
	AProxyRegistrationBeaconHostObject(const FObjectInitializer& ObjectInitializer);

	//~ Begin AOnlineBeaconHostObject interface
	virtual void OnClientConnected(AOnlineBeaconClient* NewClientActor, UNetConnection* ClientConnection) override;
	virtual void NotifyClientDisconnected(AOnlineBeaconClient* LeavingClientActor) override;
	//~ End AOnlineBeaconHostObject interface

private:
	/** Register a game server with the proxy and send it the current DSTM peer list. */
	void HandleGameServerRegistration(const FString& GameServerAddress, const FString& DSTMAddress, AOnlineBeaconClient* BeaconClient);

	/** DSTM peers tracked for handing the full peer list to new joiners. */
	TArray<FRegisteredDSTMPeer> RegisteredDSTMPeers;
};
