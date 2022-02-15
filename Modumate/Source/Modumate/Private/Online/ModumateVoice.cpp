// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "Online/ModumateVoice.h"
#include "Misc/Base64.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "picohash.h"

DEFINE_LOG_CATEGORY(ModumateVoice);

#if !UE_SERVER
AModumateVoice::AModumateVoice() :
	AModumateCapability(),
	bIsConnectedToChannel(false),
	VivoxAccountId(AccountId()),
	CurrentChannel(ChannelId(TEXT("empty"), TEXT("empty"), TEXT("empty"))),
	VoiceClient(nullptr),
	VivoxCoreModule(nullptr),
	LoginSession(nullptr),
	ChannelSession(nullptr)
{
	bReplicates = true;
	bAlwaysRelevant = true;
	bNetUseOwnerRelevancy = false;
	bIsLocallyOwned = false;
}
#else
AModumateVoice::AModumateVoice() :
	AModumateCapability(),
	bIsConnectedToChannel(false)
{
	bReplicates = true;
	bAlwaysRelevant = true;
	bNetUseOwnerRelevancy = false;
}
#endif


AModumateVoice::~AModumateVoice()
{
	UE_LOG(ModumateVoice, Log, TEXT("ModumateVoice Collected"));
}

void AModumateVoice::BeginPlay()
{
	Super::BeginPlay();
}

void AModumateVoice::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
}

void AModumateVoice::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AModumateVoice, bIsConnectedToChannel);
}

void AModumateVoice::Connect(const FString& Username, const FString& ChannelName)
{
#if !UE_SERVER

	auto playerController = Cast<AEditModelPlayerController>(GetOwner());
	if (playerController && playerController->IsLocalController())
	{
		bIsLocallyOwned = true;
		VivoxCoreModule = static_cast<FVivoxCoreModule*>(&FModuleManager::Get().LoadModuleChecked(TEXT("VivoxCore")));
		VoiceClient = &VivoxCoreModule->VoiceClient();

		if (VoiceClient->Initialize() != VxErrorAlreadyInitialized)
		{
			UE_LOG(ModumateVoice, Log, TEXT("Binding Events"));
			VoiceClient->AudioInputDevices().EventAfterDeviceAvailableAdded.AddUObject(this, &AModumateVoice::OnDeviceAdded);
			VoiceClient->AudioOutputDevices().EventAfterDeviceAvailableAdded.AddUObject(this, &AModumateVoice::OnDeviceAdded);

			VoiceClient->AudioInputDevices().EventBeforeAvailableDeviceRemoved.AddUObject(this, &AModumateVoice::OnDeviceRemoved);
			VoiceClient->AudioOutputDevices().EventBeforeAvailableDeviceRemoved.AddUObject(this, &AModumateVoice::OnDeviceRemoved);

			VoiceClient->AudioInputDevices().EventEffectiveDeviceChanged.AddUObject(this, &AModumateVoice::OnDeviceChanged);
			VoiceClient->AudioOutputDevices().EventEffectiveDeviceChanged.AddUObject(this, &AModumateVoice::OnDeviceChanged);
		}

		UE_LOG(ModumateVoice, Log, TEXT("Voice Client Instantiated\n"));
	}

	if (bIsLocallyOwned)
	{
		if (bIsConnectedToChannel)
		{
			UE_LOG(ModumateVoice, Warning, TEXT("Disconnecting from current channel"));
			LeaveChannel();
		}

		UE_LOG(ModumateVoice, Log, TEXT("Vivox login, username=%s, channelName=%s\n"), *Username, *ChannelName);
		auto params = FVivoxParam(Username, ChannelName);
		SERVER_RequestVoiceLogin(params);
	}
#endif
}

void AModumateVoice::Disconnect()
{
#if !UE_SERVER
	if (bIsLocallyOwned)
	{
		UE_LOG(ModumateVoice, Log, TEXT("Vivox logout\n"));
		VoiceClient->Uninitialize();
		VivoxCoreModule->ShutdownModule();
	}
#endif
}

