// Copyright Epic Games, Inc. All Rights Reserved.

#include "ProxyRegistrationBeaconClient.h"
#include "DSTMSubsystem.h"
#include "Engine/GameInstance.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ProxyRegistrationBeaconClient)

DEFINE_LOG_CATEGORY_STATIC(LogProxyRegistrationClient, Log, All);

AProxyRegistrationBeaconClient::AProxyRegistrationBeaconClient()
{
}

void AProxyRegistrationBeaconClient::SetDSTMInfo(const FString& InDedicatedServerId, int32 InDSTMListenPort)
{
	DedicatedServerId = InDedicatedServerId;
	DSTMListenPort = InDSTMListenPort;
}

FString AProxyRegistrationBeaconClient::GetLoginOptions(const FUniqueNetIdRepl& PlayerId)
{
	// Append game server address and DSTM info to the login URL so the proxy
	// can extract them from Connection->RequestURL when OnClientConnected fires.
	FString Base = Super::GetLoginOptions(PlayerId);
	if (!GameServerAddress.IsEmpty())
	{
		Base += FString::Printf(TEXT("?GameServerAddress=%s"), *GameServerAddress);
	}
	if (!DedicatedServerId.IsEmpty())
	{
		Base += FString::Printf(TEXT("?DedicatedServerId=%s"), *DedicatedServerId);
	}
	if (DSTMListenPort > 0)
	{
		Base += FString::Printf(TEXT("?DSTMListenPort=%d"), DSTMListenPort);
	}
	return Base;
}

void AProxyRegistrationBeaconClient::ClientReceiveDSTMPeers_Implementation(const TArray<FString>& PeerDSTMAddresses)
{
	UE_LOG(LogProxyRegistrationClient, Log,
		TEXT("Received %d DSTM peer(s) from proxy"), PeerDSTMAddresses.Num());
	for (const FString& Peer : PeerDSTMAddresses)
	{
		UE_LOG(LogProxyRegistrationClient, Log, TEXT("  DSTM Peer: %s"), *Peer);
	}

	// This RPC runs on the game server's replicated copy of the beacon client
	// actor. Read local DSTM config from the command line (these are
	// server-local values that don't need to travel over the network).
	UWorld* World = GetWorld();
	if (!World) return;

	UGameInstance* GI = World->GetGameInstance();
	if (!GI) return;

	UDSTMSubsystem* DSTM = GI->GetSubsystem<UDSTMSubsystem>();
	if (!DSTM)
	{
		UE_LOG(LogProxyRegistrationClient, Error, TEXT("DSTMSubsystem not found — cannot create DSTM mesh"));
		return;
	}

	if (DSTM->IsMeshActive())
	{
		UE_LOG(LogProxyRegistrationClient, Log, TEXT("DSTM mesh already active — ignoring proxy peer list"));
		return;
	}

	FString LocalPeerId;
	if (!FParse::Value(FCommandLine::Get(), TEXT("-DedicatedServerId="), LocalPeerId, false))
	{
		UE_LOG(LogProxyRegistrationClient, Error, TEXT("No -DedicatedServerId= on command line — cannot create DSTM mesh"));
		return;
	}

	FString ListenIp = TEXT("0.0.0.0");
	FParse::Value(FCommandLine::Get(), TEXT("-DSTMListenIp="), ListenIp, false);

	int32 ListenPort = 16000;
	FParse::Value(FCommandLine::Get(), TEXT("-DSTMListenPort="), ListenPort);

	UE_LOG(LogProxyRegistrationClient, Log,
		TEXT("Creating DSTM mesh from proxy peer list: Id=%s, ListenPort=%d, Peers=%d"),
		*LocalPeerId, ListenPort, PeerDSTMAddresses.Num());

	DSTM->InitializeDSTMMesh(LocalPeerId, ListenIp, ListenPort, PeerDSTMAddresses);
}

void AProxyRegistrationBeaconClient::OnConnected()
{
	Super::OnConnected();
	UE_LOG(LogProxyRegistrationClient, Log,
		TEXT("Connected to proxy registration beacon (address: %s, serverId: %s, DSTMPort: %d)"),
		*GameServerAddress, *DedicatedServerId, DSTMListenPort);
}

void AProxyRegistrationBeaconClient::OnFailure()
{
	UE_LOG(LogProxyRegistrationClient, Error, TEXT("Lost connection to proxy registration beacon — requesting graceful shutdown"));
	Super::OnFailure();

	FPlatformMisc::RequestExit(false);
}
