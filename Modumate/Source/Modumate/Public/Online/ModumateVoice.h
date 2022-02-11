// Copyright 2021 Modumate, Inc. All Rights Reserved.

#pragma once

#include "UnrealClasses/ModumateCapability.h"
#include "Net/UnrealNetwork.h"
#include "Containers/Map.h"
#include "DocumentManagement/WebProjectSettings.h"

#if !UE_SERVER
//TODO: Move this out of header in to CPP by (e.g.) wrapping the vivox integration
// in another class - JN
#include "VivoxCore.h"
#endif

#include "ModumateVoice.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(ModumateVoice, Log, All);

USTRUCT()
struct MODUMATE_API FVivoxEndpoint
{
	GENERATED_BODY()

	UPROPERTY()
	FString Issuer;
	UPROPERTY()
	FString Domain;
	UPROPERTY()
	FString Server;

	FVivoxEndpoint() :
		Issuer(TEXT("")),
		Domain(TEXT("")),
		Server(TEXT(""))
	{
	}

	FVivoxEndpoint(const FString& Issuer, const FString& Domain, const FString& Server) :
		Issuer(Issuer),
		Domain(Domain),
		Server(Server)
	{
	}
};

USTRUCT()
struct MODUMATE_API FVivoxParam
{
	GENERATED_BODY()

	UPROPERTY()
	FString Username;
	UPROPERTY()
	FString Channel;
	UPROPERTY()
	FString LoginToken;
	UPROPERTY()
	FString JoinToken;


	FVivoxParam() :
		Username(TEXT("")),
		Channel(TEXT("")),
		LoginToken(TEXT("")),
		JoinToken(TEXT(""))
	{
	}

	FVivoxParam(const FString& Username, const FString& Channel, 
			const FString& LoginToken = TEXT(""), const FString & JoinToken = TEXT("")) :
		Username(Username),
		Channel(Channel),
		LoginToken(LoginToken),
		JoinToken(JoinToken)
	{

	}
};

/// <summary>
/// AModumateVoice is responsible for logging in and joining Vivox voice
/// channels. It does so by generating the required signed tokens
/// on the UE4 server, handing those tokens to the client, and having the
/// client connect to the Vivox APIs through the Plugin & SDK.
/// 
/// For more information around Issuers/Domains and the VIVOX API see:
///			https://developer.vivox.com/apps
/// </summary>
UCLASS()
class MODUMATE_API AModumateVoice : public AModumateCapability
{
	GENERATED_BODY()

public:
	AModumateVoice();
	virtual ~AModumateVoice();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void GetLifetimeReplicatedProps(TArray< FLifetimeProperty >& OutLifetimeProps) const override;

	void Connect(const FString& Username, const FString& ChannelName);
	void Disconnect();

	void SetMuted(bool IsMuted);
	bool GetInputDevices(TMap<FString, FString> &OutInputs) const;
	bool GetOutputDevices(TMap<FString, FString> &OutOutputs) const;
	bool SetInputDevice(const FString& InputDevice);
	bool SetOutputDevice(const FString& OutputDevice);

	void AdjustOutputVolume(int Value);
	void AdjustInputVolume(int Value);
	void JoinChannel(const FString& JoinToken);
	void LeaveChannel();

	bool ToWebProjectSettings(FWebProjectSettings& OutSettings) const;
	bool FromWebProjectSettings(const FWebProjectSettings& InSettings);

	DECLARE_EVENT_TwoParams(AModumateVoice, FTalkingEvent, FString, bool)
	FTalkingEvent& OnTalkingChanged() { return TalkingChangedEvent; }

	DECLARE_EVENT_OneParam(AModumateVoice, FParticipantEvent, FString)
	FParticipantEvent& OnParticipantJoin() { return ParticipantJoinEvent; }
	FParticipantEvent& OnParticipantLeave() { return ParticipantLeaveEvent; }

	DECLARE_EVENT(AModumateVoice, FVoiceDeviceEvent)
	FVoiceDeviceEvent& OnVoiceDevicesChanged() { return VoiceDeviceChangedEvent; }

	UPROPERTY(replicated)
	bool bIsConnectedToChannel = false;
	UPROPERTY()
	bool bIsLocallyOwned = false;

private:

	UFUNCTION(Server, Reliable)
	void SERVER_RequestVoiceLogin(const FVivoxParam& Parameters);

	UFUNCTION(Client, Reliable)
	void CLIENT_AllowVoiceLogin(const FVivoxParam& Parameters, const FVivoxEndpoint& Endpoint);

	FString GenerateToken(const FVivoxEndpoint& Endpoint, const FVivoxParam& Parameters,
	const FString& Request, const FString& Key) const;
	FString HMACSHA256(const FString& Msg, const FString& Key) const;
	void ToBase64URL(FString& Base64) const;
	static FString GenerateAccountURI(const FVivoxEndpoint& Endpoint, const FString& Username);
	static FString GenerateChannelURI(const FVivoxEndpoint& Endpoint, const FString& Channel);

	FTalkingEvent TalkingChangedEvent;
	FParticipantEvent ParticipantJoinEvent;
	FParticipantEvent ParticipantLeaveEvent;
	FVoiceDeviceEvent VoiceDeviceChangedEvent;

	//There is no available Linux SDK for Vivox, so the types defined in that sdk are isolated
	// to this block.
#if !UE_SERVER

	void SetAudioDevices(const IAudioDevice& InputToSet, const IAudioDevice& OutputToSet);
	void OnDeviceAdded(const IAudioDevice& Device);
	void OnDeviceRemoved(const IAudioDevice& Device);
	void OnDeviceChanged(const IAudioDevice& Device);

	void OnParticipantUpdated(const IParticipant& Participant);
	void OnParticipantAdded(const IParticipant& Participant);
	void OnParticipantRemoved(const IParticipant& Participant);

	void OnLoginSessionStateChanged(LoginState State);
	void OnChannelSessionConnectionStateChanged(const IChannelConnectionState& State);

	ILoginSession::FOnBeginLoginCompletedDelegate OnBeginLoginCompleted;

	AccountId VivoxAccountId;
	ChannelId CurrentChannel;

	//This memory is handled by vivox
	IClient* VoiceClient;
	FVivoxCoreModule* VivoxCoreModule;
	ILoginSession* LoginSession;
	IChannelSession* ChannelSession;
#else
	//These values come from: https://developer.vivox.com/apps
	const FString VIVOX_ISSUER = TEXT("moduma4241-mo84-dev");
	const FString VIVOX_DOMAIN = TEXT("mt1s.vivox.com");
	const FString VIVOX_SERVER = TEXT("https://mt1s.www.vivox.com/api2");
	const FString VIVOX_KEY = TEXT("just705"); //Should only add the key to the server for security reasons
#endif
};