void AModumateVoice::SetMuted(bool IsMuted)
{
#if !UE_SERVER
	if (IsMuted)
	{
		LoginSession->SetTransmissionMode(TransmissionMode::None);
	}
	else
	{
		LoginSession->SetTransmissionMode(TransmissionMode::All);
	}
#endif
}

bool AModumateVoice::GetInputDevices(TMap<FString, FString> &OutInputs) const
{
#if !UE_SERVER
	OutInputs.Empty();
	TArray<FString> keys;
	auto devices = VoiceClient->AudioInputDevices().AvailableDevices();
	devices.GetKeys(keys);
	for (auto k : keys)
	{
		FString name = devices[k]->Name();
		OutInputs.Add(name,k);
	}
	return true;
#else
	return false;
#endif
}

bool AModumateVoice::GetOutputDevices(TMap<FString, FString> &OutOutputs) const
{
#if !UE_SERVER
	OutOutputs.Empty();
	TArray<FString> keys;
	auto devices = VoiceClient->AudioOutputDevices().AvailableDevices();
	devices.GetKeys(keys);
	for (auto k : keys)
	{
		FString name = devices[k]->Name();
		OutOutputs.Add(name,k);
}
	return true;
#else
	return false;
#endif
}

bool AModumateVoice::SetInputDevice(const FString& InputDevice)
{
#if !UE_SERVER
	if (VoiceClient->AudioInputDevices().AvailableDevices().Contains(InputDevice))
	{
		IAudioDevice* device = VoiceClient->AudioInputDevices().AvailableDevices()[InputDevice];
		VoiceClient->AudioInputDevices().SetActiveDevice(*device);
		return true;
	}
#endif
	return false;
}

bool AModumateVoice::SetOutputDevice(const FString& OutputDevice)
{
#if !UE_SERVER
	if (VoiceClient->AudioOutputDevices().AvailableDevices().Contains(OutputDevice))
	{
		IAudioDevice* device = VoiceClient->AudioOutputDevices().AvailableDevices()[OutputDevice];
		VoiceClient->AudioOutputDevices().SetActiveDevice(*device);
		return true;
	}
#endif
	return false;
}

void AModumateVoice::AdjustOutputVolume(int Value)
{
#if !UE_SERVER
	if (bIsLocallyOwned)
	{
		VoiceClient->AudioOutputDevices().SetVolumeAdjustment(Value);
	}
#endif
}

void AModumateVoice::AdjustInputVolume(int Value)
{
#if !UE_SERVER
	if (bIsLocallyOwned)
	{
		VoiceClient->AudioInputDevices().SetVolumeAdjustment(Value);
	}
#endif
}

void AModumateVoice::JoinChannel(const FString& JoinToken)
{
#if !UE_SERVER
	if (bIsLocallyOwned)
	{
		ChannelSession = &(LoginSession->GetChannelSession(CurrentChannel));
		ChannelSession->EventChannelStateChanged.AddUObject(this, &AModumateVoice::OnChannelSessionConnectionStateChanged);
		ChannelSession->EventBeforeParticipantRemoved.AddUObject(this, &AModumateVoice::OnParticipantRemoved);
		ChannelSession->EventAfterParticipantAdded.AddUObject(this, &AModumateVoice::OnParticipantAdded);

		UE_LOG(ModumateVoice, Log, TEXT("Joining Vivox Channel: %s\n"), *(CurrentChannel.Name()));
		IChannelSession::FOnBeginConnectCompletedDelegate OnBeginConnectCompleted;
		OnBeginConnectCompleted.BindLambda([this](VivoxCoreError Error)
			{
				if (VxErrorSuccess == Error)
				{
					UE_LOG(ModumateVoice, Log, TEXT("Channel join initiated."));
				}
				else
				{
					UE_LOG(ModumateVoice, Warning, TEXT("Channel join failed.."));
				}
			});

		auto vivoxReturn = ChannelSession->BeginConnect(true, true, true, JoinToken, OnBeginConnectCompleted);
		if (vivoxReturn != VxErrorSuccess)
		{
			UE_LOG(ModumateVoice, Error, TEXT("Failed to connect to channel: %d"), vivoxReturn);
		}

		SetMuted(true);
	}
#endif
}

