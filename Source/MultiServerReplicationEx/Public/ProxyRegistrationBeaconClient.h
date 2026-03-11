// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OnlineBeaconClient.h"
#include "ProxyRegistrationBeaconClient.generated.h"

/**
 * Beacon client used by game servers to register with a proxy and join the DSTM mesh.
 *
 * The game server passes its listen address and DSTM info to the proxy via
 * GetLoginOptions(), which injects them into the NMT_Login URL. The proxy
 * extracts them in OnClientConnected, registers the game server, and sends
 * back the DSTM peer list via ClientReceiveDSTMPeers so the game server
 * can create its DSTM mesh with all existing peers.
 */
UCLASS(transient, notplaceable)
class MULTISERVERREPLICATIONEX_API AProxyRegistrationBeaconClient : public AOnlineBeaconClient
{
	GENERATED_BODY()

public:
	AProxyRegistrationBeaconClient();

	/** Set the address (host:port) this game server is listening on. Must be called before connecting. */
	void SetGameServerAddress(const FString& InAddress) { GameServerAddress = InAddress; }

	/** Set the DSTM info for this game server. Must be called before connecting. */
	void SetDSTMInfo(const FString& InDedicatedServerId, int32 InDSTMListenPort);

	//~ Begin AOnlineBeaconClient interface
	virtual FString GetLoginOptions(const FUniqueNetIdRepl& PlayerId) override;
	virtual void OnConnected() override;
	virtual void OnFailure() override;
	//~ End AOnlineBeaconClient interface

	/**
	 * Client RPC: proxy sends DSTM peer addresses after registration.
	 * The game server creates its DSTM mesh with these peers to join the mesh.
	 */
	UFUNCTION(Client, Reliable)
	void ClientReceiveDSTMPeers(const TArray<FString>& PeerDSTMAddresses);

private:
	/** The host:port address this game server can be reached at. */
	FString GameServerAddress;

	/** This server's DedicatedServerId (passed to proxy in login URL). */
	FString DedicatedServerId;

	/** Port for the DSTM beacon listener on this game server. */
	int32 DSTMListenPort = 16000;
};
