// Copyright Epic Games, Inc. All Rights Reserved.

#include "ProxyRegistrationBeaconHostObject.h"
#include "ProxyRegistrationBeaconClient.h"
#include "MultiServerProxy.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/NetConnection.h"
#include "OnlineError.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ProxyRegistrationBeaconHostObject)

DEFINE_LOG_CATEGORY_STATIC(LogProxyRegistrationHost, Log, All);

// ─── AProxyRegistrationBeaconHost ────────────────────────────────────────────

AProxyRegistrationBeaconHost::AProxyRegistrationBeaconHost()
{
	bAuthRequired = true;
}

bool AProxyRegistrationBeaconHost::StartVerifyAuthentication(
	const FUniqueNetId& PlayerId,
	const FString& LoginOptions,
	const FString& AuthenticationToken,
	const FOnAuthenticationVerificationCompleteDelegate& OnComplete)
{
	OnComplete.ExecuteIfBound(FOnlineError::Success());
	return true;
}

// ─── AProxyRegistrationBeaconHostObject ──────────────────────────────────────

AProxyRegistrationBeaconHostObject::AProxyRegistrationBeaconHostObject(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ClientBeaconActorClass = AProxyRegistrationBeaconClient::StaticClass();
	BeaconTypeName = ClientBeaconActorClass->GetName();
}

void AProxyRegistrationBeaconHostObject::OnClientConnected(AOnlineBeaconClient* NewClientActor, UNetConnection* ClientConnection)
{
	Super::OnClientConnected(NewClientActor, ClientConnection);

	UE_LOG(LogProxyRegistrationHost, Log, TEXT("Game server beacon client connected (%d total)"), ClientActors.Num());

	// Extract the game server address and DSTM info from the login URL that the
	// beacon client injected via GetLoginOptions(). The beacon host stored it in
	// Connection->RequestURL during NMT_Login handling.
	if (!ClientConnection)
	{
		return;
	}

	const int32 QMark = ClientConnection->RequestURL.Find(TEXT("?"));
	const FString Options = (QMark != INDEX_NONE) ? ClientConnection->RequestURL.Mid(QMark) : FString();
	const FString GameServerAddress = UGameplayStatics::ParseOption(Options, TEXT("GameServerAddress"));

	if (GameServerAddress.IsEmpty())
	{
		UE_LOG(LogProxyRegistrationHost, Error, TEXT("No GameServerAddress in beacon login URL: %s"), *ClientConnection->RequestURL);
		return;
	}

	// Derive the DSTM reachable address from the game server's host + DSTMListenPort.
	FString DSTMAddress;
	const FString DSTMListenPortStr = UGameplayStatics::ParseOption(Options, TEXT("DSTMListenPort"));
	if (!DSTMListenPortStr.IsEmpty())
	{
		FString Host, PortStr;
		if (GameServerAddress.Split(TEXT(":"), &Host, &PortStr))
		{
			DSTMAddress = FString::Printf(TEXT("%s:%s"), *Host, *DSTMListenPortStr);
		}
	}

	const FString ServerId = UGameplayStatics::ParseOption(Options, TEXT("DedicatedServerId"));
	UE_LOG(LogProxyRegistrationHost, Log, TEXT("Parsed beacon login: server=%s addr=%s DSTMAddr=%s"),
		*ServerId, *GameServerAddress, *DSTMAddress);

	HandleGameServerRegistration(GameServerAddress, DSTMAddress, NewClientActor);
}

void AProxyRegistrationBeaconHostObject::NotifyClientDisconnected(AOnlineBeaconClient* LeavingClientActor)
{
	Super::NotifyClientDisconnected(LeavingClientActor);

	const int32 Removed = RegisteredDSTMPeers.RemoveAll(
		[LeavingClientActor](const FRegisteredDSTMPeer& P) { return P.BeaconClient == LeavingClientActor; });

	UE_LOG(LogProxyRegistrationHost, Log,
		TEXT("Game server beacon client disconnected (%d remaining, %d DSTM peer(s) removed)"),
		ClientActors.Num(), Removed);
}

void AProxyRegistrationBeaconHostObject::HandleGameServerRegistration(
	const FString& GameServerAddress, const FString& DSTMAddress, AOnlineBeaconClient* BeaconClient)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogProxyRegistrationHost, Error, TEXT("No world — cannot register game server"));
		return;
	}

	UProxyNetDriver* ProxyDriver = Cast<UProxyNetDriver>(World->GetNetDriver());
	if (!ProxyDriver)
	{
		UE_LOG(LogProxyRegistrationHost, Error, TEXT("World net driver is not UProxyNetDriver — cannot register game server"));
		return;
	}

	FURL GameServerURL(nullptr, *GameServerAddress, ETravelType::TRAVEL_Absolute);
	if (!GameServerURL.Valid)
	{
		UE_LOG(LogProxyRegistrationHost, Error, TEXT("Invalid game server URL: %s"), *GameServerAddress);
		return;
	}

	// 1. Collect existing DSTM peer addresses BEFORE adding the new server.
	TArray<FString> ExistingDSTMPeers;
	for (const FRegisteredDSTMPeer& Peer : RegisteredDSTMPeers)
	{
		if (!Peer.DSTMAddress.IsEmpty())
		{
			ExistingDSTMPeers.Add(Peer.DSTMAddress);
		}
	}

	// 2. Register the game server with the proxy net driver for game traffic.
	UE_LOG(LogProxyRegistrationHost, Log, TEXT("Registering game server via beacon: %s"), *GameServerAddress);
	ProxyDriver->RegisterGameServerAndConnectClients(GameServerURL);

	// 3. Store the new server's DSTM peer info.
	if (!DSTMAddress.IsEmpty())
	{
		FRegisteredDSTMPeer NewPeer;
		NewPeer.BeaconClient = BeaconClient;
		NewPeer.DSTMAddress = DSTMAddress;
		RegisteredDSTMPeers.Add(NewPeer);
	}

	// 4. Send the existing peer list to the new server via Client RPC.
	//    The new server creates its DSTM mesh with these peers and connects
	//    outbound; existing servers accept the incoming connections via their
	//    DSTM beacon hosts (no broadcast needed).
	AProxyRegistrationBeaconClient* RegClient = Cast<AProxyRegistrationBeaconClient>(BeaconClient);
	if (RegClient && !DSTMAddress.IsEmpty())
	{
		UE_LOG(LogProxyRegistrationHost, Log,
			TEXT("Sending %d existing DSTM peer(s) to new server (DSTM: %s)"),
			ExistingDSTMPeers.Num(), *DSTMAddress);
		RegClient->ClientReceiveDSTMPeers(ExistingDSTMPeers);
	}
}