void AModumateVoice::LeaveChannel()
{
#if !UE_SERVER
	if (bIsLocallyOwned)
	{
		if (bIsConnectedToChannel)
		{
			UE_LOG(ModumateVoice, Log, TEXT("Disconnecting from Vivox Channel"));
			LoginSession->GetChannelSession(CurrentChannel).Disconnect();
		}
		bIsConnectedToChannel = false;
	}
#endif
}

bool AModumateVoice::ToWebProjectSettings(FWebProjectSettings& OutSettings) const
{
#if UE_SERVER
	return false;
#endif
	OutSettings.microphone.value = VoiceClient->AudioInputDevices().ActiveDevice().Name();
	TMap<FString, FString> inputs;
	GetInputDevices(inputs);
	for (auto kvp : inputs)
	{
		OutSettings.microphone.options.Add(FWebProjectSettingsPropertyOption(kvp.Key, kvp.Key));
	}

	OutSettings.speaker.value = VoiceClient->AudioOutputDevices().ActiveDevice().Name();
	TMap<FString, FString> outputs;
	GetOutputDevices(outputs);
	for (auto kvp : outputs)
	{
		OutSettings.speaker.options.Add(FWebProjectSettingsPropertyOption(kvp.Key, kvp.Key));
	}
	return true;
}

bool AModumateVoice::FromWebProjectSettings(const FWebProjectSettings& InSettings)
{
#if UE_SERVER
	return false;
#endif
	bool bInputFound = false;
	TMap<FString, FString> inputs;
	if (GetInputDevices(inputs))
	{
		bInputFound = SetInputDevice(inputs[InSettings.microphone.value]);
	}
	bool bOutputFound = false;
	TMap<FString, FString> outputs;
	if (GetOutputDevices(outputs))
	{
		bOutputFound = SetOutputDevice(outputs[InSettings.speaker.value]);
	}
	return bInputFound && bOutputFound;
}

void AModumateVoice::SERVER_RequestVoiceLogin_Implementation(const FVivoxParam& Parameters)
{
#if UE_SERVER
	UE_LOG(ModumateVoice, Warning, TEXT("Got request for Voice login from: %s"), *(Parameters.Username));

	FVivoxEndpoint endpoint = FVivoxEndpoint(VIVOX_ISSUER, VIVOX_DOMAIN, VIVOX_SERVER);

	FString loginToken = GenerateToken(endpoint, Parameters, "login", VIVOX_KEY);
	FString joinToken = GenerateToken(endpoint, Parameters, "join", VIVOX_KEY);

	FVivoxParam populatedParameters(Parameters.Username, Parameters.Channel, loginToken, joinToken);

	CLIENT_AllowVoiceLogin(populatedParameters, endpoint);
#endif
}

void AModumateVoice::CLIENT_AllowVoiceLogin_Implementation(const FVivoxParam& Parameters, const FVivoxEndpoint& Endpoint)
{
#if !UE_SERVER
	if (!bIsLocallyOwned)
	{
		return;
	}

	UE_LOG(ModumateVoice, Log, TEXT("AllowVoiceLogin loginToken=%s \n joinToken=%s \n "), *Parameters.LoginToken, *Parameters.JoinToken);

	VivoxAccountId = AccountId(Endpoint.Issuer, Parameters.Username, Endpoint.Domain);
	CurrentChannel = ChannelId(Endpoint.Issuer, Parameters.Channel, Endpoint.Domain, ChannelType::NonPositional);

	if (LoginSession == nullptr)
	{
		LoginSession = &(VoiceClient->GetLoginSession(VivoxAccountId));
	}

	if (LoginSession->State() != LoginState::LoggedIn)
	{
		OnBeginLoginCompleted.BindLambda([this, Parameters](VivoxCoreError result)
			{
				if (VxErrorSuccess == result)
				{
					UE_LOG(ModumateVoice, Log, TEXT("Connected to Vivox\n"));
					JoinChannel(Parameters.JoinToken);
				}
				else
				{
					UE_LOG(ModumateVoice, Warning, TEXT("Failed to connect to Vivox: %d\n"), result);
				}
			});

		// Request the user to login to Vivox
		LoginSession->EventStateChanged.AddUObject(this, &AModumateVoice::OnLoginSessionStateChanged);
		auto vivoxReturn = LoginSession->BeginLogin(Endpoint.Server, Parameters.LoginToken, OnBeginLoginCompleted);
		if (vivoxReturn != VxErrorSuccess)
		{
			UE_LOG(ModumateVoice, Error, TEXT("Failed to log in to voice chat: %d"), vivoxReturn);
		}
	}
	else
	{
		UE_LOG(ModumateVoice, Log, TEXT("Already Logged In! Joining Channel %s"), *Parameters.Channel);
		JoinChannel(Parameters.JoinToken);
	}
#endif
}

FString AModumateVoice::GenerateToken(const FVivoxEndpoint& Endpoint, const FVivoxParam& Parameters,
										const FString& Request, const FString& Key) const
{
	//Every token must be unique. Expiry time takes care of most of this, but vivox
	// provides a 'vxi' field to help guarantee this. Incrementing a static is their 
	// recommended pattern.
	static unsigned int vxi = 1;
	FDateTime expiration = FDateTime::UtcNow() + FTimespan::FromSeconds(90);

	TSharedPtr<FJsonObject> payloadJson = MakeShareable(new FJsonObject);

	payloadJson->SetStringField("iss", Endpoint.Issuer);
	payloadJson->SetStringField("vxa", Request);
	payloadJson->SetStringField("f", GenerateAccountURI(Endpoint, Parameters.Username));
	payloadJson->SetNumberField("exp", expiration.ToUnixTimestamp());
	if (Request.Equals("join"))
	{
		payloadJson->SetStringField("t", GenerateChannelURI(Endpoint, Parameters.Channel));
	}
	payloadJson->SetNumberField("vxi", vxi++);

	FString payload;
	TSharedRef<TJsonWriter<>> writer = TJsonWriterFactory<>::Create(&payload);
	FJsonSerializer::Serialize(payloadJson.ToSharedRef(), writer);

	UE_LOG(ModumateVoice, Log, TEXT("Generated JSON Key=%s"), *payload);

	FString header64 = FBase64::Encode(TEXT("{}"));
	FString payload64 = FBase64::Encode(payload);

	ToBase64URL(header64);
	ToBase64URL(payload64);

	FString toSign = header64 + TEXT(".") + payload64;
	FString sig = HMACSHA256(toSign, Key);

	ToBase64URL(sig);

	FString token = toSign + "." + sig;

	UE_LOG(ModumateVoice, Log, TEXT("TOKEN=%s"), *token);
	return token;
}

//This converts the Base64 to the URL-safe varient so
// the requests to Vivox do not choke.
void AModumateVoice::ToBase64URL(FString& Base64) const
{
	//Remove Trailing ='s
	while (Base64.RemoveFromEnd("="));

	//Change + to -
	Base64 = Base64.Replace(TEXT("+"), TEXT("-"));

	//Change / to _
	Base64 = Base64.Replace(TEXT("/"), TEXT("_"));
}

FString AModumateVoice::GenerateAccountURI(const FVivoxEndpoint& Endpoint, const FString& Username)
{
	return TEXT("sip:." + Endpoint.Issuer + "." + Username + ".@" + Endpoint.Domain);
}

FString AModumateVoice::GenerateChannelURI(const FVivoxEndpoint& Endpoint, const FString& Channel)
{
	return TEXT("sip:confctl-g-" + Endpoint.Issuer + "." + Channel + "@" + Endpoint.Domain);
}

FString AModumateVoice::HMACSHA256(const FString& Msg, const FString& Key) const
{
	auto cmsg = StringCast<ANSICHAR>(*Msg);
	auto ckey = StringCast<ANSICHAR>(*Key);

	size_t msg_length = strlen(cmsg.Get());
	size_t key_length = strlen(ckey.Get());

	picohash_ctx_t ctx;
	uint8 digest[PICOHASH_SHA256_DIGEST_LENGTH];
	picohash_init_hmac(&ctx, picohash_init_sha256, ckey.Get(), key_length);
	picohash_update(&ctx, cmsg.Get(), msg_length);
	picohash_final(&ctx, digest);

	FString result = FBase64::Encode(digest, PICOHASH_SHA256_DIGEST_LENGTH);
	UE_LOG(ModumateVoice, Log, TEXT("HMAC=%s"), *result);
	return result;
}

#if !UE_SERVER
void AModumateVoice::SetAudioDevices(const IAudioDevice& InputToSet, const IAudioDevice& OutputToSet)
{
	if (VoiceClient->AudioInputDevices().ActiveDevice().Id() != InputToSet.Id())
	{
		VoiceClient->AudioInputDevices().SetActiveDevice(InputToSet);
	}
	if (VoiceClient->AudioOutputDevices().ActiveDevice().Id() != OutputToSet.Id())
	{
		VoiceClient->AudioOutputDevices().SetActiveDevice(OutputToSet);
	}
}

void AModumateVoice::OnDeviceAdded(const IAudioDevice& Device)
{
	UE_LOG(ModumateVoice, Log, TEXT("Audio device added: %s"), *(Device.Name()));
	VoiceDeviceChangedEvent.Broadcast();
}

void AModumateVoice::OnDeviceRemoved(const IAudioDevice& Device)
{
	//If the device to be removed is the currently active device
	// we should change to the system defaults
	UE_LOG(ModumateVoice, Log, TEXT("Audio device removed: %s"), *(Device.Name()));

	if (VoiceClient->AudioInputDevices().ActiveDevice().Id() == Device.Id())
	{
		VoiceClient->AudioInputDevices().SetActiveDevice(VoiceClient->AudioInputDevices().SystemDevice());
	}

	if (VoiceClient->AudioOutputDevices().ActiveDevice().Id() == Device.Id())
	{
		VoiceClient->AudioOutputDevices().SetActiveDevice(VoiceClient->AudioOutputDevices().SystemDevice());
	}
	VoiceDeviceChangedEvent.Broadcast();
}

void AModumateVoice::OnDeviceChanged(const IAudioDevice& Device)
{
	UE_LOG(ModumateVoice, Log, TEXT("Audio device changed to: %s"), *(Device.Name()));
	VoiceDeviceChangedEvent.Broadcast();
}

void AModumateVoice::OnParticipantUpdated(const IParticipant& Participant)
{
	TalkingChangedEvent.Broadcast(Participant.Account().Name(), Participant.SpeechDetected());
}

void AModumateVoice::OnParticipantAdded(const IParticipant& Participant)
{
	ParticipantJoinEvent.Broadcast(Participant.Account().Name());
}

void AModumateVoice::OnParticipantRemoved(const IParticipant& Participant)
{
	ParticipantLeaveEvent.Broadcast(Participant.Account().Name());
}

void AModumateVoice::OnLoginSessionStateChanged(LoginState State)
{
	switch (State)
	{
	case LoginState::LoggingOut:
		UE_LOG(ModumateVoice, Error, TEXT("LoginSession Logging out\n"));
		break;
	case LoginState::LoggedOut:
		UE_LOG(ModumateVoice, Log, TEXT("LoginSession Logged Out Unexpectedly\n"));
		break;
	case LoginState::LoggedIn:
		UE_LOG(ModumateVoice, Log, TEXT("LoginSession Logged In\n"));
		break;
	case LoginState::LoggingIn:
		UE_LOG(ModumateVoice, Log, TEXT("LoginSession Logging In\n"));
		break;
	default:
		UE_LOG(ModumateVoice, Log, TEXT("LoginSession Unknown State\n"));
		break;
	}
}

void AModumateVoice::OnChannelSessionConnectionStateChanged(const IChannelConnectionState& State)
{
	FString channelName(State.ChannelSession().Channel().Name());

	if (ConnectionState::Connected == State.State())
	{
		UE_LOG(ModumateVoice, Log, TEXT("Channel %s connected\n"), *channelName);
		bIsConnectedToChannel = true;
		ChannelSession->EventAfterParticipantUpdated.AddUObject(this, &AModumateVoice::OnParticipantUpdated);
	}
	else if (ConnectionState::Disconnected == State.State())
	{
		UE_LOG(ModumateVoice, Warning, TEXT("Channel %s disconnected\n"), *channelName);
		bIsConnectedToChannel = false;
	}
}
#endif